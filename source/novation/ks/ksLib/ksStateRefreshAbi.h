#pragma once
#include "ksStateRefreshService.h"
#include "ksControllerDspAbi.h"
#include "ksStateDeferredPartsAbi.h"
#include "ksClockSyncAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
// Combinational composition at the external return only. Live jobs never own
// this ABI value or use it as an execution continuation.
template<class A>void projectStateRefresh(A&a,const StateRefreshService&s){
 if(!s.complete()||!s.packet()||!s.deferred()||!s.clock())throw std::logic_error("Incomplete refresh projection");
 auto&v=a.values;const auto word=[](uint32_t&x,uint16_t y){x=(x&0xffff0000u)|y;};
 const auto nz8=[](uint8_t x){return uint8_t((x&128?8:0)|(!x?4:0));};const auto nz32=[](uint32_t x){return uint8_t((x&0x80000000u?8:0)|(!x?4:0));};
 v[1]=uint32_t(s.summary().refreshFlags)<<8;word(v[4],0x8056);a.flags=uint8_t((a.flags&0xd0)|8);
 projectControllerDsp(a,*s.packet());v[0]=(v[0]&0xffffff00u)|s.summary().dirty;a.flags=uint8_t((a.flags&0xf1)|nz8(s.summary().dirty));
 if(s.summary().panelRefreshed){const auto*p=s.panel();if(!p||!p->complete())throw std::logic_error("Missing refresh panel outcome");
  v[0]=p->current();v[1]=p->pointer();v[2]=(p->lastResolvedBase()&0xffff0000u)|p->bounds();v[3]=(p->lastBitSelectorReference()&0xffff0000u)|p->bounds();word(v[4],0x8046);v[5]=0x60010e;word(v[6],uint16_t((p->panelTask().value()<<8)|21u));
  const auto&f=s.panelFacts();const auto mode=f.indicator.mode();uint8_t hc=0;if(mode==3)hc=(f.input>>4)&1;else if(mode!=1&&mode!=2){const auto limit=f.indicator.limit();hc=controlArithmetic::subtract(int8_t(f.input),int8_t(limit),0).flags&0x21;const auto amount=f.input>limit?limit:f.input;if(int8_t(amount)>0)hc=uint8_t((hc&32)|(amount<=8?((unsigned(f.set)>>(8-amount))&1):0));}a.flags=uint8_t((a.flags&0xd0)|hc|nz32(v[0]));
 }
 projectStateDeferredParts(a,*s.deferred());projectClockSync(a,*s.clock());
}
}
