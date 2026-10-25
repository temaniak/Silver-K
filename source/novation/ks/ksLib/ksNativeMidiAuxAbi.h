#pragma once
#include "ksNativeMidiAuxService.h"
#include "ksNativePedalControlAbi.h"
#include "ksNativeProgramEventAbi.h"
#include "ksSysExNativeLeavesAbi.h"
#include "ksPeriodicAbi.h"
namespace ks {
template<class A>void projectNativeMidiAux(A&a,const NativeMidiAuxService&s){if(!s.complete())throw std::logic_error("Incomplete auxiliary MIDI projection");const auto&f=s.summary();const auto&t=s.auxTask();using K=NativeMidiAuxService::Kind;const auto low=[](uint32_t&v,uint8_t n){v=(v&0xffffff00u)|n;};const auto high=[](uint32_t&v,uint8_t n){v=(v&0xffff00ffu)|(unsigned(n)<<8);};const auto word=[](uint32_t&v,uint16_t n){v=(v&0xffff0000u)|n;};const auto nz8=[](uint8_t n){return uint8_t((n&128?8:0)|(!n?4:0));};const auto move=[&](uint8_t n){a.flags=uint8_t((a.flags&0xf1)|nz8(n));};
 if(f.kind==K::Noop)return;if(f.kind==K::Parameter){projectNativePedalControl(a,*s.parameterOutcome());return;}
 if(f.kind==K::GlobalControl){word(a.values[0],uint16_t((unsigned(t.controller())<<8)|t.value()));a.flags=controlArithmetic::subtract(int8_t(t.controller()),int8_t(32),a.flags).flags;if(f.parameter){projectNativePedalControl(a,*s.parameterOutcome());return;}low(a.values[5],t.bank());low(a.values[0],t.bankMask());move(t.bankMask());if(t.bankMask()){a.values[0]=0x160d2u+334*t.part();a.flags=uint8_t((a.flags&0xd0)|nz8(t.bank()));}return;}
 if(f.kind==K::Program||f.kind==K::ConditionalProgram){a.values[0]=0x160d2u+334*t.part();high(a.values[5],t.bank());a.flags=uint8_t((a.flags&0xd0)|nz8(t.bank()));if(f.kind==K::ConditionalProgram&&t.bank()>=128)return;low(a.values[1],255);word(a.values[5],uint16_t((unsigned(t.bank())<<8)|t.program()));move(t.program());projectNativeProgramEvent(a,*s.eventOutcome());return;}
 if(f.kind==K::Start||f.kind==K::Continue){projectSysExNative(a,*s.sysExOutcome());return;}
 if(f.kind==K::Stop){projectPeriodic(a,*s.periodicOutcome());return;}throw std::logic_error("Unsupported auxiliary MIDI kind");
}
}
