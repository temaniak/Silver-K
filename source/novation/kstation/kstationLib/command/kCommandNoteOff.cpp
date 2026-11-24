#include "kCommandNoteOff.h"
#include <algorithm>
#include <exception>

namespace novation::controller::command {
KCommandNoteOff::~KCommandNoteOff() {if(accepted_&&!complete_&&!fault_)fail();}
bool KCommandNoteOff::prepare(CommandSession& session,KMusicalState& model,uint8_t note,bool sustainBypass,
                              uint8_t excludedSlots)noexcept {
    return note<128 && prepareFor(session,model,note,std::nullopt,sustainBypass,false,nullptr,0,excludedSlots);
}
bool KCommandNoteOff::preparePedal(CommandSession& session,KMusicalState& model,uint8_t value,bool sustainBypass)noexcept {
    return value<128 && prepareFor(session,model,std::nullopt,value>=64,sustainBypass);
}
bool KCommandNoteOff::preparePending(CommandSession& session,KMusicalState& model)noexcept {
    return prepareFor(session,model,std::nullopt,std::nullopt,false);
}
bool KCommandNoteOff::prepareAll(CommandSession& session,KMusicalState& model)noexcept {
    return prepareFor(session,model,std::nullopt,std::nullopt,true,true);
}
bool KCommandNoteOff::prepareOwned(CommandSession& session,KMusicalState& model,
                                   const VoiceAllocation* owned,size_t count)noexcept {
    return owned && count<=KMusicalState::VoiceCount &&
           prepareFor(session,model,std::nullopt,std::nullopt,true,false,owned,count);
}
bool KCommandNoteOff::prepareFor(CommandSession& session,KMusicalState& model,std::optional<uint8_t> note,
                                std::optional<bool> pedal,bool sustainBypass,bool all,
                                const VoiceAllocation* owned,size_t ownedCount,uint8_t excludedSlots)noexcept {
    if(model_ || fault_ || session.model()!=Model::K || session.snapshot().session!=model.session() ||
       session.snapshot().lifecycle!=Lifecycle::Running || !model.voiceOperationAvailable())return false;
    kCommandDetail::WriteSource captured;if(!captured.capture(model,session.rateGeneration()))return false;
    const bool releasePedal=pedal && !*pedal && model.sustainDown() && !sustainBypass;
    size_t count=0;
    for(const auto& voice:captured.voices) {
        if(!voice || !voice->feedbackAge)continue;
        const auto allocation=voice->allocation;
        if(excludedSlots & (1u<<size_t(allocation.slot)))continue;
        const bool selected=owned?std::find(owned,owned+ownedCount,allocation)!=owned+ownedCount:all || (note?voice->note==*note:
            pedal?releasePedal && model.deferredRelease(allocation):
            model.releasePending(allocation) && !model.activationPending(allocation));
        if(selected)allocations_[count++]=allocation;
    }
    if(!owned && !note && !pedal && !all && !count)return false;
    defer_=note && model.sustainDown() && !sustainBypass;
    pedal_=pedal;
    session_=&session;model_=&model;source_=captured;count_=count;
    selectGates();
    operationRevision_=model.voiceOperationRevision();return true;
}
void KCommandNoteOff::selectGates()noexcept {
    gateCount_=0;
    if(!defer_)for(size_t i=0;i<count_;++i)
        if(!model_->activationPending(allocations_[i]))gates_[gateCount_++]=allocations_[i];
}
bool KCommandNoteOff::live()const noexcept {
    return model_&&!fault_ && session_->snapshot().session==model_->session() &&
        session_->snapshot().lifecycle==Lifecycle::Running && session_->rateGeneration()==source_.dependency.rateGeneration;
}
bool KCommandNoteOff::current()const noexcept {
    return source_.current(*model_,source_.dependency.rateGeneration) &&
        model_->voiceOperationRevision()==operationRevision_+unsigned(accepted_) &&
        (accepted_?model_->ownsVoiceOperation(this):model_->voiceOperationAvailable());
}
DspAdmission KCommandNoteOff::admit(uint64_t operation,NativeFrame at)noexcept {
    if(!live())return reject(DspAdmission::StaleSession);
    if(accepted_ || complete_)return DspAdmission::StaleOperation;
    if(!operation)return DspAdmission::StaleOperation;
    if(!current() || !model_->canCommitRelease(allocations_.data(),count_,at))return reject(DspAdmission::InvalidDraft);
    if(session_->transactions().poisoned(Endpoint::A))return reject(DspAdmission::Poisoned);
    selectGates();
    const auto commit=[&]()noexcept {
        if(!model_->claimVoiceOperation(this) || !model_->commitRelease(allocations_.data(),count_,at,defer_,pedal_))std::terminate();
        accepted_=true;if(!source_.capture(*model_,source_.dependency.rateGeneration))std::terminate();
    };
    if(!size()) {commit();finish();return DspAdmission::Accepted;}
    const uint32_t off=0;std::array<DspTransactionDraft,KMusicalState::VoiceCount> drafts{};
    for(size_t i=0;i<gateCount_;++i) {
        drafts[i].model=Model::K;drafts[i].endpoint=Endpoint::A;drafts[i].earliest=at;
        drafts[i].address={DspAddressMode::Write,KFeedbackProtocol::gateAddress(gates_[i].slot)};
        drafts[i].words=&off;drafts[i].wordCount=1;
    }
    const auto result=session_->transactions().submit(model_->session(),operation,drafts.data(),gateCount_,tickets_.data(),commit);
    if(result!=DspAdmission::Accepted && result!=DspAdmission::Full)return reject(result);
    return result;
}
KWriteProgress KCommandNoteOff::progress()noexcept {
    if(complete_&&live())return KWriteProgress::Complete;
    if(!live() || !current() || session_->transactions().poisoned(Endpoint::A))return fail();
    if(!accepted_)return KWriteProgress::Ready;
    const auto next=ticket();DspTransactionProgress observed;
    if(!next || !session_->transactions().progress(*next,observed))return fail();
    if(observed.status==DspStatus::Queued)return KWriteProgress::WaitingForTransfer;
    if(observed.status!=DspStatus::Transferred || observed.model!=Model::K || observed.wordCount!=2 ||
       observed.address.mode!=DspAddressMode::Write || observed.address.address!=KFeedbackProtocol::gateAddress(gates_[next_].slot) ||
       !session_->transactions().retire(*next))return fail();
    if(++next_==gateCount_) {finish();return KWriteProgress::Complete;}
    return KWriteProgress::Ready;
}
void KCommandNoteOff::finish()noexcept {
    if(!model_->releaseVoiceOperation(this))std::terminate();complete_=true;
}
DspAdmission KCommandNoteOff::reject(DspAdmission result)noexcept {if(accepted_)fail();return result;}
KWriteProgress KCommandNoteOff::fail()noexcept {
    if(model_ && accepted_ && !complete_ && session_->snapshot().session==model_->session())session_->stop(model_->session());
    if(model_&&model_->ownsVoiceOperation(this))model_->releaseVoiceOperation(this);
    fault_=true;return KWriteProgress::Fault;
}
bool KCommandNoteOff::reset()noexcept {
    if(accepted_&&!complete_&&!fault_)return false;
    model_=nullptr;session_=nullptr;count_=gateCount_=next_=0;pedal_.reset();defer_=false;
    accepted_=complete_=fault_=false;return true;
}
}
