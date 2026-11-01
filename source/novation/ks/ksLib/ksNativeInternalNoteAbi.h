#pragma once
#include "ksNativeInternalNoteService.h"
#include "ksInternalKeyOutputAbi.h"
#include "ksNativeArpEmissionAbi.h"
namespace ks {
template<class A>void projectNativeInternalNote(A&a,const NativeInternalNoteService&s){if(!s.complete())throw std::logic_error("Incomplete internal note projection");const auto&f=s.summary();const auto&t=s.noteTask();a.values[4]=f.input.payload+1;a.values[5]=(a.values[5]&0xffff0000u)|f.noteVelocity;a.values[0]=0x160d2u+334*f.part;a.flags=uint8_t((a.flags&0xd0)|(a.values[0]&0x80000000u?8:0)|(!a.values[0]?4:0));projectInternalKeyOutput(a,*f.bitmap);if(f.retained)projectNativeArpEmission(a,*s.emissionOutcome(),f.retainedNoteVelocity);else projectNativeArpEmission(a,*s.emissionOutcome());}
}
