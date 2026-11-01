#pragma once
#include <cstdint>
namespace ks {
// Initial engine state and selected-program lifecycle. External child services
// remain separate; native state contains domain fields and child completions.
class StateInitTask final {
public:
 enum class Stage {BindReset,SetResetCount,ClearResetValue,PublishReset,AdvanceReset,TickReset,AdmitReset,SetDefault,PublishDefault,DispatchPanelDefault,DispatchTimer0,DispatchTimer1,DispatchTimer2,DispatchTimer3,DispatchTimer4,DispatchBanks,DispatchNotes,DispatchMidi,DispatchPanelTimers,SetPinRequest,DispatchPin,ShiftPin,CapturePin,DispatchPanelValue,DispatchPanelState,DispatchGlobalBank,BindGlobal,PublishGlobal,ClearPatchControls,PublishPatch0,PublishPatch1,PublishPatch2,PublishPreviewGate,SetPatchBias,PublishPatchBias,ReadClock0High,ReadClock0Low,PublishClock0,ReadClock1High,ReadClock1Low,PublishClock1,ReadActiveMask,PublishActiveMask,ReadReceiveMask,PublishReceiveMask,SetSentinel,PublishSentinel0,PublishSentinel1,SetSingleMode,PublishSingleMode,DispatchSingleMode,DispatchSingle,SetMultiMode,PublishMultiMode,DispatchMultiMode,DispatchMulti,RebindGlobal,ReadPreferredMode,PublishPreferredMode,SetPreferredSentinel,PublishPreferredSentinel0,PublishPreferredSentinel1,DispatchPreferredMode,ReadSelectedMode,AdmitSelectedMode,DispatchPreferredSingle,SkipPreferredMulti,DispatchPreferredMulti,DispatchConfiguration,DispatchDisplay,DispatchClock,PublishClock,DispatchHardware,AwaitChild,Complete};
 enum class Child {PanelDefault,Timer0,Timer1,Timer2,Timer3,Timer4,Banks,Notes,Midi,PanelTimers,Pin,PanelValue,PanelState,GlobalBank,SingleMode,Single,MultiMode,Multi,PreferredMode,PreferredSingle,PreferredMulti,Configuration,Display,Clock,Hardware};
 enum class Read {HardwarePin,Clock0High,Clock0Low,Clock1High,Clock1Low,ActiveMask,ReceiveMask,PreferredMode,SelectedMode,CurrentClock};
 enum class Write {ResetCell,Default,ActiveGlobalPatch,Patch0,Patch1,Patch2,PreviewGate,PatchBias,Clock0,Clock1,ActiveMask,ReceiveMask,Sentinel0,Sentinel1,SelectedMode,LastClock};
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}
 Child child()const{return child_;}bool awaiting()const{return stage_==Stage::AwaitChild;}
 bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(){if(!awaiting())return false;stage_=resume_;return true;}
 unsigned position()const{return cursor_;}unsigned remaining()const{return remaining_;}
 uint8_t byte()const{return byte_;}uint16_t word()const{return word_;}uint8_t selectedMode()const{return selectedMode_;}
 bool publication()const{switch(stage_){case Stage::PublishReset:case Stage::PublishDefault:case Stage::PublishGlobal:case Stage::PublishPatch0:case Stage::PublishPatch1:case Stage::PublishPatch2:case Stage::PublishPreviewGate:case Stage::PublishPatchBias:case Stage::PublishClock0:case Stage::PublishClock1:case Stage::PublishActiveMask:case Stage::PublishReceiveMask:case Stage::PublishSentinel0:case Stage::PublishSentinel1:case Stage::PublishSingleMode:case Stage::PublishMultiMode:case Stage::PublishPreferredMode:case Stage::PublishPreferredSentinel0:case Stage::PublishPreferredSentinel1:case Stage::PublishClock:return true;default:return false;}}
 Write publicationField()const{switch(stage_){case Stage::PublishReset:return Write::ResetCell;case Stage::PublishDefault:return Write::Default;case Stage::PublishGlobal:return Write::ActiveGlobalPatch;
 case Stage::PublishPatch0:return Write::Patch0;case Stage::PublishPatch1:return Write::Patch1;case Stage::PublishPatch2:return Write::Patch2;
 case Stage::PublishPreviewGate:return Write::PreviewGate;case Stage::PublishPatchBias:return Write::PatchBias;
 case Stage::PublishClock0:return Write::Clock0;case Stage::PublishClock1:return Write::Clock1;
 case Stage::PublishActiveMask:return Write::ActiveMask;case Stage::PublishReceiveMask:return Write::ReceiveMask;
 case Stage::PublishSentinel0:case Stage::PublishPreferredSentinel0:return Write::Sentinel0;
 case Stage::PublishSentinel1:case Stage::PublishPreferredSentinel1:return Write::Sentinel1;
 case Stage::PublishClock:return Write::LastClock;default:return Write::SelectedMode;}}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){
 case Stage::BindReset:case Stage::BindGlobal:case Stage::RebindGlobal:return 9;case Stage::SetResetCount:return 6;
 case Stage::PublishReset:return 3+store;case Stage::PublishDefault:case Stage::PublishPreviewGate:case Stage::PublishActiveMask:case Stage::PublishReceiveMask:case Stage::PublishSentinel0:case Stage::PublishSentinel1:case Stage::PublishSingleMode:case Stage::PublishMultiMode:case Stage::PublishPreferredMode:case Stage::PublishPreferredSentinel0:case Stage::PublishPreferredSentinel1:return 9+store;
 case Stage::PublishPatch0:case Stage::PublishPatch1:case Stage::PublishPatch2:case Stage::PublishPatchBias:return 12+store;
 case Stage::PublishGlobal:return 18;case Stage::PublishClock0:case Stage::PublishClock1:case Stage::PublishClock:return 12;
 case Stage::ReadClock0High:case Stage::ReadClock0Low:case Stage::ReadClock1High:case Stage::ReadClock1Low:case Stage::ReadActiveMask:case Stage::ReadReceiveMask:case Stage::ReadPreferredMode:return 15;
 case Stage::ReadSelectedMode:return 12;
 case Stage::DispatchPanelDefault:return 20;
 case Stage::DispatchTimer0:return 20;
 case Stage::DispatchTimer1:return 20;
 case Stage::DispatchTimer2:return 20;
 case Stage::DispatchTimer3:return 20;
 case Stage::DispatchTimer4:return 20;
 case Stage::DispatchBanks:return 20;
 case Stage::DispatchNotes:return 20;
 case Stage::DispatchMidi:return 20;
 case Stage::DispatchPanelTimers:return 20;
 case Stage::DispatchPin:return 20;
 case Stage::DispatchPanelValue:return 20;
 case Stage::DispatchPanelState:return 20;
 case Stage::DispatchGlobalBank:return 20;
 case Stage::DispatchSingleMode:return 20;
 case Stage::DispatchSingle:return 20;
 case Stage::DispatchMultiMode:return 20;
 case Stage::DispatchMulti:return 20;
 case Stage::DispatchPreferredMode:return 20;
 case Stage::DispatchPreferredSingle:return 20;
 case Stage::DispatchPreferredMulti:return 20;
 case Stage::DispatchConfiguration:return 20;
 case Stage::DispatchDisplay:return 20;
 case Stage::DispatchClock:return 20;
 case Stage::DispatchHardware:return 20;
 case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;
 while(const auto cost=nextClocks(publication()?timing(publicationField(),cursor_):3u)){if(cost>available-used)break;
  const auto current=stage_;Stage next=Stage(unsigned(stage_)+1);
  switch(current){
  case Stage::BindReset:cursor_=0;break;case Stage::SetResetCount:remaining_=40;break;
  case Stage::PublishReset:write(Write::ResetCell,cursor_,0);break;case Stage::AdvanceReset:++cursor_;break;case Stage::TickReset:--remaining_;break;
  case Stage::AdmitReset:next=remaining_?Stage::PublishReset:Stage::SetDefault;break;
  case Stage::PublishDefault:write(Write::Default,0,5);break;case Stage::ShiftPin:byte_=uint8_t(read(Read::HardwarePin))>>1;break;
  case Stage::PublishGlobal:write(Write::ActiveGlobalPatch,0,0);break;
  case Stage::PublishPatch0:write(Write::Patch0,0,0);break;case Stage::PublishPatch1:write(Write::Patch1,0,0);break;case Stage::PublishPatch2:write(Write::Patch2,0,0);break;
  case Stage::PublishPreviewGate:write(Write::PreviewGate,0,0);break;case Stage::PublishPatchBias:write(Write::PatchBias,0,64);break;
  case Stage::ReadClock0High:byte_=uint8_t(read(Read::Clock0High));word_=uint16_t(unsigned(byte_)<<8);break;
  case Stage::ReadClock0Low:byte_=uint8_t(read(Read::Clock0Low));word_=uint16_t((word_&0xff00)|byte_);break;
  case Stage::PublishClock0:write(Write::Clock0,0,word_);break;
  case Stage::ReadClock1High:byte_=uint8_t(read(Read::Clock1High));word_=uint16_t(unsigned(byte_)<<8);break;
  case Stage::ReadClock1Low:byte_=uint8_t(read(Read::Clock1Low));word_=uint16_t((word_&0xff00)|byte_);break;
  case Stage::PublishClock1:write(Write::Clock1,0,word_);break;
  case Stage::ReadActiveMask:byte_=uint8_t(read(Read::ActiveMask));break;case Stage::PublishActiveMask:write(Write::ActiveMask,0,byte_);break;
  case Stage::ReadReceiveMask:byte_=uint8_t(read(Read::ReceiveMask));break;case Stage::PublishReceiveMask:write(Write::ReceiveMask,0,byte_);break;
  case Stage::PublishSentinel0:case Stage::PublishPreferredSentinel0:write(Write::Sentinel0,0,255);break;
  case Stage::PublishSentinel1:case Stage::PublishPreferredSentinel1:write(Write::Sentinel1,0,255);break;
  case Stage::PublishSingleMode:write(Write::SelectedMode,0,0);break;case Stage::PublishMultiMode:write(Write::SelectedMode,0,1);break;
  case Stage::ReadPreferredMode:byte_=uint8_t(read(Read::PreferredMode));break;case Stage::PublishPreferredMode:write(Write::SelectedMode,0,byte_);break;
  case Stage::ReadSelectedMode:selectedMode_=uint8_t(read(Read::SelectedMode));break;case Stage::AdmitSelectedMode:next=selectedMode_?Stage::DispatchPreferredMulti:Stage::DispatchPreferredSingle;break;
  case Stage::SkipPreferredMulti:next=Stage::DispatchConfiguration;break;
  case Stage::PublishClock:write(Write::LastClock,0,uint16_t(read(Read::CurrentClock)));break;
  case Stage::DispatchPanelDefault:child_=Child::PanelDefault;resume_=Stage::DispatchTimer0;next=Stage::AwaitChild;break;
  case Stage::DispatchTimer0:child_=Child::Timer0;resume_=Stage::DispatchTimer1;next=Stage::AwaitChild;break;
  case Stage::DispatchTimer1:child_=Child::Timer1;resume_=Stage::DispatchTimer2;next=Stage::AwaitChild;break;
  case Stage::DispatchTimer2:child_=Child::Timer2;resume_=Stage::DispatchTimer3;next=Stage::AwaitChild;break;
  case Stage::DispatchTimer3:child_=Child::Timer3;resume_=Stage::DispatchTimer4;next=Stage::AwaitChild;break;
  case Stage::DispatchTimer4:child_=Child::Timer4;resume_=Stage::DispatchBanks;next=Stage::AwaitChild;break;
  case Stage::DispatchBanks:child_=Child::Banks;resume_=Stage::DispatchNotes;next=Stage::AwaitChild;break;
  case Stage::DispatchNotes:child_=Child::Notes;resume_=Stage::DispatchMidi;next=Stage::AwaitChild;break;
  case Stage::DispatchMidi:child_=Child::Midi;resume_=Stage::DispatchPanelTimers;next=Stage::AwaitChild;break;
  case Stage::DispatchPanelTimers:child_=Child::PanelTimers;resume_=Stage::SetPinRequest;next=Stage::AwaitChild;break;
  case Stage::DispatchPin:child_=Child::Pin;resume_=Stage::ShiftPin;next=Stage::AwaitChild;break;
  case Stage::DispatchPanelValue:child_=Child::PanelValue;resume_=Stage::DispatchPanelState;next=Stage::AwaitChild;break;
  case Stage::DispatchPanelState:child_=Child::PanelState;resume_=Stage::DispatchGlobalBank;next=Stage::AwaitChild;break;
  case Stage::DispatchGlobalBank:child_=Child::GlobalBank;resume_=Stage::BindGlobal;next=Stage::AwaitChild;break;
  case Stage::DispatchSingleMode:child_=Child::SingleMode;resume_=Stage::DispatchSingle;next=Stage::AwaitChild;break;
  case Stage::DispatchSingle:child_=Child::Single;resume_=Stage::SetMultiMode;next=Stage::AwaitChild;break;
  case Stage::DispatchMultiMode:child_=Child::MultiMode;resume_=Stage::DispatchMulti;next=Stage::AwaitChild;break;
  case Stage::DispatchMulti:child_=Child::Multi;resume_=Stage::RebindGlobal;next=Stage::AwaitChild;break;
  case Stage::DispatchPreferredMode:child_=Child::PreferredMode;resume_=Stage::ReadSelectedMode;next=Stage::AwaitChild;break;
  case Stage::DispatchPreferredSingle:child_=Child::PreferredSingle;resume_=Stage::SkipPreferredMulti;next=Stage::AwaitChild;break;
  case Stage::DispatchPreferredMulti:child_=Child::PreferredMulti;resume_=Stage::DispatchConfiguration;next=Stage::AwaitChild;break;
  case Stage::DispatchConfiguration:child_=Child::Configuration;resume_=Stage::DispatchDisplay;next=Stage::AwaitChild;break;
  case Stage::DispatchDisplay:child_=Child::Display;resume_=Stage::DispatchClock;next=Stage::AwaitChild;break;
  case Stage::DispatchClock:child_=Child::Clock;resume_=Stage::PublishClock;next=Stage::AwaitChild;break;
  case Stage::DispatchHardware:child_=Child::Hardware;resume_=Stage::Complete;next=Stage::AwaitChild;break;
  default:break;}stage_=next;used+=cost;
 }return used;}
private:Stage stage_=Stage::BindReset,resume_=Stage::Complete;Child child_=Child::PanelDefault;unsigned cursor_=0,remaining_=0;uint8_t byte_=0,selectedMode_=0;uint16_t word_=0;
};
}
