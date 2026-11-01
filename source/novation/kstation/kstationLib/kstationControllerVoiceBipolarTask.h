#pragma once
#include "kstationVoiceUpdateTask.h"
namespace kstation {
class ControllerVoiceBipolarTask final {
 using T=VoiceParameterBipolarTask;using S=T::Stage;
public:
 ControllerVoiceBipolarTask(uint32_t patch,unsigned lane=0):task_(patch,lane){}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}uint32_t word()const{return task_.word();}bool resumeDsp(){return task_.resumeDsp();}bool stackWrites()const{return task_.stage()==S::DispatchDsp;}
 unsigned nextClocks()const{auto table=task_.stage()==S::ReadTable;auto a=table?task_.tableAddress():task_.valueAddress();auto width=table?2u:1u;T::Credits c{bus(a,width),10};return task_.nextClocks(c);}
 template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;auto table=task_.stage()==S::ReadTable;auto a=table?task_.tableAddress():task_.valueAddress();auto width=table?2u:1u;T::Credits c{bus(a,width),10};return task_.advanceBefore(cost,c,[&](bool){return read(a&0xffffff,width,cost);});}
private:static unsigned bus(uint32_t a,unsigned n){int p=-1;unsigned c=0;for(unsigned i=0;i<n;++i){auto x=(a+i)&0xffffff;if(x<0x400000||(x>=0xfffd10&&x<0xffffa0)){if((p&1)||x!=uint32_t(p|1))c+=x<0x400000?3:1;p=int(x);}else{c+=x>=0xffffa0?1:3;p=-1;}}return c;}T task_;
};
}
