#pragma once
#include "kstationVoiceUpdateTask.h"
namespace kstation {
class ControllerVoiceDepthLaneTask final {
 using T=VoiceParameterDepthLaneTask;using S=T::Stage;
public:
 ControllerVoiceDepthLaneTask(uint32_t patch,unsigned lane=0):task_(patch,lane){}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingWrite();}uint32_t word()const{return task_.word();}bool resumeDsp(){return task_.resumeWrite();}bool stackWrites()const{return task_.stage()==S::DispatchWord;}
 unsigned nextClocks()const{uint32_t a=0;unsigned width=0;access(a,width);T::Credits c{width?bus(a,width):0,10};return task_.nextClocks(c);}
 template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;uint32_t a=0;unsigned width=0;access(a,width);T::Credits c{width?bus(a,width):0,10};return task_.advanceBefore(cost,c,[&](T::Read){return read(a&0xffffff,width,cost);});}
private:static unsigned bus(uint32_t a,unsigned n){int p=-1;unsigned c=0;for(unsigned i=0;i<n;++i){auto x=(a+i)&0xffffff;if(x<0x400000||(x>=0xfffd10&&x<0xffffa0)){if((p&1)||x!=uint32_t(p|1))c+=x<0x400000?3:1;p=int(x);}else{c+=x>=0xffffa0?1:3;p=-1;}}return c;}
 void access(uint32_t&a,unsigned&w)const{switch(task_.stage()){case S::ReadBase:a=task_.baseAddress();w=1;break;case S::ReadFirst:a=task_.paramAddress(0);w=1;break;case S::ReadSecond:a=task_.paramAddress(1);w=1;break;case S::ReadThird:a=task_.paramAddress(2);w=1;break;case S::ReadFirstControl:a=0xbfc0;w=1;break;case S::ReadSecondControl:a=0xbfc1;w=1;break;case S::ReadThirdControl:a=0xbfc3;w=1;break;case S::ReadTable:a=task_.tableAddress();w=2;break;default:break;}}
 T task_;
};
}
