#pragma once
#include "ksNativeVoiceAllocationService.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeVoiceAllocation(A&a,const NativeVoiceAllocationService&s){
 if(!s.complete()||!s.population()||!s.populationPitch()||!s.populationSelect()||!s.velocity())throw std::logic_error("Incomplete allocation projection");
 const auto&t=*s.population();const auto&v=s.summary();const auto&pitch=s.populationPitch()->summary();const auto&select=s.populationSelect()->summary();
 a.values[0]=(pitch.retried?uint32_t(pitch.remaining)<<16:0u)|(pitch.offsetValue&0xff00u);
 a.values[1]=0x10000u|select.voice;
 const auto curveAddress=t.curve()>=4?0x1534eu+s.velocity()->curve():0u;
 a.values[2]=(curveAddress&0xff00u)|v.mirrorByte;
 a.values[3]=0x17560u+20*v.slot;
 a.values[4]=(a.values[4]&0xffff0000u)|(uint32_t(t.pitch())<<8)|t.distance();
 a.values[5]=pitch.port==DspPort::A?0x400000:0x600000;a.values[6]=v.input.patch;
 const auto difference=uint8_t(t.glideNote()-t.pitch());
 const auto hc=t.glideNote()<t.pitch()?controlArithmetic::subtract(int8_t(0),int8_t(difference),0).flags:controlArithmetic::subtract(int8_t(t.glideNote()),int8_t(t.pitch()),0).flags;
 a.flags=uint8_t((a.flags&0xd0)|(hc&33)|4);
}
}
