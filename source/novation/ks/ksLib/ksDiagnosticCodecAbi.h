#pragma once
#include "ksDiagnosticCodecService.h"
namespace ks {
template<class A>void projectDiagnosticCodec(A&a,const DiagnosticCodecService&s){
 if(!s.complete())throw std::logic_error("Incomplete codec projection");using K=DiagnosticCodecService::Kind;const auto&r=s.result();const auto&i=s.input();auto&v=a.values;
 const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto high=[](uint32_t&x,uint8_t n){x=(x&0xffff00ffu)|(uint32_t(n)<<8);};const auto nz16=[](uint16_t n){return uint8_t((n&0x8000?8:0)|(!n?4:0));};
 uint8_t hc=a.flags&0x21,nz=0;
 if(s.kind()==K::Prefix){v[1]=0x1b6;high(v[0],r.byte);low(v[0],0);nz=4;}
 else if(s.kind()==K::Word){word(v[1],uint16_t(r.payload));high(v[0],r.byte);hc&=0x20;nz=nz16(uint16_t(v[0]));}
 else if(s.kind()==K::Long){const auto shifted=uint32_t((i.payload>>14)<<2);v[1]=(shifted&0xffff0000u)|(shifted>>16);high(v[0],r.byte);low(v[0],0);hc&=0x20;nz=nz16(uint16_t(v[0]));}
 else {v[1]=r.packed;v[2]=(uint32_t(r.carry)<<16)|r.weight;high(v[0],r.byte);if(s.kind()!=K::Tail){v[3]=r.sum;v[6]=r.source;}hc=s.kind()==K::Group||s.kind()==K::Block?0:s.kind()==K::Sample?uint8_t(((uint64_t(r.lastSumBefore&0xfffffff)+r.sample)>0xfffffff)?32:0):uint8_t(hc&0x20);if(s.kind()==K::Block){v[0]&=0xffffu;nz=4;}else nz=nz16(r.carry);}
 a.flags=uint8_t((a.flags&0xd0)|hc|nz);
}
}
