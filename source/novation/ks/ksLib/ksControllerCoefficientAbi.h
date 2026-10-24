#pragma once
#include "ksControllerCoefficientService.h"
namespace ks {
// Called once at the external return, after the semantic calculation finishes.
template<class A>void projectControllerCoefficient(A&a,const ControllerCoefficientService::Summary&s){
 const auto prepared=prepareParameterScale(s.encoded);const auto magnitude=uint32_t(s.encoded<0?-int(s.encoded):int(s.encoded));const auto exponent=(s.encoded==-32768?32767u:magnitude)>>10;
 const unsigned count=unsigned(prepared.shift<0?-prepared.shift:prepared.shift);
 bool half=((exponent&15u)+5u)>15u;bool carry=exponent>=11u;
 if(prepared.shift>0)carry=false;else if(prepared.shift<0)carry=bool((prepared.mantissa>>(count-1))&1);
 uint8_t valueFlags=uint8_t(!s.encoded?4:0);
 if(s.encoded<0){const auto positive=uint32_t(-int64_t(s.coefficient));const auto negative=uint32_t(s.coefficient);half=bool((positive|negative)&0x08000000u);carry=positive!=0;valueFlags=uint8_t(negative&0x80000000u?8:negative?0:4);}
 a.values[0]=uint32_t(s.coefficient);a.values[1]=(uint32_t(uint16_t(s.encoded))<<16)|(a.values[1]&0xff00u);a.flags=uint8_t((a.flags&0xd0)|(half?32:0)|(carry?1:0)|valueFlags);
}
}
