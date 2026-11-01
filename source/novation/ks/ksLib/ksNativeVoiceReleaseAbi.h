#pragma once
#include "ksNativeVoiceReleaseService.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeVoiceRelease(A&a,const NativeVoiceReleaseService&s){
 if(!s.complete()||!s.voiceTask())throw std::logic_error("Incomplete native release projection");
 const auto&t=*s.voiceTask();auto&v=a.values;const uint16_t remainingPrefix=v[0]&0xff00u;
 const uint16_t match=t.mode()==VoiceReleaseTask::Mode::Note?uint16_t((unsigned(t.note())<<8)|t.part()):uint16_t((v[1]&0xff00u)|t.part());
 if(s.parameterResult())v[0]=(s.parameterReference()&0xffff0000u)|remainingPrefix;
 else if(s.releasedAny())v[0]=(s.dspRetried()?uint32_t(s.dspRemaining())<<16:0u)|remainingPrefix;
 else v[0]=(v[0]&0xffff0000u)|remainingPrefix;
 if(s.releasedAny()){
  v[1]=match;v[2]=(uint32_t(s.lastReleaseAge())<<16)|(t.wholePart()?t.cut():s.finalAgeWord());
  v[4]=(v[4]&0xffff0000u)|uint16_t(s.lastAddress()+1);v[5]=s.dspPort();
 }else {v[1]=(v[1]&0xffff0000u)|match;v[2]=(v[2]&0xffff0000u)|(t.wholePart()?t.cut():s.finalAgeWord());}
 v[3]=0x17560u+20*s.lastSlot();if(s.patchObserved())v[6]=s.lastPatch();
 if(t.wholePart()){a.flags=uint8_t((a.flags&0xd0)|(v[0]&0x80000000u?8:0)|(!v[0]?4:0));return;}
 uint8_t hc=0;using D=NativeVoiceReleaseService::Decision;
 if(s.decision()==D::Owner)hc=controlArithmetic::subtract(int8_t(s.comparedPart()),int8_t(s.comparedOwner()),0).flags&33;
 else if(s.decision()==D::Note)hc=controlArithmetic::subtract(int8_t(t.note()),int8_t(s.comparedNote()),0).flags&33;
 else if(s.decision()==D::Dsp&&!s.dspRetried())hc=controlArithmetic::subtract(int16_t(s.lastComparedAddress()),int16_t(s.lastCachedAddress()),0).flags&33;
 a.flags=uint8_t((a.flags&0xd0)|hc|4);
}
}
