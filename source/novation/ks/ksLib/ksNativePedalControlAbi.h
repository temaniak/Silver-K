#pragma once
#include "ksNativePedalControlService.h"
#include "ksNativeParameterEditAbi.h"
#include "ksNativeVoiceReleaseAbi.h"
namespace ks {
template<class A>void projectNativePedalControl(A&a,const NativePedalControlService&s){if(!s.complete())throw std::logic_error("Incomplete pedal control projection");const auto&r=s.summary();const auto low=[](uint32_t&v,uint8_t n){v=(v&0xffffff00u)|n;};const auto word=[](uint32_t&v,uint16_t n){v=(v&0xffff0000u)|n;};const auto nz8=[](uint8_t n){return uint8_t((n&128?8:0)|(!n?4:0));};
 if(r.parameter){const auto&t=*s.parameterTask();if(r.input.controller==98){a.values[0]=0x160d2u+334*t.part();low(a.values[1],t.selector());a.flags=uint8_t((a.flags&0xd0)|nz8(t.selector()));return;}if(r.input.controller==6){a.values[0]=0x160d2u+334*t.part();word(a.values[6],uint16_t((unsigned(t.value())<<8)|t.selector()));a.flags=uint8_t((a.flags&0xd0)|nz8(t.selector()));if(!r.edited)return;}else word(a.values[6],uint16_t((unsigned(t.value())<<8)|t.controller()));projectNativeParameterEdit(a,*s.editOutcome());return;}
 const auto&t=*s.pedalTask();a.values[0]=0x160d2u+334*t.part();a.values[1]=t.patchReference();low(a.values[2],t.flags());word(a.values[3],uint16_t((unsigned(t.desired())<<8)|t.previous()));a.flags=uint8_t((a.flags&0xd0)|(t.flags()&128?8:0)|(!(t.flags()&4)?4:0));if(t.flags()&4)return;word(a.values[3],uint16_t((unsigned(t.changed())<<8)|t.previous()));a.flags=uint8_t((a.flags&0xd0)|nz8(t.changed()));if(!t.changed())return;low(a.values[3],t.released());a.flags=uint8_t((a.flags&0xd0)|(t.released()&128?8:0)|(!(t.released()&16)?4:0));if(r.released)projectNativeVoiceRelease(a,*r.release);
}
}
