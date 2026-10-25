#pragma once
#include "ksNativeArpEmitService.h"
#include "ksNativeArpGateAbi.h"
#include "ksNativeArpEmissionAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeArpEmit(A&a,const NativeArpEmitService&s){if(!s.complete())throw std::logic_error("Incomplete arp emit projection");const auto&v=s.summary();a.values[0]=0x160d2u+334*v.input.part;a.values[1]=(a.values[1]&0xffff0000u)|v.transpose;a.values[5]=(a.values[5]&0xffff0000u)|v.playedNoteVelocity;a.flags=controlArithmetic::add(int8_t(uint8_t(v.input.noteVelocity)),int8_t(uint8_t(v.transpose)),a.flags).flags;if(s.append())projectNativeArpGate(a,*s.append());if(s.emission())projectNativeArpEmission(a,*s.emission(),v.retainedNoteVelocity);else{a.values[5]=(a.values[5]&0xffff0000u)|v.retainedNoteVelocity;const auto note=uint8_t(v.retainedNoteVelocity);a.flags=uint8_t((a.flags&0xf1)|(note&128?8:!note?4:0));}}
}
