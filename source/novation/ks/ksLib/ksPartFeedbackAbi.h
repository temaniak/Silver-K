#pragma once
#include "ksPartFeedbackService.h"
#include "ksControllerDspAbi.h"
#include "ksGlobalUpdateForeground.h"
#include "ksControlArithmetic.h"
namespace ks {
// Final external compatibility edge; none of this projected state participates
// in a running feedback, DSP or global-update child.
template<class A>void projectPartFeedback(A&a,const PartFeedbackService&s){
 if(!s.complete())throw std::logic_error("Incomplete part feedback projection");
 const auto&v=s.summary();const auto saved=uint16_t((a.values[0]&0xff00u)|v.current);
 const auto word=[](uint32_t&d,uint16_t w){d=(d&0xffff0000u)|w;};
 const auto nz16=[](uint16_t x){return uint8_t((x&0x8000?8:0)|(!x?4:0));};
 for(unsigned n=0;n<4;++n){
  const auto address=uint16_t(0x8041+n);const auto index=uint16_t(((4-n)<<8)|n);
  word(a.values[0],index);word(a.values[4],address);
  if(!s.status(n))throw std::logic_error("Missing part status outcome");projectControllerDsp(a,*s.status(n));
  a.flags=controlArithmetic::subtract(int8_t(v.status[n]),int8_t(3),a.flags).flags;
  if(v.refreshed&(1u<<n)){
   if(!s.clear(n)||!s.global(n))throw std::logic_error("Missing refreshed part outcome");
   word(a.values[4],address);a.values[1]=0;a.flags=uint8_t((a.flags&0xd0)|4);projectControllerDsp(a,*s.clear(n));
   word(a.values[4],address);word(a.values[0],uint16_t((unsigned(v.base[n])<<8)|n));
   a.flags=controlArithmetic::add(int8_t(n),int8_t(v.base[n]),a.flags).flags;
   a.values[0]=(a.values[0]&0xffffff00u)|v.part[n];
   GlobalUpdateForeground::project(a,*s.global(n));
  }
  word(a.values[4],uint16_t(address+1));word(a.values[0],uint16_t(((3-n)<<8)|(n+1)));
  a.flags=uint8_t((a.flags&0xf1)|nz16(uint16_t(a.values[0])));
 }
 word(a.values[0],saved);a.flags=uint8_t((a.flags&0xf1)|(v.current&128?8:0)|(!v.current?4:0));
}
}
