#pragma once

#include "ksGlobalState.h"
#include <exception>

namespace novation::controller::command {
inline std::optional<KSRefreshPacket> encodeKsGlobalUpdate(const KSGlobalState::Snapshot& source)noexcept {
 if((source.layout!=KSLayout::Single && source.layout!=KSLayout::Performance) ||
    size_t(source.source)>=5 || (source.layout==KSLayout::Performance && source.source==KSStorageSlot::Single && !source.storedSingle) ||
    (source.storedSingle && (source.layout!=KSLayout::Performance || source.source!=KSStorageSlot::Single || source.mailbox || source.includeAuxiliary)) ||
    (source.mailbox && KSGlobalState::mailboxSource(source.layout,*source.mailbox)!=source.source))return std::nullopt;
 KSRefreshPacket packet;
 if(source.mailbox && !packet.appendWord(Endpoint::A,KSCommandFeedback::partStatusAddress(*source.mailbox),0))return std::nullopt;
 if(source.includeAuxiliary && !appendKsAuxiliary(packet,source.auxiliary))return std::nullopt;
 const auto performance=source.layout==KSLayout::Performance && !source.storedSingle?KSMusicalState::logicalPart(source.source):std::nullopt;
 if(!appendKsGlobals(packet,source.part,source.master,source.tables,performance))return std::nullopt;
 return packet;
}

// One complete immutable global/FX update. Semantic groups decide whether
// work is required; the proven complete wire sequence is retained. Replies
// requesting a rebuild remain mandatory even with identical desired values.
class KSCommandGlobalUpdate final {
public:
 KSCommandGlobalUpdate()=default;
 ~KSCommandGlobalUpdate(){if(accepted_&&!complete_&&!fault_)fail();}
 KSCommandGlobalUpdate(const KSCommandGlobalUpdate&)=delete;KSCommandGlobalUpdate& operator=(const KSCommandGlobalUpdate&)=delete;
 bool prepare(CommandSession& session,KSGlobalState& state,KSStorageSlot part,std::optional<KSPart> mailbox={})noexcept {
  return prepareSnapshot(session,state,state.snapshot(part,mailbox));
 }
 bool prepareStoredSingle(CommandSession& session,KSGlobalState& state)noexcept {
  return prepareSnapshot(session,state,state.snapshotStoredSingle());
 }
private:
 bool prepareSnapshot(CommandSession& session,KSGlobalState& state,const std::optional<KSGlobalState::Snapshot>& source)noexcept {
  if(state_ || session.snapshot().session!=state.session() || session.snapshot().lifecycle!=Lifecycle::Running || state.busy())return false;
  if(!source)return false;
  const auto packet=encodeKsGlobalUpdate(*source);if(!packet)return false;
  state_=&state;session_=&session;source_=*source;packet_=*packet;rate_=session.rateGeneration();
  endpoints_=0;for(size_t i=0;i<packet_.size();++i)endpoints_|=uint8_t(1u<<unsigned(packet_.at(i)->endpoint));return true;
 }
public:
 DspAdmission admit(uint64_t operation,NativeFrame at)noexcept {
  if(!live())return reject(DspAdmission::StaleSession);
  if(ticket_ || complete_)return DspAdmission::StaleOperation;
  if(!state_->current(*source_,accepted_,this))return reject(DspAdmission::InvalidDraft);
  if(poisoned())return reject(DspAdmission::Poisoned);
  const auto write=packet_.at(next_);if(!write)return reject(DspAdmission::InvalidDraft);
  DspTransactionDraft draft;draft.model=Model::KS;draft.endpoint=write->endpoint;draft.earliest=at;
  draft.address={DspAddressMode::Write,write->address};draft.words=write->words;draft.wordCount=write->count;
  DspTicket ticket;
  const auto result=session_->transactions().submit(state_->session(),operation,&draft,1,&ticket,[&]()noexcept{
   if(!accepted_ && !state_->claim(*source_,this))std::terminate();accepted_=true;
  });
  if(result==DspAdmission::Accepted)ticket_=ticket;
  else if(result!=DspAdmission::Full)return reject(result);
  return result;
 }
 KSRefreshProgress progress()noexcept {
  if(complete_&&live())return KSRefreshProgress::Complete;
  if(!live() || !accepted_ || !state_->current(*source_,true,this) || poisoned())return fail();
  if(!ticket_)return KSRefreshProgress::Ready;
  DspTransactionProgress progress;
  if(!session_->transactions().progress(*ticket_,progress) || session_->transactions().poisoned(ticket_->endpoint))return fail();
  if(progress.status==DspStatus::Queued)return KSRefreshProgress::WaitingForTransfer;
  if(progress.status!=DspStatus::Transferred || !session_->transactions().retire(*ticket_))return fail();
  ticket_.reset();
  if(++next_!=packet_.size())return KSRefreshProgress::Ready;
  if(!state_->complete(*source_,this))return fail();complete_=true;return KSRefreshProgress::Complete;
 }
 bool reset()noexcept {
  if(accepted_&&!complete_&&!fault_)return false;
  state_=nullptr;session_=nullptr;source_.reset();ticket_.reset();next_=0;endpoints_=0;accepted_=complete_=fault_=false;return true;
 }
 KSRefreshProgress abort()noexcept{return fail();}
 const KSRefreshPacket& packet()const noexcept{return packet_;}
 std::optional<DspTicket> ticket()const noexcept{return ticket_;}
 const KSGlobalState::Snapshot* source()const noexcept{return source_?&*source_:nullptr;}
private:
 bool poisoned()const noexcept {
  for(auto endpoint:{Endpoint::A,Endpoint::B})if((endpoints_&(1u<<unsigned(endpoint))) &&
   session_->transactions().poisoned(endpoint))return true;
  return false;
 }
 bool live()const noexcept{return state_&&!fault_&&session_->snapshot().session==state_->session()&&
  session_->snapshot().lifecycle==Lifecycle::Running&&session_->rateGeneration()==rate_;}
 DspAdmission reject(DspAdmission result)noexcept{if(accepted_)fail();return result;}
 KSRefreshProgress fail()noexcept {
  if(state_&&accepted_&&!complete_&&session_->snapshot().session==state_->session())session_->stop(state_->session());
  if(state_)state_->abandon(this);fault_=true;return KSRefreshProgress::Fault;
 }
 KSGlobalState* state_=nullptr;CommandSession* session_=nullptr;std::optional<KSGlobalState::Snapshot> source_;
 KSRefreshPacket packet_;std::optional<DspTicket> ticket_;uint64_t rate_=0;size_t next_=0;
 uint8_t endpoints_=0;
 bool accepted_=false,complete_=false,fault_=false;
};
}
