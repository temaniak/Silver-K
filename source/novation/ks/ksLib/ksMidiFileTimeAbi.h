#pragma once
#include "ksMidiFileTimeService.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectMidiFileTime(A&a,const MidiFileTimeService&s){if(!s.complete())throw std::logic_error("Incomplete file time projection");const auto&f=s.summary();const auto&i=s.input();auto&v=a.values;using K=MidiFileTimeService::Kind;const auto nz32=[](uint32_t n){return uint8_t((n&0x80000000u?8:0)|(!n?4:0));};
 if(s.kind()==K::Due){v[0]=f.now;v[1]=f.deadline;a.flags=controlArithmetic::subtract(int32_t(f.now),int32_t(f.deadline),a.flags).flags;}
 else if(s.kind()==K::Tempo){v[0]=i.tempo>>4;v[1]=f.period;a.flags=uint8_t((a.flags&0xf0)|(i.tempo&8?1:0)|nz32(f.period));}
 else if(s.kind()==K::Vlq){v[4]=f.stream;v[5]=f.value;if(f.vlqCount>1){v[1]=f.vlqBytes[f.vlqCount-1];a.flags=controlArithmetic::add(int32_t(f.lastAddBase),int32_t(v[1]),a.flags).flags;}else a.flags=uint8_t((a.flags&0xd0)|(!f.value?4:0));}
 else {v[4]=f.stream;v[5]=f.value;v[1]=f.published;const auto flags=controlArithmetic::add(int32_t(f.deadline),int32_t(f.value),a.flags).flags;a.flags=uint8_t((flags&0xf1)|nz32(f.published));}
}
}
