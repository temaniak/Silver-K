#pragma once
#include "ksPanelMonitorService.h"
#include "ksControlArithmetic.h"
namespace ks {
// The only compatibility projection, after the complete native job has returned.
template<class A>void projectPanelMonitor(A&a,const PanelMonitorService&s){
 if(!s.complete())throw std::logic_error("Incomplete monitor outcome");auto&v=a.values;const auto&f=s.summary();const auto&in=s.input();using K=PanelMonitorService::Kind;
 const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};
 const auto nz16=[](uint16_t x){return uint8_t((x&0x8000?8:0)|(!x?4:0));};const auto nz32=[](uint32_t x){return uint8_t((x&0x80000000u?8:0)|(!x?4:0));};
 const auto hc=f.lcd.observed?controlArithmetic::subtract(int8_t(f.lcd.reply),int8_t(20),uint8_t(0)).flags&0x21:a.flags&0x21;
 uint8_t nz=0;
 switch(s.kind()){
 case K::Clock:nz=nz32(v[0]);break;
 case K::Bar:word(v[5],uint16_t(0x500|f.barRemainder));word(v[6],f.presentation);nz=nz16(uint16_t(v[0]));break;
 case K::Percent:v[0]=f.percentResult;word(v[1],f.percentDivisor);nz=nz32(v[6]);break;
 case K::Version:case K::WithPatch:low(v[0],s.kind()==K::WithPatch?f.secondary:uint8_t(f.primary>>3));low(v[1],f.primary);low(v[5],f.secondary);v[6]=((f.versionSource+1)&0xffff0000u)|f.presentation;nz=nz16(uint16_t(v[0]));break;
 case K::Patch:low(v[0],in.patch);word(v[6],f.presentation);nz=nz16(uint16_t(v[0]));break;
 }
 a.flags=uint8_t((a.flags&0xd0)|hc|nz);
}
}
