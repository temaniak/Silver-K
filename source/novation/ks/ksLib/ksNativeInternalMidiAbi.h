#pragma once
#include "ksNativeInternalMidiService.h"
#include "ksNativeMidiHandlerAbi.h"
#include "ksNativeKeyAbi.h"
namespace ks {
template<class A>void projectNativeInternalMidi(A&a,const NativeInternalMidiService&s){if(!s.complete())throw std::logic_error("Incomplete internal decoder projection");const auto&f=s.summary();const auto&t=s.decodeTask();const auto low=[](uint32_t&v,uint8_t n){v=(v&0xffffff00u)|n;};const auto word=[](uint32_t&v,uint16_t n){v=(v&0xffff0000u)|n;};const auto nz32=[](uint32_t n){return uint8_t((n&0x80000000u?8:0)|(!n?4:0));};
 if(f.input.byte>=128){word(a.values[1],uint16_t((unsigned(f.descriptorFlags&2)<<8)|t.length()));a.flags=uint8_t(a.flags&0xd0);if(!f.held){a.values[0]=0x15c28;low(a.values[6],t.channel());a.flags=uint8_t((a.flags&0xf1)|nz32(a.values[0]));return;}}
 else{low(a.values[0],t.remaining());if(!f.cursorObserved){a.flags=uint8_t((a.flags&0xf1)|4);return;}if(!f.held){a.values[1]=t.cursor();a.flags=uint8_t((a.flags&0xf1)|nz32(a.values[1]));return;}}
 projectNativeKey(a,*s.heldOutcome());a.values[0]=f.handlerDataBinding;a.values[4]=t.payload();a.flags=uint8_t((a.flags&0xd0)|nz32(a.values[0]));if(f.dispatched)projectNativeMidiHandler(a,*s.handlerOutcome());
}
}
