#pragma once
#include "ksNativeMidiControlService.h"
#include "ksNativeVoiceModulationAbi.h"
#include "ksNativeVoiceSweepAbi.h"
#include "ksNativeVoiceReleaseAbi.h"
#include "ksNativePedalControlAbi.h"
#include "ksNativeKeyAbi.h"
namespace ks {
template<class A>void projectNativeMidiControl(A&a,const NativeMidiControlService&s){if(!s.complete())throw std::logic_error("Incomplete MIDI control projection");const auto&r=s.summary();const auto&t=s.controlTask();using K=NativeMidiControlService::Kind;const auto low=[](uint32_t&v,uint8_t n){v=(v&0xffffff00u)|n;};const auto word=[](uint32_t&v,uint16_t n){v=(v&0xffff0000u)|n;};const auto nz8=[](uint8_t n){return uint8_t((n&128?8:0)|(!n?4:0));};
 if(r.kind==K::Pitch){word(a.values[0],uint16_t((unsigned(t.value())<<8)|t.value()));word(a.values[5],t.argument());a.flags=uint8_t((a.flags&0xd0)|4);projectNativeVoiceModulation(a,*s.modulationOutcome());return;}
 if(r.kind==K::Pressure){low(a.values[0],t.first());low(a.values[5],t.argument());a.flags=uint8_t((a.flags&0xf1)|nz8(t.argument()));projectNativeVoiceModulation(a,*s.modulationOutcome());return;}
 word(a.values[0],uint16_t((unsigned(t.controller())<<8)|t.second()));
 if(r.modulated){low(a.values[5],t.argument());a.flags=uint8_t((a.flags&0xd0)|nz8(t.argument()));projectNativeVoiceModulation(a,*s.modulationOutcome());return;}
 if(t.controller()==32){low(a.values[5],t.argument());a.values[0]=0x160d2u+334*t.part();a.flags=uint8_t((a.flags&0xd0)|nz8(t.argument()));return;}
 if(r.swept){a.flags=uint8_t((a.flags&0xd0)|4);projectNativeVoiceSweep(a,*s.sweepOutcome());return;}
 if(r.released){a.flags=uint8_t((a.flags&0xd0)|4);projectNativeVoiceRelease(a,*s.releaseOutcome());projectNativeKey(a,*s.keyOutcome());return;}
 if(r.pedaled){projectNativePedalControl(a,*s.pedalOutcome());return;}
 a.flags=t.controller()==121?uint8_t((a.flags&0xd0)|4):uint8_t((a.flags&0xd0)|nz8(t.value()));
}
}
