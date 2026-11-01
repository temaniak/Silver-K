#pragma once
#include <cstdint>
namespace ks {
// ADC scan starts retain the original sixteen warmup passes, 2663-count delay
// and live completion polling. Mux and settling are separate nested children.
class PanelAdcScanTask final {
public:
 enum class Kind { Initialize, Start, Mux, ResetLast };
 enum class Stage { SetInitialGroup, WriteInitialGroup, SetPasses, SavePass, StartPass, SaveDelayBase, SetDelayPasses, SaveDelayPasses, SetDelay, DecrementDelay, MoreDelay, RestoreDelayPasses, DecrementDelayPasses, MoreDelayPasses, RestoreDelayBase, ReadCompletedGroup, CompareCompletedGroup, WaitCompletedGroup, RestorePass, DecrementPass, MorePasses, Snapshot, ResetControls, SetLast, WriteLast, ReturnInitial, SaveStartBase, SaveStartAux, ReadStartGroup, CompareStartGroup, SelectStartGroup, ClearStartGroup, WriteStartGroup, SelectMux, SetConversion, WriteConversion, RestoreStartAux, RestoreStartBase, ReturnStart, ReadMuxGroup, HalfMux, QuarterMux, WriteMux, SetSettling, WriteSettling, ReturnMux, AwaitChild, Complete };
 enum class Read { CompletedGroup, StartGroup, MuxGroup };enum class Write { Group, Last, Status, Mux, Settling };
 explicit PanelAdcScanTask(Kind kind):stage_(kind==Kind::Initialize?Stage::SetInitialGroup:kind==Kind::ResetLast?Stage::SetLast:kind==Kind::Start?Stage::SaveStartBase:Stage::ReadMuxGroup){}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}
 bool complete()const{return stage_==Stage::Complete;}bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 uint8_t group()const{return group_;}uint8_t mux()const{return mux_;}unsigned pass()const{return passes_;}
 unsigned nextClocks(unsigned countWrite=3)const{switch(stage_){case Stage::WriteConversion:case Stage::WriteMux:return 4; case Stage::SetDelayPasses:case Stage::SetDelay:return 6; case Stage::SavePass:case Stage::SaveDelayBase:case Stage::SaveDelayPasses:case Stage::RestoreDelayPasses:case Stage::RestoreDelayBase:case Stage::RestorePass:case Stage::SaveStartBase:case Stage::SaveStartAux:case Stage::RestoreStartAux:case Stage::RestoreStartBase:return 11; case Stage::WriteInitialGroup:case Stage::ReadCompletedGroup:case Stage::WriteLast:case Stage::ReadStartGroup:case Stage::WriteStartGroup:case Stage::ReadMuxGroup:return 12; case Stage::SelectMux:return 15; case Stage::ReturnInitial:case Stage::ReturnStart:case Stage::ReturnMux:return 17; case Stage::StartPass:case Stage::Snapshot:case Stage::ResetControls:return 20; case Stage::WriteSettling:return 9+countWrite;case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned countWrite=3){
  const auto cost=nextClocks(countWrite);if(!cost||cost>available)return 0;
  switch(stage_){
  case Stage::WriteInitialGroup:write(Write::Group,36);next();break;
  case Stage::SetPasses:passes_=16;next();break;
  case Stage::StartPass:await(Stage::SaveDelayBase);break;
  case Stage::SetDelayPasses:delayPasses_=1;next();break;
  case Stage::SetDelay:delay_=2663;next();break;
  case Stage::DecrementDelay:--delay_;next();break;
  case Stage::MoreDelay:stage_=delay_?Stage::DecrementDelay:Stage::RestoreDelayPasses;break;
  case Stage::DecrementDelayPasses:--delayPasses_;next();break;
  case Stage::MoreDelayPasses:stage_=delayPasses_?Stage::SaveDelayPasses:Stage::RestoreDelayBase;break;
  case Stage::ReadCompletedGroup:group_=uint8_t(read(Read::CompletedGroup));next();break;
  case Stage::WaitCompletedGroup:stage_=group_<=32?Stage::ReadCompletedGroup:Stage::RestorePass;break;
  case Stage::DecrementPass:--passes_;next();break;
  case Stage::MorePasses:stage_=passes_?Stage::SavePass:Stage::Snapshot;break;
  case Stage::Snapshot:await(Stage::ResetControls);break;
  case Stage::ResetControls:await(Stage::SetLast);break;
  case Stage::WriteLast:write(Write::Last,255);next();break;
  case Stage::ReadStartGroup:group_=uint8_t(read(Read::StartGroup));next();break;
  case Stage::SelectStartGroup:stage_=group_<=32?Stage::RestoreStartAux:Stage::ClearStartGroup;break;
  case Stage::WriteStartGroup:write(Write::Group,0);next();break;
  case Stage::SelectMux:await(Stage::SetConversion);break;
  case Stage::WriteConversion:write(Write::Status,0x73);next();break;
  case Stage::ReadMuxGroup:group_=uint8_t(read(Read::MuxGroup));next();break;
  case Stage::QuarterMux:mux_=group_>>2;next();break;
  case Stage::WriteMux:write(Write::Mux,mux_);next();break;
  case Stage::WriteSettling:write(Write::Settling,9);next();break;
  case Stage::ReturnInitial:case Stage::ReturnStart:case Stage::ReturnMux:stage_=Stage::Complete;break;
  default:next();break;
  }return cost;
 }
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_,resume_=Stage::Complete;uint8_t group_=0,mux_=0;unsigned passes_=0,delayPasses_=0,delay_=0;
};
}
