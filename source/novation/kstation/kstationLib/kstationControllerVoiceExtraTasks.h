#pragma once
#include "kstationVoiceUpdateTask.h"
namespace kstation {
class ControllerVoiceQuarterTask final {
 using T=VoiceParameterQuarterTask;using S=T::Stage;
public:ControllerVoiceQuarterTask(uint32_t patch,unsigned index):task_(patch,index){}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}uint32_t word()const{return task_.word();}bool resumeDsp(){return task_.resumeDsp();}bool stackWrites()const{return task_.stage()==S::DispatchDsp;}unsigned nextClocks()const{T::Credits c{(task_.valueAddress()&0xffffff)<0xfffd10?3u:1u,10};return task_.nextClocks(c);}template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;auto a=task_.valueAddress()&0xffffff;T::Credits c{a<0xfffd10?3u:1u,10};return task_.advanceBefore(cost,c,[&](){return read(a,1,cost);});}private:T task_;
};
class ControllerVoiceTripleTask final {
 using T=VoiceParameterTripleTask;using S=T::Stage;
public:explicit ControllerVoiceTripleTask(uint32_t patch):task_(patch){}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}uint32_t word()const{return task_.word();}bool resumeDsp(){return task_.resumeDsp();}bool stackWrites()const{auto s=task_.stage();return s==S::DispatchFirst||s==S::DispatchSecond||s==S::DispatchThird;}unsigned nextClocks()const{uint32_t a=0;unsigned n=0;access(a,n);T::Credits c{n?bus(a,n):0,10};return task_.nextClocks(c);}template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;uint32_t a=0;unsigned n=0;access(a,n);T::Credits c{n?bus(a,n):0,10};return task_.advanceBefore(cost,c,[&](T::Read){return read(a&0xffffff,n,cost);});}
private:static unsigned bus(uint32_t a,unsigned n){int p=-1;unsigned c=0;for(unsigned i=0;i<n;++i){auto x=(a+i)&0xffffff;if(x<0x400000||(x>=0xfffd10&&x<0xffffa0)){if((p&1)||x!=uint32_t(p|1))c+=x<0x400000?3:1;p=int(x);}else{c+=x>=0xffffa0?1:3;p=-1;}}return c;}void access(uint32_t&a,unsigned&n)const{switch(task_.stage()){case S::ReadFirst:a=task_.patch()+0x2d;n=1;break;case S::ReadSecond:a=task_.patch()+0x2c;n=1;break;case S::ReadThird:a=task_.patch()+0x2e;n=1;break;case S::ReadOffset:a=0xbf9c;n=2;break;default:break;}}T task_;
};
}
namespace kstation {
class ControllerVoiceFiveTask final {
 using T=VoiceParameterFiveTask;using S=T::Stage;
public:explicit ControllerVoiceFiveTask(uint32_t patch):task_(patch){}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}uint32_t word()const{return task_.word();}bool resumeDsp(){return task_.resumeDsp();}bool stackWrites()const{auto s=task_.stage();return s==S::DispatchFirst||s==S::DispatchSecond||s==S::DispatchThird||s==S::DispatchFourth||s==S::DispatchFifth;}unsigned nextClocks()const{auto a=address()&0xffffff;T::Credits c{a<0xfffd10?3u:1u,10};return task_.nextClocks(c);}template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;auto a=address()&0xffffff;T::Credits c{a<0xfffd10?3u:1u,10};return task_.advanceBefore(cost,c,[&](T::Read){return read(a,1,cost);});}
private:uint32_t address()const{auto s=task_.stage();return task_.patch()+(s==S::ReadFirst?0x2f:s==S::ReadSecond?0x2b:s==S::ReadThird?0x82:s==S::ReadFourth?0x38:0x39);}T task_;
};
}
