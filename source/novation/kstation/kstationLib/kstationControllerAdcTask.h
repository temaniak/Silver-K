#pragma once
#include "kstationAdcService.h"
#include <array>
#include <cstdint>
namespace kstation {
// Board-resource binding for the typed ADC interrupt and its sample child.
// The frame-timing table contains semantic costs only, not code addresses.
class ControllerAdcTask final {
 using T=AdcInterruptTask;using S=T::Stage;using A=AdcSampleTask;using C=A::Stage;
 struct Access{uint32_t address=0;unsigned width=0;};
public:
 bool complete()const{return task_.complete();}bool sampling()const{return sampling_;}const T&task()const{return task_;}bool stackWrites()const{if(sampling_)return false;auto k=stackKind(task_.stage());return k==1||k==3||k==5||k==6;}
 unsigned nextClocks(int writeHistory=-1)const{Access r{},w{};access(r,w);if(sampling_){A::Credits c{bus(r.address,r.width,-1),bus(w.address,w.width,writeHistory),7};return sample_.nextClocks(c);}T::Credits c{bus(r.address,r.width,-1),bus(w.address,w.width,writeHistory),stackCost(task_.stage())};return task_.nextClocks(c);}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,int writeHistory,Reader&&read,Writer&&write){Access r{},w{};access(r,w);const auto cost=nextClocks(writeHistory);if(!cost||cost>available)return 0;const unsigned rb=bus(r.address,r.width,-1),wb=bus(w.address,w.width,writeHistory);const bool postRead=sampling_&&(sample_.stage()==C::ReadHigh||sample_.stage()==C::ReadLow);const unsigned endRead=cost-(postRead?2:0);auto readValue=[&]()->uint32_t{uint32_t v=0;int previous=-1;unsigned cursor=endRead-rb;for(unsigned i=0;i<r.width;++i){cursor+=one((r.address+i)&0xffffff,previous);v=(v<<8)|uint8_t(read((r.address+i)&0xffffff,cursor));}return v;};auto writeValue=[&](uint32_t v){int previous=writeHistory;unsigned cursor=cost-wb;for(unsigned i=0;i<w.width;++i){cursor+=one((w.address+i)&0xffffff,previous);write((w.address+i)&0xffffff,uint8_t(v>>(8*(w.width-i-1))),cursor);}};
 if(sampling_){A::Credits c{rb,wb,7};auto used=sample_.advanceBefore(cost,c,[&](A::Read){return readValue();},[&](uint16_t v){writeValue(v);});if(sample_.complete()){sampling_=false;task_.resumeSample();}return used;}
 T::Credits c{rb,wb,stackCost(task_.stage())};auto used=task_.advanceBefore(cost,c,[&](T::Read){return readValue();},[&](T::Write,uint16_t v){writeValue(v);});if(task_.awaitingSample()){sample_=A{};sampling_=true;}return used;
 }
private:
 static unsigned one(uint32_t a,int&prev){a&=0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){auto cost=((prev&1)||a!=uint32_t(prev|1))?(a<0x400000?3u:1u):0u;prev=int(a);return cost;}prev=-1;return a>=0xffffa0?1:3;}
 static unsigned bus(uint32_t a,unsigned width,int prev){unsigned total=0;for(unsigned i=0;i<width;++i)total+=one((a+i)&0xffffff,prev);return total;}
 static uint8_t stackKind(S stage){constexpr std::array<uint8_t,138> kinds{{1,0,0,0,0,0,0,0,0,0,0,2,8,1,3,3,1,3,5,4,2,4,4,2,2,8,0,0,0,1,3,3,1,3,5,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,7,0,0,0,0,0,0,0,0,0,6,0,0,0,0,0,0,7,0,0,2,4,4,2,2,8}};auto i=unsigned(stage);return i<kinds.size()?kinds[i]:0;}
 static unsigned stackCost(S stage){switch(stackKind(stage)){case 1:case 2:return 6;case 3:case 4:case 6:return 10;case 5:return 5;case 7:case 8:return 7;default:return 0;}}
 void access(Access&r,Access&w)const{if(sampling_){switch(sample_.stage()){case C::ReadGroup:r={0xc490,1};break;case C::ReadHigh:r={sample_.adcAddress(),1};break;case C::ReadLow:r={sample_.adcAddress()+1,1};break;case C::ReadPrevious:r={sample_.sampleAddress(),2};break;case C::WriteSample:w={sample_.sampleAddress(),2};break;default:break;}return;}switch(task_.stage()){case S::ReadRemaining:r={0xc491,1};break;case S::ReadBusy:case S::ReadStatus:r={0xffffe8,1};break;case S::ReadGroup:case S::ReadScaleGroup:case S::ReadNextGroup:case S::ReadMuxGroup:r={0xc490,1};break;case S::ReadCurrent:r={task_.currentAddress(),2};break;case S::ReadHistory:r={task_.historyAddress(),2};break;case S::ReadArmed:r={task_.armedAddress(),1};break;case S::ReadMapping:r={task_.mappingAddress(),1};break;case S::ReadProducer:r={0xc1cc,2};break;case S::WriteRemaining:case S::WriteSettleCount:w={0xc491,1};break;case S::WriteBusy:case S::WriteStatus:case S::WriteConversion:w={0xffffe8,1};break;case S::WriteArmed:w={task_.armedAddress(),1};break;case S::WriteHistory:w={task_.historyAddress(),2};break;case S::WriteEvent:w={task_.eventAddress(),2};break;case S::WriteProducer:w={0xc1cc,2};break;case S::WriteNextGroup:w={0xc490,1};break;case S::WriteMux:w={0xffffcb,1};break;default:break;}}
 T task_{};A sample_{};bool sampling_=false;
};
}
