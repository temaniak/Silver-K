#include "kCommandNoteOn.h"
#include <exception>

namespace novation::controller::command
{
    KCommandNoteOn::~KCommandNoteOn() { if(acquired_ && !complete_ && !fault_)fail(); }

    bool KCommandNoteOn::prepare(CommandSession& session,KMusicalState& model,
                                const KExpressionSource& expression,const KVoiceTunings& tunings,
                                uint8_t note,uint8_t velocity,KVoiceStart start) noexcept
    {
        const auto status=session.snapshot();
        if(model_ || session.model()!=Model::K || status.session!=model.session() || status.lifecycle!=Lifecycle::Running ||
           !model.voiceOperationAvailable() || !velocity || velocity>=128 || note>=128 ||
           !supportsKNote(expression.patch[KVoiceModeOffset],expression.patch[KUnisonOffset],start.unit) ||
           !captured_.capture(model,session.rateGeneration()))return false;
        std::array<KPolyVoiceState,KMusicalState::VoiceCount> states{};
        for(size_t i=0;i<states.size();++i)
            if(const auto& voice=captured_.voices[i])states[i]={voice->feedbackAge,captured_.countdown[i]};
        const auto mode=expression.patch[KVoiceModeOffset];
        selection_=selectKPolyVoice(states,model.activeVoiceLimit(),start.excludedSlots);
        if(isKMono(mode) || mode==uint8_t(KVoiceMode::PolyRetrigger)) {
            for(size_t i=0;i<model.activeVoiceLimit();++i) {
                const auto& voice=captured_.voices[i];
                if((start.excludedSlots&(1u<<i)) || !voice || !voice->feedbackAge ||
                   model.voiceUnit(VoiceSlot(i))!=start.unit || (!isKMono(mode) && voice->note!=note))continue;
                selection_={VoiceSlot(i),KPolyVoiceSelectionReason::SameNote};
                break;
            }
        }
        if(size_t(selection_.slot)>=model.activeVoiceLimit())return false;
        slotWord_=uint32_t(selection_.slot);
        expression_=expression;
        tunings_=tunings;
        start_=start;
        note_=note;
        velocity_=velocity;
        revision_=model.voiceOperationRevision();
        session_=&session;
        model_=&model;
        return true;
    }

    bool KCommandNoteOn::current() const noexcept
    {
        if(!model_ || fault_)return false;
        const auto status=session_->snapshot();
        return status.session==model_->session() && status.lifecycle==Lifecycle::Running &&
               captured_.current(*model_,session_->rateGeneration()) &&
               model_->voiceOperationRevision()==revision_+unsigned(acquired_) &&
               (acquired_?model_->ownsVoiceOperation(this):model_->voiceOperationAvailable()) &&
               !session_->transactions().poisoned(Endpoint::A);
    }

    DspAdmission KCommandNoteOn::admitPitch(uint64_t operation,NativeFrame at) noexcept
    {
        if(acquired_ || complete_)return DspAdmission::StaleOperation;
        if(!current() || !model_->canStartVoice(selection_.slot,note_,at))return rejected(DspAdmission::InvalidDraft);
        DspTransactionDraft draft;
        draft.model=Model::K;
        draft.endpoint=Endpoint::A;
        draft.address={DspAddressMode::Read,pitchAddress()};
        draft.earliest=at;
        draft.releaseEndpoint=DspMilestone::Observation;
        DspTicket ticket;
        const auto result=session_->transactions().submit(model_->session(),operation,&draft,1,&ticket,[&]() noexcept {
            if(!model_->claimVoiceOperation(this))std::terminate();
            acquired_=true;
        });
        if(result==DspAdmission::Accepted)pitch_=ticket;
        return result;
    }

    bool KCommandNoteOn::acceptPitch(DspReply reply,NativeFrame at) noexcept
    {
        DspTransactionProgress record;
        if(!current() || !pitch_ || reply.endpoint!=Endpoint::A || reply.transaction!=pitch_->sequence ||
           !session_->transactions().progress(*pitch_,record) || record.status!=DspStatus::Observed ||
           record.address.mode!=DspAddressMode::Read || record.address.address!=pitchAddress() ||
           !model_->canStartVoice(selection_.slot,note_,at))return fail();
        if(repeatsVoice()) {
            const auto repeat=compileKRepeatNoteUpdate(expression_,tunings_,note_,velocity_,reply.word,start_);
            if(!repeat)return fail();
            repeat_=*repeat;
        } else {
            const auto image=compileKPolyNoteImage(expression_,tunings_,selection_.slot,note_,velocity_,reply.word,start_);
            if(!image)return fail();
            image_=*image;
        }
        if(!session_->transactions().retire(*pitch_))return fail();
        pitch_.reset();
        pitchReady_=true;
        return true;
    }

    std::array<DspTransactionDraft,KCommandNoteOn::MaxWrites> KCommandNoteOn::writes(NativeFrame at) const noexcept
    {
        const auto create=[&](uint16_t address,const uint32_t* words,size_t count) {
            DspTransactionDraft draft;
            draft.model=Model::K;
            draft.endpoint=Endpoint::A;
            draft.address={DspAddressMode::Write,address};
            draft.earliest=at;
            draft.words=words;
            draft.wordCount=count;
            return draft;
        };
        const auto base=81*size_t(selection_.slot);
        const auto triggerWrite=[&](const uint32_t* word) {
            return create(KFeedbackProtocol::triggerAddress(selection_.slot),word,1);
        };
        if(repeatsVoice())
            return {{create(0x802d,&slotWord_,1),create(uint16_t(0x4f+base),&repeat_.velocityDepths[0],1),
                     create(uint16_t(0x52+base),&repeat_.velocityDepths[1],1),
                     create(uint16_t(0x58+base),&repeat_.velocityDepths[2],1),
                     create(uint16_t(0x67+base),repeat_.pitch.data(),repeat_.pitch.size()),triggerWrite(&repeat_.trigger)}};
        return {{create(0x802d,&slotWord_,1),create(uint16_t(0x1b+base),image_.words.data(),image_.words.size()),
                 create(0x8026,&image_.mode,1),create(0x8029,&image_.level,1),triggerWrite(&image_.trigger)}};
    }

    DspAdmission KCommandNoteOn::admitImage(uint64_t operation,NativeFrame at) noexcept
    {
        if(writing_ || complete_)return DspAdmission::StaleOperation;
        if(!current() || !pitchReady_ || !model_->canStartVoice(selection_.slot,note_,at))return rejected(DspAdmission::InvalidDraft);
        const auto batch=writes(at);
        const auto result=session_->transactions().submit(model_->session(),operation,batch.data(),writeCount(),tickets_.data(),[&]() noexcept {
            if(!model_->startVoice(selection_.slot,note_,repeatsVoice()?repeat_.velocity:image_.velocity,at,start_.unit) ||
               !captured_.capture(*model_,session_->rateGeneration()))std::terminate();
            writing_=true;
        });
        if(result!=DspAdmission::Accepted && result!=DspAdmission::Full)return rejected(result);
        return result;
    }

    std::optional<DspTicket> KCommandNoteOn::ticket() const noexcept
    {
        if(writing_ && next_<writeCount())return tickets_[next_];
        return pitch_;
    }

    KWriteProgress KCommandNoteOn::progress() noexcept
    {
        if(complete_)return KWriteProgress::Complete;
        DspTransactionProgress record;
        const auto next=ticket();
        if(!current() || !writing_ || !next || !session_->transactions().progress(*next,record)) {
            fail();
            return KWriteProgress::Fault;
        }
        if(record.status==DspStatus::Queued)return KWriteProgress::WaitingForTransfer;
        const auto expected=writes({})[next_];
        if(record.status!=DspStatus::Transferred || record.address.mode!=DspAddressMode::Write ||
           record.address.address!=expected.address.address || record.wordCount!=expected.wordCount+1 ||
           !session_->transactions().retire(*next)) {
            fail();
            return KWriteProgress::Fault;
        }
        if(++next_<writeCount())return KWriteProgress::Ready;
        if(!model_->releaseVoiceOperation(this))std::terminate();
        complete_=true;
        return KWriteProgress::Complete;
    }

    bool KCommandNoteOn::fail() noexcept
    {
        if(model_ && acquired_ && !complete_ && session_->snapshot().session==model_->session())session_->stop(model_->session());
        if(model_ && model_->ownsVoiceOperation(this))model_->releaseVoiceOperation(this);
        fault_=true;
        return false;
    }

    DspAdmission KCommandNoteOn::rejected(DspAdmission result) noexcept
    { if(acquired_)fail(); return result; }

    bool KCommandNoteOn::reset() noexcept
    {
        if(acquired_ && !complete_ && !fault_)return false;
        session_=nullptr;
        model_=nullptr;
        pitch_.reset();
        next_=0;
        acquired_=pitchReady_=writing_=fault_=complete_=false;
        return true;
    }
}
