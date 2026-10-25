#pragma once
#include "ksNativeMonoArpBootstrapService.h"
#include "ksPanelMasksAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeMonoArpBootstrap(A&a,const NativeMonoArpBootstrapService&s){if(!s.complete()||!s.masks())throw std::logic_error("Incomplete mono bootstrap projection");const auto&v=s.summary();if(v.delayed){a.values[1]=v.deadline;a.values[5]=v.delta;}a.values[0]=v.input.record;a.values[4]=v.input.stream;
 a.values[1]=(a.values[1]&0xffff0000u)|(uint32_t(v.mode)<<8)|v.enabled;const auto bit=uint8_t(1u<<(uint8_t(v.selected-v.mode)&7));a.values[2]=(a.values[2]&0xffffff00u)|(bit&v.enabled);a.flags=controlArithmetic::subtract(int8_t(v.selected),int8_t(v.mode),a.flags).flags;a.flags=uint8_t((a.flags&0xf1)|((bit&v.enabled)&128?8:(bit&v.enabled)?0:4));const auto before=a.values[0];projectPanelMasks(a,*s.masks());a.values[0]=before;a.values[4]=v.input.stream;a.flags=uint8_t((a.flags&0xf1)|(int32_t(v.input.record)<0?8:!v.input.record?4:0));
}
}
