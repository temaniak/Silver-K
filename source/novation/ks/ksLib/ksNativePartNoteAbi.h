#pragma once
#include "ksNativePartNoteService.h"
#include "ksNativeNoteStateAbi.h"
#include "ksNativeNoteProgramAbi.h"
#include "ksNativeVoiceReleaseAbi.h"
#include "ksStateCentralActionsLeavesAbi.h"
namespace ks {
template<class A>void projectNativePartNote(A&a,const NativePartNoteService&s){if(!s.complete())throw std::logic_error("Incomplete native part note projection");const auto&r=s.summary();const auto&t=s.noteTask();const auto low=[](uint32_t&v,uint8_t n){v=(v&0xffffff00u)|n;};const auto high=[](uint32_t&v,uint8_t n){v=(v&0xffff00ffu)|(unsigned(n)<<8);};const auto word=[](uint32_t&v,uint16_t n){v=(v&0xffff0000u)|n;};const auto nz8=[](uint8_t n){return uint8_t((n&128?8:0)|(!n?4:0));};const auto nz16=[](uint16_t n){return uint8_t((n&0x8000?8:0)|(!n?4:0));};
 if(r.previewed){low(a.values[6],t.previewNote());a.flags=uint8_t((a.flags&0xf1)|nz8(t.previewNote()));const auto payload=a.values[4];projectStateCentralActions(a,*s.previewOutcome(),[](auto,auto&){throw std::logic_error("Unexpected preview external projection");});a.values[4]=payload;a.flags=uint8_t((a.flags&0xf1)|(payload&0x80000000u?8:0)|(!payload?4:0));}
 low(a.values[5],t.note());a.values[0]=0x174aeu+20*t.controlPart();word(a.values[1],uint16_t((unsigned(t.high())<<8)|t.low()));a.flags=controlArithmetic::subtract(int8_t(t.note()),int8_t(t.low()),uint8_t(a.flags&0xd0)).flags;if(t.note()<t.low())return;a.flags=controlArithmetic::subtract(int8_t(t.note()),int8_t(t.high()),a.flags).flags;if(t.note()>t.high())return;
 if(t.kind()==PartNoteTask::Kind::On)high(a.values[5],t.velocity());projectNativeNoteState(a,*r.state);a.values[0]=0x160d2u+334*r.policyPart;low(a.values[1],t.policy());a.flags=uint8_t((a.flags&0xf1)|nz8(t.policy()));if(t.policy()&4){low(a.values[1],t.maskedPolicy());a.flags=controlArithmetic::subtract(int8_t(t.maskedPolicy()),int8_t(96),a.flags).flags;if(t.maskedPolicy()!=96)return;}
 if(!t.releasing()){projectNativeNoteProgram(a,*r.program);return;}const auto retained=t.retainedNote();a.values[0]=0x160d2u+334*r.priorityPart;low(a.values[1],t.count());a.flags=controlArithmetic::subtract(int8_t(t.count()),int8_t(1),a.flags).flags;if(t.count()<=1){low(a.values[0],t.last());a.flags=controlArithmetic::subtract(int8_t(uint8_t(retained)),int8_t(t.last()),a.flags).flags;if(r.priority){projectNativeKey(a,*r.keys);a.flags=uint8_t((a.flags&0xf1)|nz8(uint8_t(r.keys->summary().noteVelocity)));}}
 if(r.program)projectNativeNoteProgram(a,*r.program);word(a.values[5],retained);a.flags=uint8_t((a.flags&0xf1)|nz16(retained));if(r.release)projectNativeVoiceRelease(a,*r.release);
}
}
