#pragma once
#include "ksNativeQueuedMidiDecodeService.h"
#include "ksNativeMidiRouteAbi.h"
namespace ks {
template<class A>void projectNativeQueuedMidiDecode(A&a,const NativeQueuedMidiDecodeService&s){if(!s.complete())throw std::logic_error("Incomplete queued decoder projection");const auto&f=s.summary();const auto&t=s.byteTask();const auto&r=s.result();using P=NativeQueuedMidiDecodeService::Path;const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};const auto nz32=[](uint32_t n){return uint8_t((n&0x80000000u?8:0)|(!n?4:0));};
 if(f.input.byte>=128){word(a.values[1],uint16_t((unsigned(f.descriptorFlags&2)<<8)|t.length()));a.flags=uint8_t(a.flags&0xd0);if(r.path==P::DirectReturn){a.values[0]=0x15c13;low(a.values[6],t.channel());a.flags=uint8_t((a.flags&0xf1)|nz32(a.values[0]));return;}a.values[0]=f.handlerDataBinding;a.flags=uint8_t((a.flags&0xf1)|nz32(a.values[0]));projectNativeMidiHandler(a,*s.immediateOutcome());return;}
 low(a.values[0],t.remaining());if(!f.cursorObserved){a.flags=uint8_t((a.flags&0xf1)|4);return;}if(r.path==P::DirectReturn){a.values[1]=t.cursor();a.flags=uint8_t((a.flags&0xf1)|nz32(a.values[1]));return;}
 a.values[1]=(f.descriptorFlags&1)?0x15c13u:(f.observedCursor&0xffffff00u);low(a.values[1],t.channel());a.values[0]=f.handlerDataBinding;a.values[4]=0x15c13;a.flags=uint8_t((a.flags&0xd0)|(t.channel()&128?8:0)|(!t.channel()?4:0));projectNativeMidiRoute(a,*s.routeOutcome());
}
}
