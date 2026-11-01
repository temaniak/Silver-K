#pragma once
#include "ksNativeArpResetService.h"
#include "ksNativeKeyAbi.h"
#include "ksPanelMasksAbi.h"
#include "ksPeriodicAbi.h"
namespace ks {
template<class A>void projectNativeArpReset(A&a,const NativeArpResetService&s){
 if(!s.complete())throw std::logic_error("Incomplete native arp reset projection");const auto&r=s.summary();if(!r.firstHeldTask||!r.resetTask||!r.periodicTask)throw std::logic_error("Missing native arp reset outcome");
 projectNativeKey(a,*r.firstHeldTask);
 if(r.phase){a.values[1]=0;if(r.mono){if(!r.maskTask)throw std::logic_error("Missing mono reset mask outcome");const auto before=a.values[0];a.values[1]=r.deadline;a.values[5]=r.delta;projectPanelMasks(a,*r.maskTask);a.values[0]=before;}}
 a.values[4]=(a.values[4]&0xffff0000u)|(unsigned(r.direction)<<8)|r.last;a.values[3]=(a.values[3]&0xffffff00u)|r.octave;
 if(r.heldScans>1)projectNativeKey(a,*r.heldTask);
 // The held reset restores its caller's lookup/list/count values; the final
 // sentinel changes only the low byte before periodic synchronization.
 a.values[1]=(a.values[1]&0xffffff00u)|255;a.flags=uint8_t((a.flags&0xd0)|8);
 projectPeriodic(a,*r.periodicTask);
}
}
