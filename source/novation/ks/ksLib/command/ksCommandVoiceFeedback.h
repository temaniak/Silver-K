#pragma once

#include "ksCommandFeedback.h"
#include "../../../controller/command/commandSession.h"

namespace novation::controller::command {
enum class KSFeedbackProgress:uint8_t { Ready,WaitingForReply,Complete,Fault };

// A complete observation of one owned physical voice. Its only continuation
// is a correlated DSP status/age reply; the model changes once after every
// required reply. Countdown is returned to the musical voice owner.
class KSCommandVoiceFeedback final {
public:
 KSCommandVoiceFeedback()=default;
 ~KSCommandVoiceFeedback(){if(accepted_&&!complete_&&!fault_)fail();}
 KSCommandVoiceFeedback(const KSCommandVoiceFeedback&)=delete;
 KSCommandVoiceFeedback& operator=(const KSCommandVoiceFeedback&)=delete;
 bool prepare(CommandSession& session,KSMusicalState& state,VoiceAllocation allocation,KSVoiceFeedbackState before)noexcept {
  if(state_ || session.snapshot().session!=state.session() || session.snapshot().lifecycle!=Lifecycle::Running ||
     state.voiceOperationBusy())return false;
  const auto voice=state.voice(allocation);if(!voice || voice->feedbackAge!=before.age || !before.age ||
     voice->observationRevision==std::numeric_limits<uint64_t>::max())return false;
  const auto dependency=state.dependency(voice->part,session.rateGeneration());if(!dependency)return false;
  state_=&state;session_=&session;before_=before;allocation_=allocation;observation_=voice->observationRevision;
  dependency_=*dependency;operationRevision_=state.voiceOperationRevision();return true;
 }
 DspAdmission admit(uint64_t operation,NativeFrame at)noexcept {
  if(!live())return reject(DspAdmission::StaleSession);
  if(ticket_ || complete_)return DspAdmission::StaleOperation;
  if(!current())return reject(DspAdmission::InvalidDraft);
  const auto draft=haveStatus_?KSCommandFeedback::voiceAgeRead(allocation_.slot,at):
                                KSCommandFeedback::voiceStatusRead(allocation_.slot,at);
  DspTicket ticket;const auto result=session_->transactions().submit(state_->session(),operation,&draft,1,&ticket,[&]()noexcept{accepted_=true;});
  if(result==DspAdmission::Accepted)ticket_=ticket;
  else if(result!=DspAdmission::Full)return reject(result);
  return result;
 }
 KSFeedbackProgress accept(DspReply reply,NativeFrame at)noexcept {
  if(!live() || !current() || !ticket_ || reply.endpoint!=Endpoint::B || reply.transaction!=ticket_->sequence ||
     reply.word>0xffffff || !state_->canPublish(at) || session_->transactions().poisoned(Endpoint::B))return fail();
  DspTransactionProgress progress;
  const auto expected=haveStatus_?uint16_t(0x808f+22*size_t(allocation_.slot)):KSCommandFeedback::voiceStatusAddress(allocation_.slot);
  if(!session_->transactions().progress(*ticket_,progress) || progress.status!=DspStatus::Observed ||
     progress.address.mode!=DspAddressMode::Read || progress.address.address!=expected ||
     !session_->transactions().retire(*ticket_))return fail();
  ticket_.reset();
  if(!haveStatus_) {
   status_=reply.word;
   if(const auto pending=KSCommandFeedback::pendingResult(before_,status_))return commit(*pending,at);
   haveStatus_=true;return KSFeedbackProgress::Ready;
  }
  const auto result=KSCommandFeedback::voiceResult(before_,status_,reply.word);
  return result?commit(*result,at):fail();
 }
 KSFeedbackProgress progress()const noexcept {
  if(fault_)return KSFeedbackProgress::Fault;
  return complete_?KSFeedbackProgress::Complete:ticket_?KSFeedbackProgress::WaitingForReply:KSFeedbackProgress::Ready;
 }
 std::optional<KSVoiceFeedbackState> result()const noexcept{return complete_?std::optional{result_}:std::nullopt;}
 std::optional<DspTicket> ticket()const noexcept{return ticket_;}
 KSFeedbackProgress abort()noexcept{return fail();}
 bool reset()noexcept {
  if(accepted_&&!complete_&&!fault_)return false;
  state_=nullptr;session_=nullptr;ticket_.reset();accepted_=complete_=fault_=haveStatus_=false;return true;
 }
private:
 bool live()const noexcept{return state_&&!fault_&&session_->snapshot().session==state_->session()&&
  session_->snapshot().lifecycle==Lifecycle::Running&&session_->rateGeneration()==dependency_.rateGeneration;}
 bool current()const noexcept {
  const auto dependency=state_->dependency(dependency_.part,dependency_.rateGeneration);const auto voice=state_->voice(allocation_);
  return dependency && *dependency==dependency_ && voice && voice->feedbackAge==before_.age &&
   voice->observationRevision==observation_ && !state_->voiceOperationBusy() &&
   state_->voiceOperationRevision()==operationRevision_;
 }
 KSFeedbackProgress commit(KSVoiceFeedbackState result,NativeFrame at)noexcept {
  if(!current() || !state_->commitVoiceFeedback(allocation_,result.age,at))return fail();
  result_=result;complete_=true;return KSFeedbackProgress::Complete;
 }
 DspAdmission reject(DspAdmission result)noexcept{if(accepted_)fail();return result;}
 KSFeedbackProgress fail()noexcept {
  if(state_&&accepted_&&!complete_&&session_->snapshot().session==state_->session())session_->stop(state_->session());
  fault_=true;return KSFeedbackProgress::Fault;
 }
 KSMusicalState* state_=nullptr;CommandSession* session_=nullptr;VoiceAllocation allocation_{};
 ParameterDependency<KSStorageSlot> dependency_{};uint64_t observation_=0,operationRevision_=0;
 KSVoiceFeedbackState before_{},result_{};std::optional<DspTicket> ticket_;uint32_t status_=0;
 bool accepted_=false,complete_=false,fault_=false,haveStatus_=false;
};
}
