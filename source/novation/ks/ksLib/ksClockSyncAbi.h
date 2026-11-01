#pragma once
#include "ksClockSyncService.h"
#include "ksControllerDspAbi.h"
namespace ks {
// One combinational projection at an external return; never stored in a job.
template<class A>void projectClockSync(A&a,const ClockSyncService&s){
 if(!s.complete())throw std::logic_error("Clock-sync ABI requires completed work");
 const auto&v=s.summary();const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};const auto byte=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};
 const auto nz=[](uint32_t x,unsigned bits){const auto sign=uint32_t(1)<<(bits-1);const auto mask=bits==32?0xffffffffu:(uint32_t(1)<<bits)-1;x&=mask;return uint8_t(x&sign?8:x?0:4);};
 const auto sub=[](uint32_t x,uint32_t y){const auto z=x-y;const auto borrow=(~x&y)|(~x&z)|(y&z);return uint8_t((borrow&0x80000000u?1:0)|(borrow&0x08000000u?32:0)|(((x^y)&(x^z)&0x80000000u)?2:0)|(z&0x80000000u?8:z?0:4));};
 if(v.divider&1){byte(a.values[0],v.divider);a.flags=uint8_t((a.flags&0xf1)|(v.divider&128?8:0));return;}
 if(v.internal)a.values[0]=v.internalPeriod;
 if(v.external)a.values[0]=v.correction;
 a.values[0]=(v.coefficient&0xffff0000u)|(a.values[0]&0xff00u)|v.active;
 if(v.gainObserved)word(a.values[0],uint16_t(v.gain==0x800?0x7ff:v.gain));
 a.values[1]=v.coefficient;a.values[2]=v.normalized;
 if(v.coefficientSent){a.values[2]=v.difference;word(a.values[4],4);a.flags=uint8_t((a.flags&0xd0)|sub(v.difference,0x1000));a.flags=uint8_t((a.flags&0xf1)|nz(v.coefficient,32));projectControllerDsp(a,*s.transaction(ClockSyncService::Component::Coefficient));}
 const auto quantized=v.publishPeriod&0xfffff000u;a.values[1]=quantized;a.values[2]=v.previousPeriod;a.flags=uint8_t((a.flags&0xd0)|sub(v.previousPeriod,quantized));
 if(v.periodSent){
  const auto scaled=quantized<<2;a.values[1]=scaled;word(a.values[4],3);a.flags=uint8_t((a.flags&0xf0)|((quantized&0x40000000u)?1:0));
  projectControllerDsp(a,*s.transaction(ClockSyncService::Component::PeriodAddress));
  a.values[1]=scaled;word(a.values[4],3);a.flags=uint8_t(a.flags&0xf1);
  projectControllerDsp(a,*s.transaction(ClockSyncService::Component::PeriodValue));
 }
}
}
