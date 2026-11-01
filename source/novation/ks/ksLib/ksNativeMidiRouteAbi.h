#pragma once
#include "ksNativeMidiRouteService.h"
#include "ksNativeMidiHandlerAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
// Firmware references exist only in this final outer-boundary projection.
inline uint32_t nativeMidiHandlerReference(NativeMidiHandler k){using K=NativeMidiHandler;switch(k){case K::PartOff:return 0x33b8;case K::PartOn:return 0x3426;case K::Control:return 0x34bc;case K::Program:return 0x3600;case K::Pressure:return 0x34b2;case K::Pitch:return 0x349c;case K::BeginSysEx:return 0x35f4;case K::EndSysEx:return 0x35fa;case K::ManualPeriodic:return 0x363c;case K::Noop:return 0x3640;case K::InternalOff:return 0xa894;case K::InternalOn:return 0xa882;case K::ConditionalProgram:return 0x361c;case K::GlobalControl:return 0x35d6;}throw std::logic_error("Unsupported final MIDI capability");}
template<class A>void projectNativeMidiRoute(A&a,const NativeMidiRouteService&s){if(!s.complete())throw std::logic_error("Incomplete MIDI routing projection");const auto&f=s.summary();const auto&t=s.routeTask();const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};const auto nz16=[](uint16_t n){return uint8_t((n&0x8000?8:0)|(!n?4:0));};const auto root=a.values[0];if((root&0xffffffu)!=nativeMidiHandlerReference(f.input.handler)||f.input.standardFallback!=(root==nativeMidiHandlerReference(f.input.handler)))throw std::logic_error("MIDI route binding capability differs at outer boundary");
 if(f.system){low(a.values[1],t.status());a.flags=controlArithmetic::subtract(int8_t(t.status()),int8_t(240),a.flags).flags;projectNativeMidiHandler(a,s.handlerOutcome(0));return;}
 a.values[6]=root;
 for(unsigned n=0;n<f.count;++n){const auto&d=f.deliveries[n];if(d.kind==NativeMidiRouteService::DeliveryKind::System)throw std::logic_error("Mixed system MIDI delivery");a.values[0]=0x174aeu+20*d.controlPart;word(a.values[1],uint16_t((unsigned(d.remaining)<<8)|d.part));word(a.values[2],uint16_t((unsigned(d.mask)<<8)|d.channel));word(a.values[3],uint16_t((unsigned(d.mode)<<8)|d.partChannel));a.values[4]=f.input.payload;a.values[5]=d.patch;a.values[6]=root;a.flags=uint8_t(a.flags&0xd0);
  if(d.kind==NativeMidiRouteService::DeliveryKind::Part){projectNativeMidiHandler(a,s.handlerOutcome(n));a.values[6]=root;a.values[5]=d.patch;a.values[4]=f.input.payload;word(a.values[3],uint16_t((unsigned(d.mode)<<8)|d.partChannel));word(a.values[2],uint16_t((unsigned(d.mask)<<8)|d.channel));word(a.values[1],uint16_t((unsigned(d.remaining)<<8)|d.part));a.flags=uint8_t((a.flags&0xf1)|nz16(uint16_t(a.values[1])));}
  else{if(d.kind==NativeMidiRouteService::DeliveryKind::FallbackProgram)a.flags=uint8_t((a.flags&0xd0)|4);else{low(a.values[0],1);a.flags=uint8_t(a.flags&0xf1);}projectNativeMidiHandler(a,s.handlerOutcome(n));if(d.kind==NativeMidiRouteService::DeliveryKind::FallbackControl){low(a.values[0],0);a.flags=uint8_t((a.flags&0xf1)|4);}word(a.values[2],uint16_t((unsigned(d.mask)<<8)|d.channel));a.flags=uint8_t((a.flags&0xf1)|nz16(uint16_t(a.values[2])));return;}
 }
 word(a.values[1],uint16_t((t.remaining()<<8)|t.index()));word(a.values[2],uint16_t((unsigned(t.mask())<<8)|t.channel()));word(a.values[3],uint16_t((unsigned(t.mode())<<8)|t.partChannel()));a.values[4]=f.input.payload;a.values[5]=t.patch();a.values[6]=root;
 if(f.cleanup){a.values[0]=0x160d2u+334*f.lastParameterPart;a.flags=uint8_t((a.flags&0xd0)|8);return;}
 if(!f.fallback){if(f.lastLookupOrder>f.lastHandlerOrder)a.values[0]=0x174aeu+20*f.lastControlPart;low(a.values[0],t.finalMode());a.flags=uint8_t((a.flags&0xd0)|4);return;}
 a.values[0]=0x174aeu+20*f.lastControlPart;word(a.values[1],0);a.flags=controlArithmetic::subtract(int8_t(t.partChannel()),int8_t(t.channel()),a.flags).flags;if(t.partChannel()!=t.channel())return;a.flags=controlArithmetic::subtract(int32_t(root),int32_t(0x34bc),a.flags).flags;a.flags=uint8_t((a.flags&0xf1)|nz16(uint16_t(a.values[2])));
}
}
