#include "kCommandModWheelUpdate.h"
#include <exception>

namespace novation::controller::command {
KCommandModWheelUpdate::~KCommandModWheelUpdate() {if(accepted_&&!complete_&&!fault_)fail();}

bool KCommandModWheelUpdate::prepare(CommandSession& session,KMusicalState& model,
    const KExpressionPacket& packet,uint8_t newWheel)noexcept {
    if(model_ || fault_ || session.model()!=Model::K || session.snapshot().session!=model.session() ||
       session.snapshot().lifecycle!=Lifecycle::Running || !model.voiceOperationAvailable() ||
       packet.session()!=model.session() || newWheel>=128 || packet.source().controls[0]!=newWheel ||
       packet.size()<3 || packet.size()>KExpressionPacket::Capacity)return false;
    kCommandDetail::WriteSource captured;if(!captured.capture(model,session.rateGeneration()))return false;
    std::array<bool,KMusicalState::VoiceCount> included{};
    for(size_t i=0;i<packet.voiceCount();++i) {
        const auto* supplied=packet.voice(i);if(!supplied)return false;
        const auto index=size_t(supplied->allocation.slot);const auto voice=model.voice(supplied->allocation);
        if(index>=included.size() || included[index] || !voice || voice->part!=KPart::Main || voice->feedbackAge!=supplied->age)return false;
        included[index]=true;
    }
    for(size_t i=0;i<captured.voices.size();++i)
        if(captured.voices[i] && captured.voices[i]->feedbackAge && !included[i])return false;
    for(size_t i=0;i<packet.size();++i) {
        const auto* write=packet.at(i);if(!write || write->endpoint!=Endpoint::A || write->word>0xffffff)return false;
    }
    session_=&session;model_=&model;packet_=packet;source_=captured;wheel_=newWheel;
    operationRevision_=model.voiceOperationRevision();return true;
}

bool KCommandModWheelUpdate::live()const noexcept {
    return model_&&!fault_ && session_->snapshot().session==model_->session() &&
        session_->snapshot().lifecycle==Lifecycle::Running && session_->rateGeneration()==source_.dependency.rateGeneration;
}
bool KCommandModWheelUpdate::current()const noexcept {
    return source_.current(*model_,source_.dependency.rateGeneration) &&
        model_->voiceOperationRevision()==operationRevision_+unsigned(accepted_) &&
        (accepted_?model_->ownsVoiceOperation(this):model_->voiceOperationAvailable());
}
DspAdmission KCommandModWheelUpdate::admit(uint64_t operation,NativeFrame at)noexcept {
    if(!live())return reject(DspAdmission::StaleSession);
    if(complete_ || ticket_)return DspAdmission::StaleOperation;
    if(!current() || (!accepted_&&!model_->canCommitModWheel(wheel_,at)))return reject(DspAdmission::InvalidDraft);
    if(session_->transactions().poisoned(Endpoint::A))return reject(DspAdmission::Poisoned);
    const auto* write=packet_.at(next_);if(!write)return reject(DspAdmission::InvalidDraft);
    DspTransactionDraft draft;draft.model=Model::K;draft.endpoint=Endpoint::A;draft.earliest=at;
    draft.address={DspAddressMode::Write,write->address};draft.words=&write->word;draft.wordCount=1;
    DspTicket ticket;
    const auto result=session_->transactions().submit(model_->session(),operation,&draft,1,&ticket,[&]()noexcept {
        if(!accepted_) {
            if(!model_->claimVoiceOperation(this) || !model_->commitModWheel(wheel_,at))std::terminate();
            accepted_=true;
            if(!source_.capture(*model_,source_.dependency.rateGeneration))std::terminate();
        }
    });
    if(result==DspAdmission::Accepted)ticket_=ticket;
    else if(result!=DspAdmission::Full)return reject(result);
    return result;
}
KWriteProgress KCommandModWheelUpdate::progress()noexcept {
    if(complete_&&live())return KWriteProgress::Complete;
    if(!live() || !current() || session_->transactions().poisoned(Endpoint::A))return fail();
    if(!ticket_)return KWriteProgress::Ready;
    DspTransactionProgress observed;const auto* write=packet_.at(next_);
    if(!write || !session_->transactions().progress(*ticket_,observed))return fail();
    if(observed.status==DspStatus::Queued)return KWriteProgress::WaitingForTransfer;
    if(observed.status!=DspStatus::Transferred || observed.model!=Model::K ||
       observed.address.mode!=DspAddressMode::Write || observed.address.address!=write->address ||
       observed.wordCount!=2 || !session_->transactions().retire(*ticket_))return fail();
    ticket_.reset();
    if(++next_==packet_.size()) {finish();return KWriteProgress::Complete;}
    return KWriteProgress::Ready;
}
void KCommandModWheelUpdate::finish()noexcept {
    if(!model_->releaseVoiceOperation(this))std::terminate();complete_=true;
}
DspAdmission KCommandModWheelUpdate::reject(DspAdmission result)noexcept {if(accepted_)fail();return result;}
KWriteProgress KCommandModWheelUpdate::fail()noexcept {
    if(model_ && accepted_ && !complete_ && session_->snapshot().session==model_->session())session_->stop(model_->session());
    if(model_&&model_->ownsVoiceOperation(this))model_->releaseVoiceOperation(this);
    fault_=true;return KWriteProgress::Fault;
}
bool KCommandModWheelUpdate::reset()noexcept {
    if(accepted_&&!complete_&&!fault_)return false;
    model_=nullptr;session_=nullptr;ticket_.reset();next_=0;accepted_=complete_=fault_=false;return true;
}
}
