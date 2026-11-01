#pragma once
#include "kstationMidiPumpTask.h"
namespace kstation {
// Typed foreground pump policy; queue/parser/reset services are explicit
// children and receive a semantic result or MIDI byte, never a CPU frame.
class ControllerMidiPumpTask final {
public:
 using Child=MidiPumpTask::Child;using Stage=MidiPumpTask::Stage;
 Stage stage()const{return task_.stage();}bool complete()const{return task_.complete();}bool awaitingChild()const{return task_.awaitingChild();}Child child()const{return task_.child();}uint8_t byte()const{return task_.byte();}bool resume(Child child,uint16_t result=0){return task_.resume(child,result);}
 bool stackWrites()const{auto s=stage();return s==Stage::PopExternal||s==Stage::ParseExternal||s==Stage::ResetExternal||s==Stage::PopInternal||s==Stage::ParseInternal||s==Stage::ResetInternal;}
 unsigned nextClocks()const{auto s=stage();if(s==Stage::AwaitChild||s==Stage::Complete)return 0;if(stackWrites())return 10;if(s==Stage::Return)return 7;return 3;}
 unsigned advanceBefore(unsigned available){auto cost=nextClocks();if(!cost||cost>available)return 0;return task_.advance()?cost:0;}
private:MidiPumpTask task_{};
};
}
