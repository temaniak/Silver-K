#pragma once
#include "ksNativeArpEmissionService.h"
namespace ks {
// The pending panel activation is a single explicit domain byte. Each transfer
// uses the completed musical job's observed mapping/voice facts only.
inline uint8_t voiceStartActivationAfter(const NativeVoiceStartService&s,uint8_t prior){if(!s.complete()||!s.start())throw std::logic_error("Incomplete voice activation binding");if(s.allocation()){const auto&t=*s.allocation()->population();prior=t.curve()>=4?uint8_t((0x1534eu+s.allocation()->velocity()->curve())>>8):0;}if(s.start()->suppressed())return prior;if(s.parameters()){const auto&v=s.parameters()->summary();if(!v.glide)throw std::logic_error("Missing voice parameter mapping");const auto&g=*v.glide;if(!g.time())return g.mode();return v.distanceObserved&&g.distance()?g.distance():v.distanceObserved?0:g.adjustment();}if(s.glide()){const auto&g=*s.glide();if(!g.time())return g.mode();return s.summary().distanceObserved&&g.distance()?g.distance():s.summary().distanceObserved?0:g.adjustment();}return prior;}
inline uint8_t noteProgramActivationAfter(const NativeNoteProgramService&s,uint8_t prior){if(!s.complete())throw std::logic_error("Incomplete program activation binding");const auto&v=s.summary();if(v.played)prior=voiceStartActivationAfter(*v.start,prior);if(v.temporary)prior=uint8_t(v.selectTask->originalReference()>>8);return prior;}
inline uint8_t arpEmissionActivationAfter(const NativeArpEmissionService&s,uint8_t prior){if(!s.complete())throw std::logic_error("Incomplete emission activation binding");if(s.release())return uint8_t(s.release()->finalAgeWord()>>8);if(s.program())return noteProgramActivationAfter(*s.program(),prior);return prior;}
}
