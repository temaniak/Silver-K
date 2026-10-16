#pragma once
#include <cstdint>
namespace ks{
// Two banks of four panel decay cells, coalesced expiry flags and one-shot
// latch holdoff. No hardware addresses or CPU caller state live in this task.
class PanelDecayTask final{public:
 enum class Stage{RetainCaller,BindFirst,ClearFirstChanges,SetFirstCount,ReadFirst,AdmitFirst,DecayFirst,PublishFirst,AdmitFirstZero,MarkFirstChanged,AdvanceFirst,TickFirst,LoopFirst,ReadFirstPending,MergeFirstPending,PublishFirstPending,BindSecond,ClearSecondChanges,SetSecondCount,ReadSecond,AdmitSecond,DecaySecond,PublishSecond,AdmitSecondZero,MarkSecondChanged,AdvanceSecond,TickSecond,LoopSecond,ReadSecondPending,MergeSecondPending,PublishSecondPending,RestoreCaller,ReadHoldoff,AdmitHoldoff,TickHoldoff,PublishHoldoff,AdmitHoldoffExpiry,RetainLatch,SetLatch,DispatchLatch,RestoreLatch,Finish,AwaitLatch,Complete};
 enum class Read{Cell,Pending,Holdoff};enum class Write{Cell,Pending,Holdoff};
 Stage stage()const{return stage_;}unsigned bank()const{return bank_;}unsigned index()const{return index_;}unsigned remaining()const{return remaining_;}
 uint8_t cell()const{return cell_;}uint8_t changed()const{return changed_;}uint8_t pending()const{return pending_;}uint8_t holdoff()const{return holdoff_;}
 bool complete()const{return stage_==Stage::Complete;}bool awaiting()const{return stage_==Stage::AwaitLatch;}bool resumeChild(){if(!awaiting())return false;stage_=Stage::RestoreLatch;return true;}static constexpr unsigned latchRequest(){return 12;}
 unsigned nextClocks()const{switch(stage_){case Stage::RetainCaller:case Stage::RestoreCaller:return 20;case Stage::BindFirst:case Stage::BindSecond:return 9;case Stage::SetFirstCount:case Stage::SetSecondCount:return 6;
 case Stage::ReadFirst:case Stage::ReadSecond:case Stage::PublishFirst:case Stage::PublishSecond:return 6;
 case Stage::ReadFirstPending:case Stage::ReadSecondPending:case Stage::PublishFirstPending:case Stage::PublishSecondPending:case Stage::ReadHoldoff:case Stage::PublishHoldoff:return 12;
 case Stage::RetainLatch:case Stage::RestoreLatch:return 11;case Stage::DispatchLatch:return 20;case Stage::Finish:return 17;case Stage::AwaitLatch:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){unsigned used=0;while(auto cost=nextClocks()){if(cost>available-used)break;auto next=Stage(unsigned(stage_)+1);switch(stage_){
 case Stage::BindFirst:bank_=0;index_=0;break;case Stage::BindSecond:bank_=1;index_=0;break;
 case Stage::ClearFirstChanges:case Stage::ClearSecondChanges:changed_=0;break;case Stage::SetFirstCount:case Stage::SetSecondCount:remaining_=4;break;
 case Stage::ReadFirst:case Stage::ReadSecond:cell_=uint8_t(read(Read::Cell,bank_,index_));break;
 case Stage::AdmitFirst:if(!cell_)next=Stage::AdvanceFirst;break;case Stage::AdmitSecond:if(!cell_)next=Stage::AdvanceSecond;break;
 case Stage::DecayFirst:case Stage::DecaySecond:cell_>>=1;break;
 case Stage::PublishFirst:case Stage::PublishSecond:write(Write::Cell,bank_,index_,cell_);break;
 case Stage::AdmitFirstZero:if(cell_)next=Stage::AdvanceFirst;break;case Stage::AdmitSecondZero:if(cell_)next=Stage::AdvanceSecond;break;
 case Stage::MarkFirstChanged:case Stage::MarkSecondChanged:changed_=1;break;
 case Stage::AdvanceFirst:case Stage::AdvanceSecond:++index_;break;case Stage::TickFirst:case Stage::TickSecond:--remaining_;break;
 case Stage::LoopFirst:if(remaining_)next=Stage::ReadFirst;break;case Stage::LoopSecond:if(remaining_)next=Stage::ReadSecond;break;
 case Stage::ReadFirstPending:case Stage::ReadSecondPending:pending_=uint8_t(read(Read::Pending,bank_,0));break;
 case Stage::MergeFirstPending:case Stage::MergeSecondPending:pending_|=changed_;break;
 case Stage::PublishFirstPending:case Stage::PublishSecondPending:write(Write::Pending,bank_,0,pending_);break;
 case Stage::ReadHoldoff:holdoff_=uint8_t(read(Read::Holdoff,0,0));break;case Stage::AdmitHoldoff:if(!holdoff_)next=Stage::Finish;break;
 case Stage::TickHoldoff:--holdoff_;break;case Stage::PublishHoldoff:write(Write::Holdoff,0,0,holdoff_);break;
 case Stage::AdmitHoldoffExpiry:if(holdoff_)next=Stage::Finish;break;case Stage::DispatchLatch:next=Stage::AwaitLatch;break;case Stage::Finish:next=Stage::Complete;break;
 default:break;}stage_=next;used+=cost;}return used;}
private:Stage stage_=Stage::RetainCaller;unsigned bank_=0,index_=0,remaining_=0;uint8_t cell_=0,changed_=0,pending_=0,holdoff_=0;};}
