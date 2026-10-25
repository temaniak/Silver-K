#pragma once
#include "ksNativeArpGateService.h"
#include "ksNativeKeyAbi.h"
#include "ksNativeVoiceReleaseAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeArpGate(A&a,const NativeArpGateService&s){if(!s.complete())throw std::logic_error("Incomplete arp gate projection");const auto&v=s.summary();const auto&g=v.gate;const auto&o=v.outputs;const uint16_t savedNote=uint16_t(a.values[5]),savedPatch=uint16_t(a.values[6]);const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};const auto nz8=[](uint8_t n){return uint8_t(n&128?8:!n?4:0);};a.values[0]=0x160d2u+334*v.part;
 if(s.kind()!=NativeArpGateService::Kind::Append||v.appendGate){
  if(g.bound)a.flags=uint8_t(a.flags&0xd0);
  if(g.stateRead){low(a.values[1],g.state);a.flags=uint8_t((a.flags&0xf1)|nz8(g.state));if(g.state){a.flags=controlArithmetic::subtract(int8_t(g.state),int8_t(2),a.flags).flags;}}
  if(g.phaseRead){a.values[1]=(uint32_t(g.expiry)<<16)|g.phase;a.flags=controlArithmetic::subtract(int16_t(g.phase),int16_t(g.expiry),a.flags).flags;}
  if(g.held){projectNativeKey(a,*s.held());if(g.voicesRead){low(a.values[1],g.voices);a.flags=controlArithmetic::subtract(int8_t(g.voices),int8_t(1),a.flags).flags;}if(g.shapeRead){low(a.values[1],g.shape);a.flags=controlArithmetic::subtract(int8_t(g.shape),int8_t(127),a.flags).flags;}}
  if(g.loop){
   if(o.voice)a.values[3]=0x17560u+20*o.lastVoice->lastSlot();
   if(o.released){a.values[2]=(uint32_t(o.releasedAge)<<16)|(a.values[2]&0xffffu);word(a.values[4],uint16_t(o.releasedAddress+1));a.values[5]=o.releasedPort;}
   if(o.lastVelocityEvent>o.lastReleaseEvent)word(a.values[5],o.velocityNote);
   if(o.lastNoteEvent>o.lastVelocityEvent&&o.lastNoteEvent>o.lastReleaseEvent)low(a.values[5],o.note);
   if(o.patchObserved)a.values[6]=o.patch;
   if(o.lastMidiEvent>o.lastPatchEvent)word(a.values[6],o.midiEvent);
   uint8_t hc=0;if(o.comparisonPolicy)hc=controlArithmetic::subtract(int8_t(o.comparedPolicy),int8_t(64),uint8_t(0)).flags&33;else if(o.voice){auto child=a;projectNativeVoiceRelease(child,*o.lastVoice);hc=child.flags&33;}if(!o.lastPolicyObserved)hc&=32;
   a.values[0]=0x160d2u+334*v.part;a.values[1]=a.values[0]+16+s.gate().cursor();a.values[2]&=0xffffff00u;a.flags=uint8_t((a.flags&0xd0)|hc|4);
  }
 }
 if(s.kind()==NativeArpGateService::Kind::Append){if(v.appendGate){word(a.values[5],savedNote);word(a.values[6],savedPatch);}a.values[1]=1;a.flags=uint8_t(a.flags&0xd0);}
}
}
