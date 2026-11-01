#pragma once
#include "ksNativeNoteProgramService.h"
#include "ksNativeVoiceStartAbi.h"
#include "ksPeriodicAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeNoteProgram(A&a,const NativeNoteProgramService&s){if(!s.complete())throw std::logic_error("Incomplete native note program projection");const auto&r=s.summary();const auto nz32=[](uint32_t v){return uint8_t((v&0x80000000u?8:0)|(!v?4:0));};
 if(r.kind==NativeNoteProgramService::Kind::Select){const auto&t=*r.selectTask;a.values[0]=t.program();if(t.bank()>4){if(t.note()<36){a.flags=controlArithmetic::subtract(int8_t(t.note()),int8_t(36),a.flags).flags;return;}if(t.note()>84){a.flags=controlArithmetic::subtract(int8_t(t.note()),int8_t(84),a.flags).flags;return;}a.values[3]=(a.values[3]&0xffffff00u)|t.sampleIndex();if(r.temporary){a.values[0]=0x174aeu+20*t.controlPart();a.values[1]=t.patchReference();a.values[2]=t.originalReference();}}}
 if(r.played){const auto&t=r.playTask?*r.playTask:*r.selectTask;a.values[0]=0x160d2u+334*t.part();a.flags=uint8_t((a.flags&0xf1)|(t.lastNote()&128?8:0)|(!t.lastNote()?4:0));projectNativeVoiceStart(a,*r.start);projectPeriodic(a,*r.periodic);}
 if(r.temporary){const auto&t=*r.selectTask;a.values[0]=0x174aeu+20*t.controlPart();a.values[2]=t.originalReference();a.flags=uint8_t((a.flags&0xf1)|nz32(t.originalReference()));}
}
}
