#pragma once
#include "ksNativeArpRandomService.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeArpRandom(A&a,const NativeArpRandomService&s){if(!s.complete())throw std::logic_error("Incomplete arp random projection");const auto&v=s.summary();a.values[6]=uint32_t(v.index)*0x10001u;const auto flags=controlArithmetic::add(int16_t(uint16_t(v.product)),int16_t(v.increment),a.flags).flags;a.flags=uint8_t((flags&0xf1)|(v.index&0x8000?8:!v.index?4:0));}
}
