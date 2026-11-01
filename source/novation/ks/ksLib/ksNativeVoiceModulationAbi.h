#pragma once
#include "ksNativeVoiceModulationService.h"
#include "ksControllerDspAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeVoiceModulation(A&a,const NativeVoiceModulationService&s){
 if(!s.complete())throw std::logic_error("Incomplete voice modulation projection");const auto&v=s.summary();const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};const auto value=[](uint8_t f,uint32_t x,unsigned sign){return uint8_t((f&0xf1)|(x&sign?8:x?0:4));};
 if(s.part()){const auto&p=*s.part();a.values[0]=0x174aeu+20*p.part();a.flags=uint8_t(a.flags&0xd0);if(s.kind()==NativeVoiceModulationService::Kind::Cutoff)word(a.values[1],p.word());else{if(s.kind()==NativeVoiceModulationService::Kind::Square){word(a.values[1],p.word());a.flags=uint8_t((a.flags&0xfe)|((unsigned(p.input())*p.input())&0x8000?1:0));}a.values[1]=(a.values[1]&0xffffff00u)|p.gate();a.flags=value(a.flags,p.gate(),128);}}
 if(v.mirrorObserved){a.values[0]=0x174aeu+20*v.mirrorPart+20;a.values[1]=0x17526;a.values[2]=(a.values[2]&0xff00u)|v.mirrorByte;a.flags=uint8_t((a.flags&0xd0)|4);}
 if(s.parameterOutput()){
  if(const auto*t=s.modulation();t&&t->complete()){a.values[0]=(uint32_t(t->scaled())<<16)|(a.values[0]&65535u);a.values[1]=t->converted();word(a.values[2],uint16_t(t->factor()));}
  else if(const auto*t=s.pitch();t&&t->complete()){a.values[0]=(t->wheelProduct()&0xffff0000u)|(a.values[0]&65535u);a.values[1]=t->pitchProduct();word(a.values[2],t->unisonCount()?uint16_t(int16_t(int8_t(t->spreadValue()))):uint16_t(a.values[2]&0xff00u));}
  projectControllerDsp(a,*s.parameterOutput());
 }
 if(s.refresh()){
  const auto&t=*s.refresh();if(t.kind()==VoiceRefreshTask::Kind::Filter&&t.mode()){a.values[1]=(a.values[1]&0xffffff00u)|t.mode();a.flags=value(a.flags,t.mode(),128);}
  else {word(a.values[0],uint16_t((0x174aeu+20*v.mirrorPart+20)&0xff00u));word(a.values[1],uint16_t(0x7500|t.match()));uint8_t hc=0;if(t.age()){if(t.owner()!=t.match())hc=controlArithmetic::subtract(int8_t(t.match()),int8_t(t.owner()),0).flags&33;else hc=a.flags&33;}a.flags=uint8_t((a.flags&0xd0)|hc|4);}
 }
 uint64_t order=0;if(v.voiceObserved){a.values[3]=0x17560u+20*v.voice;order=v.lastLookupOrder;}
 if(v.lastPitchGlobalOrder>order){a.values[3]=0x168fc;order=v.lastPitchGlobalOrder;}if(v.lastPitchSelectedOrder>order)a.values[3]=v.selected;
 if(s.global()&&s.global()->complete()){
  if(v.globalRetried)a.values[0]=(uint32_t(v.globalRemaining)<<16)|(a.values[0]&65535u);
  const auto&out=*s.lastOutput();a.values[1]=(out.summary().word<<8)|uint8_t(v.lastGlobalLevel*2);word(a.values[2],uint16_t(out.summary().input.address-0x1b));word(a.values[4],out.summary().input.address);projectControllerDsp(a,out);
 }
 if(v.patchObserved||s.kind()==NativeVoiceModulationService::Kind::Modulation||s.kind()==NativeVoiceModulationService::Kind::Pitch)a.values[6]=v.patch;
 if(s.lastOutput())a.values[5]=v.port==DspPort::A?0x400000:0x600000;
}
}
