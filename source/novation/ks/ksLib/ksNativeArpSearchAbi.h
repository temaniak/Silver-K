#pragma once
#include "ksNativeArpSearchService.h"
#include "ksNativeArpEmitAbi.h"
#include "ksNativeKeyAbi.h"
namespace ks {
template<class A>void projectNativeArpSearch(A&a,const NativeArpSearchService&s){if(!s.complete())throw std::logic_error("Incomplete arp search projection");const auto&v=s.summary();if(s.keys()){projectNativeKey(a,*s.keys());const auto key=v.candidate;a.flags=uint8_t((a.flags&0xf1)|(key&128?8:!key?4:0));}if(s.emit()){a.values[0]=0x160d2u+334*v.part;a.values[4]=a.values[0]+v.candidate;a.values[5]=(a.values[5]&0xffff0000u)|(unsigned(v.velocity)<<8)|v.candidate;a.flags=uint8_t((a.flags&0xd0)|(v.velocity&128?8:!v.velocity?4:0));projectNativeArpEmit(a,*s.emit());}}
}
