#pragma once
#include "ksGlobalState.h"
#include "../../../controller/command/commandSession.h"

namespace novation::controller::command {
// Correlated read of one physical part mailbox. A rebuild request becomes
// owned pending musical work even if all desired global values are unchanged.
class KSCommandPartFeedback final {
public:
 KSCommandPartFeedback()=default;
 ~KSCommandPartFeedback(){if(ticket_&&!complete_&&!fault_)fail();}
 KSCommandPartFeedback(const KSCommandPartFeedback&)=delete;
 KSCommandPartFeedback& operator=(const KSCommandPartFeedback&)=delete;
 bool prepare(CommandSession& session,KSGlobalState& state,KSPart mailbox)noexcept {
  if(state_ || size_t(mailbox)>=4 || session.snapshot().session!=state.session() ||
     session.snapshot().lifecycle!=Lifecycle::Running)return false;
  state_=&state;session_=&session;mailbox_=mailbox;layoutRevision_=state.layoutRevision();rate_=session.rateGeneration();return true;
 }
 DspAdmission admit(uint64_t operation,NativeFrame at)noexcept {
  if(!live())return DspAdmission::StaleSession;
  if(ticket_||complete_)return DspAdmission::StaleOperation;
  if(state_->layoutRevision()!=layoutRevision_)return DspAdmission::InvalidDraft;
  const auto draft=*KSCommandFeedback::partRead(mailbox_,at);DspTicket ticket;
  const auto result=session_->transactions().submit(state_->session(),operation,&draft,1,&ticket,[]()noexcept{});
  if(result==DspAdmission::Accepted)ticket_=ticket;return result;
 }
 bool accept(DspReply reply)noexcept {
  if(!live() || state_->layoutRevision()!=layoutRevision_ || !ticket_ || reply.endpoint!=Endpoint::A ||
     reply.transaction!=ticket_->sequence || reply.word>0xffffff || session_->transactions().poisoned(Endpoint::A))return fail();
  DspTransactionProgress progress;
  if(!session_->transactions().progress(*ticket_,progress) || progress.status!=DspStatus::Observed ||
     progress.address.address!=KSCommandFeedback::partStatusAddress(mailbox_) ||
     !session_->transactions().retire(*ticket_) || !state_->observePart(mailbox_,reply.word))return fail();
  ticket_.reset();result_=KSCommandFeedback::partResult(reply.word);complete_=true;return true;
 }
 std::optional<DspTicket> ticket()const noexcept{return ticket_;}
 std::optional<KSPartFeedbackResult> result()const noexcept{return complete_?std::optional{result_}:std::nullopt;}
 bool reset()noexcept {
  if(ticket_&&!complete_&&!fault_)return false;
  state_=nullptr;session_=nullptr;ticket_.reset();complete_=fault_=false;return true;
 }
 bool abort()noexcept{return fail();}
private:
 bool live()const noexcept{return state_&&!fault_&&session_->snapshot().session==state_->session()&&
  session_->snapshot().lifecycle==Lifecycle::Running&&session_->rateGeneration()==rate_;}
 bool fail()noexcept {
  if(state_&&ticket_&&session_->snapshot().session==state_->session())session_->stop(state_->session());
  fault_=true;return false;
 }
 KSGlobalState* state_=nullptr;CommandSession* session_=nullptr;KSPart mailbox_=KSPart::One;
 uint64_t layoutRevision_=0,rate_=0;std::optional<DspTicket> ticket_;KSPartFeedbackResult result_{};
 bool complete_=false,fault_=false;
};
}
