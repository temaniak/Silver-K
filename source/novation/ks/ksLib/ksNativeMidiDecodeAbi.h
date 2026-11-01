#pragma once
#include "ksNativeMidiDecodeService.h"
#include "ksNativeMidiRouteAbi.h"
#include "ksPanelMasksAbi.h"
namespace ks {
template<class A>void projectNativeMidiDecode(A&a,const NativeMidiDecodeService&s){if(!s.complete())throw std::logic_error("Incomplete MIDI decoder projection");const auto&f=s.summary();const auto&t=s.decodeTask();const auto&r=s.result();using P=NativeMidiDecodeService::Path;const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};const auto nz8=[](uint8_t n){return uint8_t((n&128?8:0)|(!n?4:0));};const auto nz32=[](uint32_t n){return uint8_t((n&0x80000000u?8:0)|(!n?4:0));};
 if(f.input.byte>=128){low(a.values[1],6);word(a.values[1],uint16_t((unsigned(f.descriptorFlags&2)<<8)|t.length()));a.flags=uint8_t(a.flags&0xd0);if(r.path==P::DirectReturn){a.values[0]=r.dataBinding;low(a.values[6],t.channel());a.flags=uint8_t((a.flags&0xf1)|nz32(r.dataBinding));return;}a.values[0]=f.handlerDataBinding;a.flags=uint8_t((a.flags&0xf1)|nz32(a.values[0]));projectNativeMidiHandler(a,*s.immediateOutcome());return;}
 a.values[0]=(a.values[0]&0xffffff00u)|t.remaining();if(!f.cursorObserved){a.flags=uint8_t((a.flags&0xf1)|4);return;}if(r.path==P::DirectReturn){a.values[1]=t.cursor();a.flags=uint8_t((a.flags&0xf1)|nz32(t.cursor()));return;}
 a.values[1]=(f.descriptorFlags&1)?0x15c1fu:(f.observedCursor&0xffffff00u);low(a.values[1],t.channel());a.values[0]=f.handlerDataBinding;a.values[4]=0x15c1f;a.flags=uint8_t((a.flags&0xd0)|nz8(t.channel()));projectNativeMidiRoute(a,*s.routeOutcome());projectPanelMasks(a,*s.maskOutcome());
}
}
