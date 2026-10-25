#pragma once
#include "ksNativeVoiceStartService.h"
#include "ksNativeVoiceAllocationAbi.h"
#include "ksControllerDspAbi.h"
#include "ksParameterUploadForeground.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectVoiceStartParameterTail(A&a,const ParameterUploadService::Summary&s){
 if(!s.glide)throw std::logic_error("Incomplete parameter upload result");const auto&g=*s.glide;auto&v=a.values;
 auto order=s.coefficient.order;uint32_t upper=s.coefficient.observed?s.coefficient.value>>16:v[0]>>16;
 if(s.productDiscardOrder>order){upper=0;order=s.productDiscardOrder;}if(s.retry.observed&&s.retry.order>order)upper=s.retry.result.remaining;
 v[0]=(upper<<16)|0xffffu;
 const bool weighted=s.distanceObserved&&g.distance();const uint8_t tail=g.time()&&!weighted?uint8_t(g.coefficient()):0;
 v[1]=(uint32_t(g.previous())<<24)|tail;
 uint16_t lane=s.lfo[1].delaySelector?uint16_t(0x159f4u+s.lfo[1].index):uint16_t((unsigned(uint8_t((s.lfo[1].delay^127u)-115))<<8)|((s.lfo[1].delay^127u)>115?0:115));
 lane=uint16_t((unsigned(g.mode())<<8)|uint8_t(s.allocationObserved?g.allocation():lane));
 if(g.time())lane=weighted?uint16_t(unsigned(g.distance())<<8):uint16_t((unsigned(s.distanceObserved?0:g.adjustment())<<8)|(g.inverted()>118?0:118));
 v[2]=(s.presetObserved?0x10000u:v[2]&0xffff0000u)|lane;
 if(s.voiceObserved)v[3]=0x17560u+20u*s.voice;
 v[5]=0x600000;v[6]=s.input.patch;a.flags=uint8_t((a.flags&0xd0)|8);
}
template<class A>void projectNativeVoiceStart(A&a,const NativeVoiceStartService&s){
 if(!s.complete()||!s.start())throw std::logic_error("Incomplete voice start projection");const auto&t=*s.start();const auto&v=s.summary();
 const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};
 if(s.allocation())projectNativeVoiceAllocation(a,*s.allocation());a.values[6]=v.patch;
 if(t.suppressed()){a.values[0]=(a.values[0]&0xffffff00u)|t.velocity();a.flags=uint8_t((a.flags&0xf1)|(t.velocity()&128?8:t.velocity()?0:4));return;}
 if(s.parameters()){projectVoiceStartParameterTail(a,s.parameters()->summary());}
 else if(s.glide()){
  const auto&g=*s.glide();uint64_t order=0;uint32_t upper=a.values[0]>>16;
  if(v.coefficientOrder>order){order=v.coefficientOrder;upper=v.coefficient>>16;}if(v.productDiscardOrder>order){order=v.productDiscardOrder;upper=0;}if(v.retryOrder>order){upper=v.remaining;}
  a.values[0]=(upper<<16)|(a.values[0]&65535u);
  uint16_t lane=uint16_t((unsigned(g.mode())<<8)|uint8_t(v.allocationObserved?g.allocation():a.values[2]));
  if(g.time())lane=v.distanceObserved&&g.distance()?uint16_t(unsigned(g.distance())<<8):uint16_t((unsigned(v.distanceObserved?0:g.adjustment())<<8)|(g.inverted()>118?0:118));word(a.values[2],lane);
  if(v.voiceObserved)a.values[3]=0x17560u+20*v.voice;
 }
 else if(s.trackingOutput())projectControllerDsp(a,*s.trackingOutput());
 const uint8_t middle=t.middle();a.values[1]=(t.dspWord()<<8)|middle;
 a.values[0]=(a.values[0]&0xffffff00u)|t.detune();a.flags=controlArithmetic::add(int8_t(uint8_t(t.detune()+64)),int8_t(-64),a.flags).flags;
 word(a.values[4],VoiceStartTask::gateOffset());if(!s.gate())throw std::logic_error("Missing completed gate");projectControllerDsp(a,*s.gate());
 a.values[5]=v.port==DspPort::A?0x400000:0x600000;
}
}
