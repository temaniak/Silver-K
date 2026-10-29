#pragma once
#include "kstationVoiceUpdateTask.h"
namespace kstation {
class ControllerVoiceParameterStartTask final {
 using T=VoiceParameterStartTask;using S=T::Stage;
public:
 ControllerVoiceParameterStartTask(uint32_t patch,unsigned group=0):task_(patch,group){}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}uint32_t word()const{return task_.word()&0xffffff;}bool resumeDsp(){return task_.resumeDsp();}bool stackWrites()const{return task_.stage()==S::DispatchOsc||task_.stage()==S::DispatchParam;}
 unsigned nextClocks()const{uint32_t a=task_.stage()==S::ReadOsc?task_.patch()+0x83+task_.group():task_.paramAddress();T::Credits c{bus(a),10};return task_.nextClocks(c);}
 template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;uint32_t a=task_.stage()==S::ReadOsc?task_.patch()+0x83+task_.group():task_.paramAddress();T::Credits c{bus(a),10};return task_.advanceBefore(cost,c,[&](T::Read){return read(a&0xffffff,1,cost);});}
private:static unsigned bus(uint32_t address){auto a=address&0xffffff;return a<0xfffd10?3:1;}T task_;
};
}
