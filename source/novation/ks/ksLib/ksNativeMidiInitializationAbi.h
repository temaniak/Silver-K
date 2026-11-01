#pragma once
#include "ksNativeMidiInitializationService.h"
namespace ks {
template<class A>void projectNativeMidiInitialization(A&a,const NativeMidiInitializationService&s){if(!s.complete())throw std::logic_error("Incomplete MIDI initialization projection");if(s.controlsTask()){const auto&t=*s.controlsTask();a.values[0]=0xf304u+t.offset();a.values[1]=0x174aeu+20*t.part();a.values[2]=unsigned(t.byte())<<8;a.values[3]=t.patch();a.values[4]=0x174aeu+20*(t.part()-1);}else{const auto&t=*s.systemTask();const auto base=0x160d2u+334*t.part();a.values[0]=base;a.values[1]=base+t.dataOffset();a.values[2]&=0xff00u;a.values[3]=(a.values[3]&0xffff0000u)|(t.remainingParts()<<8)|t.index();a.values[4]=t.reference();}a.flags=uint8_t((a.flags&0xd0)|4);}
}
