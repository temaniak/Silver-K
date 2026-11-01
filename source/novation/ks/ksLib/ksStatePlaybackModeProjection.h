#pragma once
#include "ksStatePlaybackModeCoordinator.h"
#include "ksControlArithmetic.h"
namespace ks {namespace playbackModeProjection {
inline uint8_t nz8(uint8_t v){return uint8_t((v&0x80?8:0)|(!v?4:0));}inline uint8_t nz16(uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));}inline uint8_t nz32(uint32_t v){return uint8_t((v&0x80000000?8:0)|(!v?4:0));}
inline void low(uint32_t&v,uint8_t b){v=(v&0xffffff00u)|b;}inline void high(uint32_t&v,uint8_t b){v=(v&0xffff00ffu)|(unsigned(b)<<8);}inline void word(uint32_t&v,uint16_t w){v=(v&0xffff0000u)|w;}
// Completion-only projection of the four closed routing helpers. The pure
// coordinator's outcomes contain observations and decisions, never ABI data.
template<class A>void lowerNode(A&a,const StatePlaybackModeCoordinator::Outcome&o){using K=StatePlaybackModeCoordinator::Kind;auto&v=a.values;const auto&f=o.facts;const auto entry=a.flags;switch(o.kind){
 case K::Routing:{const auto hc=controlArithmetic::subtract(int8_t(f.previousActiveRead?f.compareActive:f.mode),int8_t(f.previousActiveRead?f.previousActive:f.previousMode),0).flags&0x21;a.flags=uint8_t((entry&0xd0)|hc|nz32(v[0]));break;}
 case K::BindPart:v[0]=f.currentBase;v[1]=f.liveBase;a.flags=uint8_t((entry&0xd0)|nz32(v[1]));break;
 case K::ResolveEditPart:v[1]=uint8_t(f.editPart*2);low(v[0],f.marker);a.flags=uint8_t((entry&0xd0)|(f.editPart&128?1:0)|nz8(f.marker));break;
 case K::ResolveParts:word(v[0],uint16_t((unsigned(f.available)<<8)|f.current));if(f.probed)word(v[1],uint16_t((unsigned(f.probeRemaining)<<8)|f.probe));a.flags=uint8_t((entry&0xf1&~(f.shifted?1:0))|nz8(f.current));break;
 default:throw std::logic_error("Unsupported completed mode helper projection");}}
template<class A>void lower(A&a,const StatePlaybackModeCoordinator&s){if(!s.complete()||!s.summary().count)throw std::logic_error("Incomplete mode helper projection");lowerNode(a,*s.summary().calls[0]);}
}}
#include "ksStatePlaybackResetService.h"
namespace ks {namespace playbackModeProjection {
template<class A>void reset(A&a,const StatePlaybackResetService&s){if(!s.complete())throw std::logic_error("Incomplete playback reset child projection");using K=StatePlaybackResetService::Kind;auto&v=a.values;const auto&f=s.summary();if(s.kind()==K::Panic){v[0]=f.controls;low(v[6],0);const auto next=uint16_t((f.serialProducer+1)&255);a.flags=uint8_t((a.flags&0xd0)|nz32(v[0])|(f.serialConsumer<next?1:0)|((f.serialConsumer&0xfff)<(next&0xfff)?32:0));return;}
 if(s.kind()==K::ReleaseVoices){const auto*t=s.voices();if(!t||!t->voiceTask())throw std::logic_error("Missing release outcome");const auto saved0=uint8_t(v[0]>>8),saved1=uint8_t(v[1]>>8);if(t->releasedAny()){v[0]=t->dspRetried()?uint32_t(t->dspRemaining())<<16:0;v[1]=0;v[2]=(uint32_t(t->lastReleaseAge())<<16)|uint16_t(v[2]);word(v[4],uint16_t(t->lastAddress()+1));v[5]=t->dspPort();v[6]=t->lastPatch();}word(v[0],uint16_t(unsigned(saved0)<<8));word(v[1],uint16_t((unsigned(saved1)<<8)|t->voiceTask()->part()));word(v[2],0);v[3]=0x17560u+20*t->lastSlot();}
 a.flags=uint8_t((a.flags&0xd0)|nz32(v[0]));
}
}}
