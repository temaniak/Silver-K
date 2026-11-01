#pragma once
#include <cstdint>
namespace ks {
// Per-part channel routing and mode transition cleanup. Event flavor is a
// semantic capability of the selected handler; raw function references exist
// only in the temporary migration bridge, never in routing decisions.
class MidiRouteTask final {
public:
 enum class Flavor {Other,Program,Control};
 enum class Stage {ReadStatus,CompareSystem,AdmitSystem,TailSystem,BindHandler,ReadChannel,ReadMask,ClearPart,SetParts,BindPatch,ReadMode,AdmitMode,FirstPart,FirstPatch,
 PublishPart,DispatchControls,AwaitControls,PublishPatch,ShiftMask,AdmitEnabled,ReadPartChannel,CompareChannel,AdmitChannel,MarkDelivered,
 RetainPart,RetainInput,RetainMode,RetainEvent,RetainPatch,RetainHandler,DispatchHandler,AwaitHandler,RestoreHandler,RestorePatch,RestoreEvent,RestoreMode,RestoreInput,RestorePart,
 AdvancePatch,TickPart,ObserveMode,AdmitObservedMode,ReadChangedMode,AdmitChangedMode,TickParts,AdmitParts,ReadFinalMode,AdmitFinalMode,
 ClearFallbackPart,PublishFallbackPart,DispatchFallbackControls,AwaitFallbackControls,BindFallbackPatch,PublishFallbackPatch,ReadFallbackChannel,CompareFallbackChannel,AdmitFallbackChannel,
 RetainFallbackInput,CompareProgram,AdmitProgram,DispatchProgram,AwaitProgram,SkipFallback,CompareControl,AdmitControl,SetMirror,PublishMirror,DispatchControl,AwaitControl,ClearMirror,PublishMirrorEnd,RestoreFallbackInput,Finish,
 InvalidateChannel,TickUnusedParts,AdmitUnusedParts,PublishUnusedPart,DispatchUnusedPart,AwaitUnusedPart,PublishUnusedBank,ShiftUnusedMask,TickUnusedPart,LoopUnused,
 PublishFinalPart,DispatchFinalPart,AwaitFinalPart,PublishFinalBank,FinishCleanup,HandoffSystem,Complete};
 enum class Read {Status,Flavor,Channel,Mask,Mode,ChangedMode,FinalMode,PartChannel};
 enum class Write {SelectedPart,Patch,Mirror,UnusedBank};
 Stage stage()const{return stage_;}uint8_t status()const{return status_;}uint8_t channel()const{return channel_;}uint8_t mask()const{return mask_;}
 uint8_t index()const{return index_;}unsigned remaining()const{return remaining_;}uint8_t mode()const{return mode_;}uint8_t changedMode()const{return changedMode_;}uint8_t finalMode()const{return finalMode_;}
 uint8_t partChannel()const{return partChannel_;}uint8_t controlPart()const{return controlPart_;}uint8_t parameterPart()const{return parameterPart_;}uint32_t patch()const{return patch_;}Flavor flavor()const{return flavor_;}
 bool awaitingControls()const{return stage_==Stage::AwaitControls||stage_==Stage::AwaitFallbackControls;}
 bool resumeControls(uint8_t p){if(!awaitingControls())return false;controlPart_=p;stage_=stage_==Stage::AwaitControls?Stage::PublishPatch:Stage::BindFallbackPatch;return true;}
 bool awaitingPart()const{return stage_==Stage::AwaitUnusedPart||stage_==Stage::AwaitFinalPart;}
 bool resumePart(uint8_t p){if(!awaitingPart())return false;parameterPart_=p;stage_=stage_==Stage::AwaitUnusedPart?Stage::PublishUnusedBank:Stage::PublishFinalBank;return true;}
 bool awaitingChild()const{return stage_==Stage::AwaitHandler||stage_==Stage::AwaitProgram||stage_==Stage::AwaitControl;}
 bool resumeChild(){switch(stage_){case Stage::AwaitHandler:stage_=Stage::RestoreHandler;break;case Stage::AwaitProgram:stage_=Stage::SkipFallback;break;case Stage::AwaitControl:stage_=Stage::ClearMirror;break;default:return false;}return true;}
 bool complete()const{return stage_==Stage::Complete;}bool handoff()const{return stage_==Stage::HandoffSystem;}
 bool publication()const{switch(stage_){case Stage::PublishPart:case Stage::PublishFallbackPart:case Stage::PublishMirror:case Stage::PublishMirrorEnd:case Stage::PublishUnusedPart:case Stage::PublishUnusedBank:case Stage::PublishFinalPart:case Stage::PublishFinalBank:return true;default:return false;}}
 Write publicationKind()const{return stage_==Stage::PublishMirror||stage_==Stage::PublishMirrorEnd?Write::Mirror:stage_==Stage::PublishUnusedBank||stage_==Stage::PublishFinalBank?Write::UnusedBank:Write::SelectedPart;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){
 case Stage::ReadStatus:case Stage::ReadChannel:case Stage::ReadMask:case Stage::ReadMode:case Stage::ReadChangedMode:case Stage::ReadFinalMode:return 12;
 case Stage::BindPatch:case Stage::FirstPatch:case Stage::AdvancePatch:case Stage::BindFallbackPatch:case Stage::CompareProgram:case Stage::CompareControl:return 9;
 case Stage::PublishPart:case Stage::PublishFallbackPart:case Stage::PublishMirror:case Stage::PublishMirrorEnd:case Stage::PublishUnusedPart:case Stage::PublishFinalPart:return 9+store;
 case Stage::PublishUnusedBank:case Stage::PublishFinalBank:return 12+store;
 case Stage::DispatchControls:case Stage::DispatchFallbackControls:case Stage::DispatchProgram:case Stage::DispatchControl:case Stage::DispatchUnusedPart:case Stage::DispatchFinalPart:return 20;
 case Stage::PublishPatch:case Stage::PublishFallbackPatch:return 21;case Stage::ReadPartChannel:case Stage::ReadFallbackChannel:return 15;
 case Stage::RetainPart:case Stage::RetainInput:case Stage::RetainMode:case Stage::RestoreMode:case Stage::RestoreInput:case Stage::RestorePart:case Stage::RetainFallbackInput:case Stage::RestoreFallbackInput:return 11;
 case Stage::RetainEvent:case Stage::RetainPatch:case Stage::RetainHandler:case Stage::RestoreHandler:case Stage::RestorePatch:case Stage::RestoreEvent:return 20;
 case Stage::DispatchHandler:return 15;case Stage::Finish:case Stage::FinishCleanup:return 17;
 case Stage::AwaitControls:case Stage::AwaitFallbackControls:case Stage::AwaitHandler:case Stage::AwaitProgram:case Stage::AwaitControl:case Stage::AwaitUnusedPart:case Stage::AwaitFinalPart:case Stage::HandoffSystem:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationKind()):3)){
  if(cost>available-used)break;switch(stage_){
  case Stage::ReadStatus:status_=uint8_t(read(Read::Status));stage_=Stage::CompareSystem;break;case Stage::CompareSystem:stage_=Stage::AdmitSystem;break;case Stage::AdmitSystem:stage_=status_>=240?Stage::TailSystem:Stage::BindHandler;break;case Stage::TailSystem:stage_=Stage::HandoffSystem;break;
  case Stage::BindHandler:flavor_=Flavor(read(Read::Flavor));stage_=Stage::ReadChannel;break;case Stage::ReadChannel:channel_=uint8_t(read(Read::Channel));stage_=Stage::ReadMask;break;case Stage::ReadMask:mask_=uint8_t(read(Read::Mask));stage_=Stage::ClearPart;break;
  case Stage::ClearPart:index_=0;stage_=Stage::SetParts;break;case Stage::SetParts:remaining_=4;stage_=Stage::BindPatch;break;case Stage::BindPatch:patch_=0x168fc;stage_=Stage::ReadMode;break;
  case Stage::ReadMode:mode_=uint8_t(read(Read::Mode));stage_=Stage::AdmitMode;break;case Stage::AdmitMode:stage_=mode_?Stage::FirstPart:Stage::PublishPart;break;case Stage::FirstPart:index_=1;stage_=Stage::FirstPatch;break;case Stage::FirstPatch:patch_=0x169fc;stage_=Stage::PublishPart;break;
  case Stage::PublishPart:write(Write::SelectedPart,index_);stage_=Stage::DispatchControls;break;case Stage::DispatchControls:stage_=Stage::AwaitControls;break;case Stage::PublishPatch:write(Write::Patch,patch_);stage_=Stage::ShiftMask;break;
  case Stage::ShiftMask:enabled_=mask_&1;mask_>>=1;stage_=Stage::AdmitEnabled;break;case Stage::AdmitEnabled:stage_=enabled_?Stage::ReadPartChannel:Stage::AdvancePatch;break;
  case Stage::ReadPartChannel:partChannel_=uint8_t(read(Read::PartChannel));stage_=Stage::CompareChannel;break;case Stage::CompareChannel:stage_=Stage::AdmitChannel;break;
  case Stage::AdmitChannel:stage_=partChannel_==channel_?Stage::MarkDelivered:Stage::AdvancePatch;break;case Stage::MarkDelivered:mask_|=8;stage_=Stage::RetainPart;break;
  case Stage::RetainPart:stage_=Stage::RetainInput;break;case Stage::RetainInput:stage_=Stage::RetainMode;break;case Stage::RetainMode:stage_=Stage::RetainEvent;break;case Stage::RetainEvent:stage_=Stage::RetainPatch;break;case Stage::RetainPatch:stage_=Stage::RetainHandler;break;case Stage::RetainHandler:stage_=Stage::DispatchHandler;break;
  case Stage::DispatchHandler:stage_=Stage::AwaitHandler;break;case Stage::RestoreHandler:stage_=Stage::RestorePatch;break;case Stage::RestorePatch:stage_=Stage::RestoreEvent;break;case Stage::RestoreEvent:stage_=Stage::RestoreMode;break;case Stage::RestoreMode:stage_=Stage::RestoreInput;break;case Stage::RestoreInput:stage_=Stage::RestorePart;break;case Stage::RestorePart:stage_=Stage::AdvancePatch;break;
  case Stage::AdvancePatch:patch_+=256;stage_=Stage::TickPart;break;case Stage::TickPart:++index_;stage_=Stage::ObserveMode;break;case Stage::ObserveMode:stage_=Stage::AdmitObservedMode;break;case Stage::AdmitObservedMode:stage_=mode_?Stage::ReadChangedMode:Stage::TickParts;break;
  case Stage::ReadChangedMode:changedMode_=uint8_t(read(Read::ChangedMode));stage_=Stage::AdmitChangedMode;break;case Stage::AdmitChangedMode:stage_=changedMode_?Stage::TickParts:Stage::InvalidateChannel;break;
  case Stage::TickParts:--remaining_;stage_=Stage::AdmitParts;break;case Stage::AdmitParts:stage_=remaining_?Stage::PublishPart:Stage::ReadFinalMode;break;
  case Stage::ReadFinalMode:finalMode_=uint8_t(read(Read::FinalMode));stage_=Stage::AdmitFinalMode;break;case Stage::AdmitFinalMode:stage_=finalMode_?Stage::ClearFallbackPart:Stage::Finish;break;
  case Stage::ClearFallbackPart:index_=0;stage_=Stage::PublishFallbackPart;break;case Stage::PublishFallbackPart:write(Write::SelectedPart,0);stage_=Stage::DispatchFallbackControls;break;
  case Stage::DispatchFallbackControls:stage_=Stage::AwaitFallbackControls;break;case Stage::BindFallbackPatch:patch_=0x168fc;stage_=Stage::PublishFallbackPatch;break;case Stage::PublishFallbackPatch:write(Write::Patch,patch_);stage_=Stage::ReadFallbackChannel;break;
  case Stage::ReadFallbackChannel:partChannel_=uint8_t(read(Read::PartChannel));stage_=Stage::CompareFallbackChannel;break;case Stage::CompareFallbackChannel:stage_=Stage::AdmitFallbackChannel;break;case Stage::AdmitFallbackChannel:stage_=partChannel_==channel_?Stage::RetainFallbackInput:Stage::Finish;break;
  case Stage::RetainFallbackInput:stage_=Stage::CompareProgram;break;case Stage::CompareProgram:stage_=Stage::AdmitProgram;break;case Stage::AdmitProgram:stage_=flavor_==Flavor::Program?Stage::DispatchProgram:Stage::CompareControl;break;
  case Stage::DispatchProgram:stage_=Stage::AwaitProgram;break;case Stage::SkipFallback:stage_=Stage::RestoreFallbackInput;break;case Stage::CompareControl:stage_=Stage::AdmitControl;break;case Stage::AdmitControl:stage_=flavor_==Flavor::Control?Stage::SetMirror:Stage::RestoreFallbackInput;break;
  case Stage::SetMirror:stage_=Stage::PublishMirror;break;case Stage::PublishMirror:write(Write::Mirror,1);stage_=Stage::DispatchControl;break;case Stage::DispatchControl:stage_=Stage::AwaitControl;break;case Stage::ClearMirror:stage_=Stage::PublishMirrorEnd;break;case Stage::PublishMirrorEnd:write(Write::Mirror,0);stage_=Stage::RestoreFallbackInput;break;case Stage::RestoreFallbackInput:stage_=Stage::Finish;break;
  case Stage::InvalidateChannel:channel_=255;stage_=Stage::TickUnusedParts;break;case Stage::TickUnusedParts:--remaining_;stage_=Stage::AdmitUnusedParts;break;case Stage::AdmitUnusedParts:stage_=remaining_?Stage::PublishUnusedPart:Stage::PublishFinalPart;break;
  case Stage::PublishUnusedPart:write(Write::SelectedPart,index_);stage_=Stage::DispatchUnusedPart;break;case Stage::DispatchUnusedPart:stage_=Stage::AwaitUnusedPart;break;case Stage::PublishUnusedBank:write(Write::UnusedBank,255);stage_=Stage::ShiftUnusedMask;break;case Stage::ShiftUnusedMask:mask_>>=1;stage_=Stage::TickUnusedPart;break;case Stage::TickUnusedPart:++index_;stage_=Stage::LoopUnused;break;case Stage::LoopUnused:stage_=Stage::TickUnusedParts;break;
  case Stage::PublishFinalPart:write(Write::SelectedPart,0);stage_=Stage::DispatchFinalPart;break;case Stage::DispatchFinalPart:stage_=Stage::AwaitFinalPart;break;case Stage::PublishFinalBank:write(Write::UnusedBank,255);stage_=Stage::FinishCleanup;break;
  case Stage::Finish:case Stage::FinishCleanup:stage_=Stage::Complete;break;default:break;
  }used+=cost;
 }return used;}
private:Stage stage_=Stage::ReadStatus;Flavor flavor_=Flavor::Other;bool enabled_=false;uint8_t status_=0,channel_=0,mask_=0,index_=0,mode_=0,changedMode_=0,finalMode_=0,partChannel_=0,controlPart_=0,parameterPart_=0;unsigned remaining_=0;uint32_t patch_=0;
};
}
