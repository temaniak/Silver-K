#pragma once
#include "kstationVoiceUpdateTask.h"
namespace kstation {
class ControllerVoiceLinearTask final {
 using T=VoiceParameterLinearTask;using S=T::Stage;
public:
 ControllerVoiceLinearTask(uint32_t patch,unsigned lane,unsigned component):task_(patch,lane,component){}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}uint32_t word()const{return task_.word();}bool resumeDsp(){return task_.resumeDsp();}bool stackWrites()const{return task_.stage()==S::DispatchDsp;}unsigned nextClocks()const{auto a=task_.valueAddress()&0xffffff;T::Credits c{a<0xfffd10?3u:1u,10};return task_.nextClocks(c);}template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;auto a=task_.valueAddress()&0xffffff;T::Credits c{a<0xfffd10?3u:1u,10};return task_.advanceBefore(cost,c,[&](){return read(a,1,cost);});}
private:T task_;
};
}
