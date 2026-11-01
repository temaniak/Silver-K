#pragma once
#include "ksNativeNoteStateService.h"
#include "ksNativeArpResetAbi.h"
namespace ks {
template<class A>void projectNativeNoteState(A&a,const NativeNoteStateService&s){if(!s.complete())throw std::logic_error("Incomplete native note state projection");const auto&r=s.summary();const auto original=a.values;
 if(r.kind==NativeNoteStateService::Kind::On&&r.arp){projectNativeArpReset(a,*r.arp);a.values[0]=original[0];a.values[1]=original[1];a.values[2]=original[2];a.values[5]=(a.values[5]&0xffff0000u)|(original[5]&65535);a.values[6]=(a.values[6]&0xffff0000u)|(original[6]&65535);}
 if(r.kind==NativeNoteStateService::Kind::Off&&r.heldScan){const auto&t=*r.held->summary().activityTask;const auto cursor=0x160d2u+334*t.part()+208+2*t.cursor();a.values[2]=(cursor&0xffff0000u)|(original[2]&65535);}
 a.flags=uint8_t((a.flags&0xd0)|(original[0]&0x80000000u?8:0)|(!original[0]?4:0));
}
}
