#pragma once
#include "ksPeriodicService.h"
#include "ksControllerDspAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectPeriodic(A&a,const PeriodicService&s){
 if(!s.complete())throw std::logic_error("Incomplete periodic projection");const auto&v=s.summary();
 const auto nz16=[](uint16_t n){return uint8_t((n&0x8000?8:0)|(!n?4:0));};
 if(s.kind()==PeriodicService::Kind::Mark){const auto flags=controlArithmetic::subtract(int8_t(v.mode),int8_t(1),a.flags).flags;const auto n=a.values[6];a.flags=uint8_t((flags&0xf1)|(n&0x80000000u?8:0)|(!n?4:0));return;}
 if(s.kind()==PeriodicService::Kind::Triggered&&!v.trigger){a.flags=uint8_t((a.flags&0xf1)|nz16(uint16_t(a.values[0])));return;}
 uint8_t flags=controlArithmetic::subtract(int8_t(v.mode),int8_t(s.kind()==PeriodicService::Kind::Fast?0:s.kind()==PeriodicService::Kind::Triggered?1:2),a.flags).flags;
 if(v.children){const auto*last=s.value(4);if(!last)throw std::logic_error("Missing periodic value outcome");if(last->dsp){const auto before=a.values;projectControllerDsp(a,*last->dsp);flags=a.flags;a.values=before;}else flags=controlArithmetic::subtract(int8_t(last->value.mode()),int8_t(6),a.flags).flags;}
 a.flags=uint8_t((flags&0xf1)|nz16(uint16_t(a.values[0])));
}
}
