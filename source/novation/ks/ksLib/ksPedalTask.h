#pragma once
#include <cstdint>
namespace ks {
// Sustain publication and falling-edge release. Stored state is always0/16;
// previous arbitrary byte values retain their original XOR/intersection rule.
class PedalTask final {
public:
 enum class Stage {CompareController,AdmitController,ClearDesired,CompareValue,AdmitValue,Hold,DispatchPart,AwaitPart,ReadPrevious,PublishCurrent,PublishMirror,
 RetainPart,CapturePatchPart,DispatchControls,AwaitControls,ReadPatch,RestorePart,ReadFlags,ObserveBypass,AdmitBypass,Changes,AdmitChanges,ReleasedBits,ObserveRelease,AdmitRelease,DispatchRelease,AwaitRelease,Finish,FinishBypass,Handoff,Complete};
 enum class Read {Controller,Value,Previous,PatchPart,PatchReference,Flags};
 explicit PedalTask(bool withController=true):stage_(withController?Stage::CompareController:Stage::ClearDesired){}
 Stage stage()const{return stage_;}uint8_t part()const{return part_;}uint8_t patchPart()const{return patchPart_;}uint8_t controlPart()const{return controlPart_;}
 uint8_t desired()const{return desired_;}uint8_t previous()const{return previous_;}uint8_t flags()const{return flags_;}
 uint8_t changed()const{return changed_;}uint8_t released()const{return released_;}uint32_t patchReference()const{return patchReference_;}
 bool awaitingPart()const{return stage_==Stage::AwaitPart;}bool awaitingControls()const{return stage_==Stage::AwaitControls;}bool awaitingRelease()const{return stage_==Stage::AwaitRelease;}
 bool resumePart(uint8_t p){if(!awaitingPart())return false;part_=p;stage_=Stage::ReadPrevious;return true;}
 bool resumeControls(uint8_t p){if(!awaitingControls())return false;controlPart_=p;stage_=Stage::ReadPatch;return true;}
 bool resumeRelease(){if(!awaitingRelease())return false;stage_=Stage::Finish;return true;}
 bool handoff()const{return stage_==Stage::Handoff;}bool complete()const{return stage_==Stage::Complete;}
 bool publication()const{return stage_==Stage::PublishCurrent||stage_==Stage::PublishMirror;}unsigned publicationOffset()const{return stage_==Stage::PublishCurrent?309:310;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){
 case Stage::DispatchPart:case Stage::DispatchControls:case Stage::DispatchRelease:case Stage::RetainPart:case Stage::RestorePart:return 20;
 case Stage::ReadPrevious:case Stage::ReadFlags:return 15;case Stage::ReadPatch:return 21;
 case Stage::PublishCurrent:case Stage::PublishMirror:return 12+store;case Stage::Finish:case Stage::FinishBypass:return 17;
 case Stage::AwaitPart:case Stage::AwaitControls:case Stage::AwaitRelease:case Stage::Handoff:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationOffset()):3)){
  if(cost>available-used)break;
  switch(stage_){
  case Stage::CompareController:controller_=uint8_t(read(Read::Controller));stage_=Stage::AdmitController;break;
  case Stage::AdmitController:stage_=controller_==64?Stage::ClearDesired:Stage::Handoff;break;
  case Stage::ClearDesired:desired_=0;stage_=Stage::CompareValue;break;case Stage::CompareValue:value_=uint8_t(read(Read::Value));stage_=Stage::AdmitValue;break;
  case Stage::AdmitValue:stage_=value_<64?Stage::DispatchPart:Stage::Hold;break;case Stage::Hold:desired_=16;stage_=Stage::DispatchPart;break;
  case Stage::DispatchPart:stage_=Stage::AwaitPart;break;case Stage::ReadPrevious:previous_=uint8_t(read(Read::Previous));stage_=Stage::PublishCurrent;break;
  case Stage::PublishCurrent:write(309,desired_);stage_=Stage::PublishMirror;break;case Stage::PublishMirror:write(310,desired_);stage_=Stage::RetainPart;break;
  case Stage::RetainPart:stage_=Stage::CapturePatchPart;break;case Stage::CapturePatchPart:patchPart_=uint8_t(read(Read::PatchPart));stage_=Stage::DispatchControls;break;
  case Stage::DispatchControls:stage_=Stage::AwaitControls;break;case Stage::ReadPatch:patchReference_=uint32_t(read(Read::PatchReference));stage_=Stage::RestorePart;break;
  case Stage::RestorePart:stage_=Stage::ReadFlags;break;case Stage::ReadFlags:flags_=uint8_t(read(Read::Flags));stage_=Stage::ObserveBypass;break;
  case Stage::ObserveBypass:stage_=Stage::AdmitBypass;break;case Stage::AdmitBypass:stage_=(flags_&4)?Stage::FinishBypass:Stage::Changes;break;
  case Stage::Changes:changed_=desired_^previous_;stage_=Stage::AdmitChanges;break;case Stage::AdmitChanges:stage_=changed_?Stage::ReleasedBits:Stage::Finish;break;
  case Stage::ReleasedBits:released_=previous_&changed_;stage_=Stage::ObserveRelease;break;case Stage::ObserveRelease:stage_=Stage::AdmitRelease;break;
  case Stage::AdmitRelease:stage_=(released_&16)?Stage::DispatchRelease:Stage::Finish;break;case Stage::DispatchRelease:stage_=Stage::AwaitRelease;break;
  case Stage::Finish:case Stage::FinishBypass:stage_=Stage::Complete;break;default:break;
  }used+=cost;
 }return used;}
private:Stage stage_;uint8_t part_=0,patchPart_=0,controlPart_=0,controller_=0,value_=0,desired_=0,previous_=0,flags_=0,changed_=0,released_=0;uint32_t patchReference_=0;
};
}
