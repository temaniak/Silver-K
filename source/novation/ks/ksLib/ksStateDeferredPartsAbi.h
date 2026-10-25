#pragma once
#include "ksStateDeferredPartsService.h"
#include "ksStateVoiceRefreshForeground.h"
namespace ks {
// Completion-only projection. Caller ABI belongs to the outer boundary, never
// to a running deferred/Voice child. A supplies values[7] and flags.
template<class A>void projectStateDeferredParts(A&a,const StateDeferredPartsService&s){if(!s.complete())throw std::logic_error("Incomplete deferred projection");const auto&f=s.summary();const auto saved=a.values;const auto flags=a.flags;
 if(f.delay){a.values[0]=(saved[0]&0xffffff00u)|f.delayAfter;a.flags=uint8_t((flags&0xf1)|(f.delayAfter&128?8:0)|(!f.delayAfter?4:0));return;}
 if(!f.pending){a.values[0]=saved[0]&0xffffff00u;a.flags=uint8_t((flags&0xf1)|4);return;}
 if(f.refreshed){StateVoiceRefreshForeground::CallerAbi child;child.values=a.values;child.flags=a.flags;StateVoiceRefreshForeground::project(child,*s.voice());a.values=child.values;a.flags=child.flags;}else a.flags=flags&0xd0;
 a.values[0]=(a.values[0]&0xffff0000u)|f.remainingMask;
 a.values[1]=(a.values[1]&0xffff0000u)|(saved[1]&0xff00u)|f.selected;
 a.values[2]=(a.values[2]&0xffff0000u)|(saved[2]&0xff00u)|f.processed;
 a.flags=uint8_t((a.flags&0xf0)|((f.refreshed&&f.lastPart==4)?a.flags&1:0)|(!f.processed?4:0));
}
}
