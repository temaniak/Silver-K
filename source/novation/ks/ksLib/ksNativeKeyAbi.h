#pragma once
#include "ksNativeKeyService.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeKey(A&a,const NativeKeyService&s){
 if(!s.complete())throw std::logic_error("Incomplete native key projection");const auto&r=s.summary();using K=NativeKeyService::Kind;
 const auto nz8=[](uint8_t x){return uint8_t((x&128?8:0)|(!x?4:0));};const auto nz16=[](uint16_t x){return uint8_t((x&0x8000?8:0)|(!x?4:0));};const auto nz32=[](uint32_t x){return uint8_t((x&0x80000000u?8:0)|(!x?4:0));};
 const auto half=[](uint8_t x,uint8_t y){return uint8_t(controlArithmetic::subtract(int8_t(x),int8_t(y),uint8_t(0)).flags&33);};
 if(r.kind==K::Physical||r.kind==K::Held){const auto&t=*r.activityTask;const uint32_t base=0x160d2u+334*t.part();a.values[0]=base;a.values[1]=(t.remaining()<<16)|t.result();a.values[2]=base+(r.kind==K::Held?208:192)+2*t.cursor();a.flags=uint8_t((a.flags&0xd0)|nz16(t.result()));}
 else if(r.kind==K::ResetHeld||r.kind==K::ResetDown)a.flags=uint8_t((a.flags&0xd0)|nz32(a.values[0]));
 else if(r.kind==K::Compact){const auto&t=*r.compactTask;a.values[0]=0x160d2u+334*t.part();if(t.position())a.values[1]=a.values[0]+240+2*t.position();a.values[2]=(a.values[2]&0xffffff00u)|t.remaining();a.flags=uint8_t((a.flags&0xd0)|(t.position()?half(r.lastCompared,uint8_t(r.input.noteVelocity)):0)|nz16(uint16_t(a.values[3])));}
 else if(r.kind==K::Priority){const auto&t=*r.priorityTask;uint8_t hc=0;if(t.replacement()>=128)hc=half(uint8_t(r.input.noteVelocity),t.lower());a.values[5]=(a.values[5]&0xffff0000u)|r.noteVelocity;a.flags=uint8_t((a.flags&0xd0)|hc|nz32(a.values[0]));}
 else {const auto&t=*r.searchTask;const auto c=t.cursor();const bool found=c.candidate>=int(c.group*8)&&c.candidate<int(c.group*8+8);const uint8_t candidate=t.terminalKey();a.values[1]=r.bitmap+c.storageIndex+(found?0:c.forward?1u:unsigned(-1));a.values[2]=(a.values[2]&0xffffff00u)|uint8_t(c.observedByte);a.values[4]=(a.values[4]&0xffff0000u)|(unsigned(found?candidate%8:c.forward?255:8)<<8)|candidate;
  if(found){const uint8_t hc=c.forward?33:c.group!=uint8_t(r.input.noteVelocity)/8?half(uint8_t((c.group+1)*8),8):0;a.flags=uint8_t((a.flags&0xd0)|hc|nz8(candidate));}else a.flags=c.forward?controlArithmetic::add(int8_t(c.group*8),int8_t(8),a.flags).flags:controlArithmetic::subtract(int8_t(c.group*8),int8_t(8),a.flags).flags;
 }
}
}
