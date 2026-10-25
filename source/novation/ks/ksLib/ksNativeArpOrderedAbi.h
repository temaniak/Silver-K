#pragma once
#include "ksNativeArpOrderedService.h"
#include "ksNativeArpRandomAbi.h"
#include "ksNativeArpEmitAbi.h"
namespace ks {
template<class A>void projectNativeArpOrdered(A&a,const NativeArpOrderedService&s){if(!s.complete()||!s.emit())throw std::logic_error("Incomplete arp ordered projection");const auto&v=s.summary();const auto base=0x160d2u+334*v.input.part;if(s.random())projectNativeArpRandom(a,*s.random());a.values[0]=base;a.values[1]=(a.values[1]&0xffff0000u)|(v.wrapped?(uint16_t(v.range)<<8)|v.publishedOctave:(uint16_t(v.index)<<8)|v.input.count);a.values[4]=((base+v.keyOffset)&0xffffff00u)|v.nextIndex;a.values[5]=(a.values[5]&0xffff0000u)|v.noteVelocity;projectNativeArpEmit(a,*s.emit());a.values[0]=base;a.flags=uint8_t(a.flags&0xf1);}
}
