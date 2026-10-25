#pragma once
#include "ksNativeParameterSupportService.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeParameterSupport(A&a,const NativeParameterSupportService&s){if(!s.complete())throw std::logic_error("Incomplete parameter support projection");const auto&r=s.summary();using K=NativeParameterSupportService::Kind;const auto nz8=[](uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));};const auto nz16=[](uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));};
 if(r.kind==K::Map){const auto&t=*r.map;if(t.outcome()==StateFieldMapTask::Outcome::Unmapped){a.values[0]&=0xffffff00u;a.flags=uint8_t((a.flags&0xd0)|4);return;}a.values[1]=t.field();if(t.outcome()!=StateFieldMapTask::Outcome::Packed){a.values[0]=((r.descriptor+2)&0xffffff00u)|1;a.values[2]=r.shifted;const auto cmp=controlArithmetic::subtract(int8_t(t.field()),int8_t(r.shifted),a.flags).flags;a.flags=uint8_t(cmp&0xf1);return;}a.values[0]=((r.base+t.field())&0xffffff00u)|1;a.values[2]=0;a.values[6]=(a.values[6]&0xffff0000u)|(unsigned(t.value())<<8)|t.preserveMask();a.flags&=0xd0;return;}
 if(r.kind==K::BindCurrent||r.kind==K::BindPart){if(r.bound){a.values[0]=0x168fcu+256*r.part;a.values[1]=0x16e12u+256*r.part;a.flags&=0xd0;}else{a.values[0]=(a.values[0]&0xffffff00u)|r.mode;a.flags=uint8_t((a.flags&0xf1)|nz8(r.mode));}return;}
 if(r.kind==K::Dirty){a.values[0]=(a.values[0]&0xffff0000u)|(unsigned(r.part)<<8)|r.dirty;a.flags=uint8_t((a.flags&0xf1)|nz8(r.dirty));return;}
 if(r.kind==K::Expand){a.values[5]=r.source;a.values[4]=r.target;}const auto decoded=r.expand->signedMode();a.values[0]=(a.values[0]&0xffff0000u)|(unsigned(r.lastPacked&7)<<8)|decoded;
 if(r.semitone)a.flags=controlArithmetic::subtract(int8_t(r.hardware),int8_t(4),a.flags).flags;else a.flags=uint8_t((a.flags&0xf0)|((r.lastPacked>>2)&1));a.flags=uint8_t((a.flags&0xf1)|nz16(uint16_t(a.values[6])));
}
}
