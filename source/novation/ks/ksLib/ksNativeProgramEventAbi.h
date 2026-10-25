#pragma once
#include "ksNativeProgramEventService.h"
#include "ksNativeParameterSupportAbi.h"
#include "ksStateCentralActionsLeavesAbi.h"
#include "ksStatePlaybackModeForeground.h"
#include "ksStateProgramLoadForeground.h"
#include "ksStateFieldForeground.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeProgramEvent(A&a,const NativeProgramEventService&s){
 if(!s.complete())throw std::logic_error("Incomplete MIDI program event projection");const auto&f=s.summary();auto&v=a.values;const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto high=[](uint32_t&x,uint8_t n){x=(x&0xffff00ffu)|(unsigned(n)<<8);};const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};const auto nz8=[](uint8_t n){return uint8_t((n&128?8:0)|(!n?4:0));};const auto nz16=[](uint16_t n){return uint8_t((n&0x8000?8:0)|(!n?4:0));};const auto move=[&](uint8_t n){a.flags=uint8_t((a.flags&0xf1)|nz8(n));};
 const auto reset=[&]{if(f.reset)projectStateCentralActions(a,*s.resetOutcome(),[](auto,auto&){throw std::logic_error("Unexpected program reset external projection");});};
 const auto clear=[&]{if(f.cleared){StatePlaybackModeForeground::CallerAbi b;b.values=v;b.flags=a.flags;StatePlaybackModeForeground::project(b,*s.clearOutcome());v=b.values;a.flags=b.flags;}};
 const auto display=[&]{if(f.displayed){StateFieldForeground::CallerAbi b;b.values=v;b.flags=a.flags;StateFieldForeground::projectUi(b,*s.displayOutcome());v=b.values;a.flags=b.flags;}};
 const bool multi=s.multiTask()!=nullptr;
 if(multi){high(v[5],uint8_t(f.event>>8));move(uint8_t(f.event>>8));a.flags=controlArithmetic::subtract(int8_t(f.event>>8),int8_t(1),a.flags).flags;if(uint8_t(f.event>>8)!=1)return;a.flags=controlArithmetic::subtract(int8_t(f.event),int8_t(100),a.flags).flags;if(uint8_t(f.event)>=100)return;high(v[0],f.previousMode);low(v[0],1);a.flags=controlArithmetic::subtract(int8_t(f.previousMode),int8_t(1),a.flags).flags;reset();clear();if(f.reloaded){const auto*r=s.reloadOutcome();const auto*p=r->panel();v[0]=p->current();v[1]=p->pointer();v[2]=(p->lastResolvedBase()&0xffff0000u)|p->bounds();v[3]=(p->lastBitSelectorReference()&0xffff0000u)|p->bounds();v[4]=(f.reloadTarget&0xffff0000u)|0x8046u;v[5]=0x60010e;if(f.reloadPatchObserved)v[6]=f.lastReloadPatch;word(v[6],uint16_t((unsigned(f.menu)<<8)|21u));}display();return;}
 const auto originalEvent=f.input.event;const auto bank=uint8_t(originalEvent>>8),index=uint8_t(originalEvent);v[0]=f.slot;a.flags=uint8_t((a.flags&0xd0)|0);low(v[1],f.previousBank);move(f.previousBank);move(bank);
 if(!(bank&128)){a.flags=uint8_t((a.flags&~4u)|(!(bank&32)?4:0));if(f.single){high(v[5],uint8_t(f.event>>8));move(uint8_t(f.event>>8));high(v[0],f.previousMode);low(v[0],0);a.flags=controlArithmetic::subtract(int8_t(f.previousMode),int8_t(0),a.flags).flags;reset();clear();word(v[5],f.event);a.flags=uint8_t((a.flags&0xf1)|nz16(f.event));low(v[0],0);move(0);playbackModeProjection::lower(a,*f.routing[0]);v[0]=0x167ae;a.flags=uint8_t((a.flags&0xf1)|0);}low(v[1],f.previousBank);move(f.previousBank);if(!f.previousBank)return;}
 high(v[1],100);move(100);a.flags=controlArithmetic::subtract(int8_t(f.previousBank),int8_t(4),a.flags).flags;if(f.previousBank>4){a.flags=controlArithmetic::subtract(int8_t(f.previousBank),int8_t(8),a.flags).flags;if(f.previousBank>8)return;high(v[1],50);move(50);}a.flags=controlArithmetic::subtract(int8_t(index),int8_t(f.limit),a.flags).flags;if(index>=f.limit)return;
 projectNativeParameterSupport(a,*f.bind);const auto heldHigh=uint8_t(v[0]>>8);low(v[0],f.activePart);move(f.activePart);StateProgramLoadForeground::project(a,*s.loadOutcome());word(v[0],uint16_t((unsigned(heldHigh)<<8)|f.savedPart));a.flags=uint8_t((a.flags&0xf1)|nz16(uint16_t(v[0])));low(v[0],255);move(255);playbackModeProjection::lower(a,*f.routing[f.routingCount-1]);display();
}
}
