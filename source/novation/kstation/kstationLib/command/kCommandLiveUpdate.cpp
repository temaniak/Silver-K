#include "kCommandLiveUpdate.h"
#include <exception>

namespace novation::controller::command {
KCommandLiveUpdate::~KCommandLiveUpdate(){if(owned_)fail();}

bool KCommandLiveUpdate::add(uint16_t address,uint32_t word) noexcept {
    if(count_==Capacity || word>0xffffffu)return false;
    writes_[count_++]={address,word};
    return true;
}

bool KCommandLiveUpdate::prepare(CommandSession& session,KMusicalState& model,const KExpressionSource& source,
    const KVoiceTunings& tuning,const KProgramTables& tables,KLiveEdit edit,NativeFrame at) noexcept {
    if(session_ || session.model()!=Model::K || session.snapshot().session!=model.session() ||
       session.snapshot().lifecycle!=Lifecycle::Running || !model.voiceOperationAvailable() ||
       session.transactions().poisoned(Endpoint::A) || !captured_.capture(model,session.rateGeneration()))return false;
    session_=&session;model_=&model;
    operationRevision_=model.voiceOperationRevision();
    after_=source;tuning_=tuning;parameters_=model.part().parameters;
    limit_=model.activeVoiceLimit();
    if(!compile(source,tuning,tables,edit) || !model.canCommitLiveParameters(parameters_,limit_,at)) {
        reset();
        return false;
    }
    return true;
}

bool KCommandLiveUpdate::silence(uint8_t mask) noexcept {
    retired_|=mask;
    for(size_t i=0;i<captured_.voices.size();++i) {
        const auto& voice=captured_.voices[i];
        if(!(mask&(1u<<i)) || !voice || !voice->feedbackAge)continue;
        const auto slot=VoiceSlot(i);
        if(model_->activationPending(voice->allocation) && !add(KFeedbackProtocol::triggerAddress(slot),0))return false;
        if(!add(uint16_t(0x55+81*i),0x100000) || !add(KFeedbackProtocol::gateAddress(slot),0))return false;
    }
    return true;
}

bool KCommandLiveUpdate::compile(const KExpressionSource& source,const KVoiceTunings& tuning,
                                 const KProgramTables& tables,KLiveEdit edit) noexcept {
    if(edit.kind!=KLiveKind::Pitch && edit.value>=128)return false;
    switch(edit.kind) {
    case KLiveKind::Volume:
        parameters_.volume=uint8_t(edit.value);
        return add(0x0013,uint32_t(edit.value)*edit.value*512u);
    case KLiveKind::Silence:
        resetKeys_=true;
        return silence(0xff);
    case KLiveKind::Pitch: {
        if(edit.value>=16384)return false;
        const uint16_t compressed=edit.value>>6;
        tuning_.pitchBend=compressed==255?512:uint16_t(compressed*2);
        parameters_.pitchBend=tuning_.pitchBend;
        for(size_t i=0;i<captured_.voices.size();++i) {
            if(!captured_.voices[i] || !captured_.voices[i]->feedbackAge)continue;
            const auto pitches=compileKOscillatorPitches(after_,tuning_,model_->voiceUnit(VoiceSlot(i)));
            for(size_t lane=0;lane<pitches.size();++lane)
                if(!add(uint16_t(0x1f+11*lane+81*i),pitches[lane]))return false;
        }
        return true;
    }
    case KLiveKind::Pressure:
    case KLiveKind::Breath: {
        if(edit.kind==KLiveKind::Pressure) {
            parameters_.pressure=uint8_t(uint32_t(edit.value)*edit.value*2u>>8);
            after_.controls[1]=parameters_.pressure;
        } else {
            parameters_.breath=uint8_t(edit.value);
            after_.controls[2]=parameters_.breath;
        }
        std::array<KExpressionVoice,KMusicalState::VoiceCount> voices{};
        size_t count=0;
        for(const auto& voice:captured_.voices)if(voice)voices[count++]={voice->allocation,voice->feedbackAge};
        const auto packet=compileKModWheelPacket(model_->session(),after_,voices.data(),count);
        if(!packet)return false;
        for(size_t i=0;i<packet->size();++i)if(!add(packet->at(i)->address,packet->at(i)->word))return false;
        return true;
    }
    case KLiveKind::Rebuild:
        return add(0x802b,0) && compileGlobals(source,tuning,tables,true);
    case KLiveKind::Patch:
        after_=editKProgram(source,{edit.offset,uint8_t(edit.value)});
        parameters_.cutoff=after_.patch[KCutoffPatchOffset];
        if(edit.offset==KCutoffPatchOffset && tables.clock && limit_==kProgramVoiceCount(after_)) {
            // A decoded patch may contain derived fields. Use the cutoff path
            // only when the entire decoded change is confined to cutoff.
            auto expected=source.patch;
            expected[KCutoffPatchOffset]=parameters_.cutoff;
            if(expected==after_.patch)return compileCutoffDeltas(source,tuning);
        }
        if(source.patch[KVoiceModeOffset]!=after_.patch[KVoiceModeOffset] ||
           source.patch[KUnisonOffset]!=after_.patch[KUnisonOffset]) {
            resetKeys_=true;
            if(!silence(0xff))return false;
        }
        if(!compileGlobals(source,tuning,tables,false))return false;
        return compileVoiceDeltas(source,tuning);
    }
    return false;
}

bool KCommandLiveUpdate::compileCutoffDeltas(const KExpressionSource& source,const KVoiceTunings& tuning) noexcept {
    if(source.patch[KCutoffPatchOffset]==parameters_.cutoff)return true;
    const auto word=kFilterCutoffWord(parameters_.cutoff,tuning.filterPitch);
    for(size_t i=0;i<captured_.voices.size();++i) {
        const auto& voice=captured_.voices[i];
        if(!voice || !voice->feedbackAge)continue;
        const auto slot=VoiceSlot(i);
        const uint32_t target=uint32_t(uint8_t(voice->note+tuning.partTranspose-60))<<16;
        const KVoiceStart params{model_->voiceUnit(slot),0,false,true};
        if(!validKPolyNoteInput(source,tuning,slot,voice->note,voice->velocity,target,params) ||
           !add(uint16_t(0x1b+81*i+KPolyNoteImage::CutoffWord),word))return false;
    }
    return true;
}

bool KCommandLiveUpdate::compileVoiceDeltas(const KExpressionSource& source,const KVoiceTunings& before) noexcept {
    if(source.patch==after_.patch)return true;
    for(size_t i=0;i<captured_.voices.size();++i) {
        const auto& voice=captured_.voices[i];
        if((retired_&(1u<<i)) || !voice || !voice->feedbackAge)continue;
        const auto slot=VoiceSlot(i);
        const uint32_t target=uint32_t(uint8_t(voice->note+before.partTranspose-60))<<16;
        const KVoiceStart params{model_->voiceUnit(slot),0,false,true};
        const auto old=compileKPolyNoteImage(source,before,slot,voice->note,voice->velocity,target,params);
        const auto next=compileKPolyNoteImage(after_,tuning_,slot,voice->note,voice->velocity,target,params);
        if(!old || !next)return false;
        const unsigned base=81*unsigned(i);
        for(size_t j=0;j<next->words.size();++j)
            if(next->words[j]!=old->words[j] && !add(uint16_t(0x1b+base+j),next->words[j]))return false;
        if(old->mode!=next->mode && !add(0x8026,next->mode))return false;
        if(old->level!=next->level && !add(0x8029,next->level))return false;
    }
    return true;
}

bool KCommandLiveUpdate::compileGlobals(const KExpressionSource& source,const KVoiceTunings& tuning,
                                        const KProgramTables& tables,bool all) noexcept {
    if(!tables.clock)return false;
    const auto before=compileKProgramGlobals(source,tuning,tables);
    const auto next=compileKProgramGlobals(after_,tuning_,tables);
    const auto write=[&](uint16_t address,uint32_t old,uint32_t value) {
        return (!all && old==value) || add(address,value);
    };
    if(next.voiceCount!=limit_) {
        if(next.voiceCount<limit_ && !silence(uint8_t(0xffu<<next.voiceCount)))return false;
        limit_=next.voiceCount;
        if(!add(0x0002,next.voiceCount))return false;
    }
    for(size_t i=0;i<next.fx.size();++i)
        if(!write(uint16_t(KProgramGlobals::FirstFxAddress+i),before.fx[i],next.fx[i]))return false;
    return write(0x8039,before.routing[0],next.routing[0]) && write(0x803b,before.routing[1],next.routing[1]) &&
           write(5,before.pan[0],next.pan[0]) && write(6,before.pan[1],next.pan[1]) &&
           write(0x8034,before.unison,next.unison) && write(0x8030,before.split,next.split) &&
           write(0x8031,before.splitAux,next.splitAux) && write(0x8033,before.level,next.level) &&
           write(0x802c,source.patch[0xab],after_.patch[0xab]);
}

bool KCommandLiveUpdate::current() const noexcept {
    return session_ && session_->snapshot().session==model_->session() &&
        session_->snapshot().lifecycle==Lifecycle::Running &&
        model_->voiceOperationRevision()==operationRevision_+unsigned(owned_) &&
        (owned_?model_->ownsVoiceOperation(this):model_->voiceOperationAvailable()) &&
        captured_.current(*model_,session_->rateGeneration()) && !session_->transactions().poisoned(Endpoint::A);
}

DspAdmission KCommandLiveUpdate::submit(uint64_t operation,NativeFrame at) noexcept {
    if(!current() || ticket_ || !count_ || completed_)return DspAdmission::InvalidDraft;
    const auto fence=next_==count_;
    auto draft=KFeedbackProtocol::mailboxRead(at);
    std::array<uint32_t,DspTransactionQueue::MaxWords-1> words;
    size_t count=0;
    if(!fence) {
        const auto address=writes_[next_].address;
        while(next_+count<count_ && count<words.size() && writes_[next_+count].address==address+count) {
            words[count]=writes_[next_+count].word;
            ++count;
        }
        draft.address={DspAddressMode::Write,address};
        draft.words=words.data();draft.wordCount=count;draft.releaseEndpoint=DspMilestone::Transfer;
    }
    DspTicket ticket;
    const auto status=session_->transactions().submit(model_->session(),operation,&draft,1,&ticket,[&]()noexcept {
        if(!owned_ && !model_->claimVoiceOperation(this))std::terminate();
        owned_=true;
    });
    if(status==DspAdmission::Accepted) {
        ticket_=ticket;
        pendingWords_=count;
    } else if(status!=DspAdmission::Full && owned_)fail();
    return status;
}

KWriteProgress KCommandLiveUpdate::transferred() noexcept {
    DspTransactionProgress state;
    if(!current() || !owned_ || !ticket_ || !pendingWords_ ||
       !session_->transactions().progress(*ticket_,state) || state.status!=DspStatus::Transferred ||
       state.address.mode!=DspAddressMode::Write || state.address.address!=writes_[next_].address ||
       state.wordCount!=pendingWords_+1 || !session_->transactions().retire(*ticket_)) {
        fail();return KWriteProgress::Fault;
    }
    next_+=pendingWords_;
    pendingWords_=0;ticket_.reset();
    return KWriteProgress::Ready;
}

bool KCommandLiveUpdate::finish(std::optional<DspReply> reply,NativeFrame at,KExpressionSource& source,
                               KVoiceTunings& tuning) noexcept {
    if(completed_ || !current() || next_!=count_ || !model_->canCommitLiveParameters(parameters_,limit_,at))return fail();
    if(count_) {
        DspTransactionProgress observed;
        if(!owned_ || !reply || !ticket_ || reply->endpoint!=Endpoint::A || reply->transaction!=ticket_->sequence ||
           reply->word!=0 || !session_->transactions().progress(*ticket_,observed) ||
           observed.status!=DspStatus::Observed || observed.address.mode!=DspAddressMode::Read ||
           observed.address.address!=0x802b || !session_->transactions().retire(*ticket_))return fail();
    } else if(reply || ticket_ || owned_)return fail();
    if(!model_->commitLiveParameters(parameters_,limit_,retired_,at))return fail();
    source=after_;tuning=tuning_;
    completed_=true;
    if(owned_ && !model_->releaseVoiceOperation(this))std::terminate();
    owned_=false;
    return true;
}

bool KCommandLiveUpdate::fail() noexcept {
    if(owned_) {
        session_->stop(model_->session());
        model_->releaseVoiceOperation(this);
        owned_=false;
    }
    return false;
}

bool KCommandLiveUpdate::reset() noexcept {
    if(owned_)return false;
    session_=nullptr;model_=nullptr;ticket_.reset();
    next_=count_=pendingWords_=0;retired_=0;completed_=resetKeys_=false;
    return true;
}
}
