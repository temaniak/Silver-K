#pragma once
#include <cstdint>
namespace ks {
// Native order of background controller services. Child work is independently
// owned; an interrupt can suspend the cycle without architectural save state.
class ServiceCycleTask final {
public:
 enum class Child {Timer0,Timer1,Timer2,Timer3,Timer4,SlowControls,PanelInput,ModeStatus,ChannelMirror,Display,KeyEvents,MidiOutput,VoiceFeedback,Arpeggiator,GlobalState,HardwareInput,PartFeedback};
 enum class Stage {Dispatch,AwaitChild,NextCycle};
 Stage stage()const{return stage_;}unsigned index()const{return index_;}
 Child child()const{return Child(index_);}uint64_t cyclesCompleted()const{return completed_;}
 unsigned nextClocks()const{return stage_==Stage::AwaitChild?0:stage_==Stage::NextCycle?3:index_==7||index_==8?15:20;}
 bool resumeChild(){if(stage_!=Stage::AwaitChild)return false;if(++index_==17)stage_=Stage::NextCycle;else stage_=Stage::Dispatch;return true;}
 unsigned advanceBefore(unsigned available){const auto cost=nextClocks();if(!cost||available<cost)return 0;
  if(stage_==Stage::Dispatch)stage_=Stage::AwaitChild;else {++completed_;index_=0;stage_=Stage::Dispatch;}return cost;}
private:Stage stage_=Stage::Dispatch;unsigned index_=0;uint64_t completed_=0;
};
}
