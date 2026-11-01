#pragma once
#include "ksNativeArpChordService.h"
#include "ksNativeArpEmitAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeArpChord(A&a,const NativeArpChordService&s){if(!s.complete())throw std::logic_error("Incomplete arp chord projection");const auto&v=s.summary();const auto&o=v.outputs;const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto high=[](uint32_t&x,uint8_t n){x=(x&0xffff00ffu)|(uint32_t(n)<<8);};const auto value8=[](uint8_t f,uint8_t n){return uint8_t((f&0xf1)|(n&128?8:!n?4:0));};a.values[0]=0x160d2u+334*v.part;a.flags=uint8_t(a.flags&0xd0);
 low(a.values[1],v.policy);a.flags=value8(a.flags,v.policy);a.flags=uint8_t((a.flags&~4)|((v.policy&4)?0:4));
 if(v.modeRead){low(a.values[6],v.mode);a.flags=value8(a.flags,v.mode);a.flags=uint8_t((a.flags&~4)|((v.mode&32)?0:4));}
 if(v.queueRead){high(a.values[1],v.queue);a.flags=value8(a.flags,v.queue);}
 if(v.orderedRead){low(a.values[1],uint8_t(v.ordered-v.queue));a.values[2]=v.ordered;a.flags=controlArithmetic::subtract(int8_t(v.ordered),int8_t(v.queue),uint8_t(a.flags&0xd0)).flags;}
 if(v.prepared){
  low(a.values[3],v.octave);if(o.voiceKnown)a.values[3]=0x17560u+20*o.voice;if(o.sampleOrder>o.fullVoiceOrder)low(a.values[3],o.sample);
  if(o.addressKnown)a.values[4]=(a.values[4]&0xffff0000u)|o.address;
  uint32_t upper=a.values[1]&0xffff0000u;using P=NativeArpChordService::Payload;switch(o.pitchPayload){case P::Append:upper=0;break;case P::SamplePatch:upper=o.payload&0xffff0000u;break;case P::VoiceSelection:upper=o.payload<<16;break;case P::VoiceWord:upper=(o.payload>>8)<<16;break;case P::Preserved:break;}
  a.values[1]=upper|(uint32_t(v.finalMode&127)<<8);a.values[2]=uint32_t(int32_t(a.values[0])+s.event().listOffset());a.values[5]=v.expressionSource;a.values[6]=s.notificationAfter(a.values[6]);
  if(!s.lastEmit())throw std::logic_error("Missing chord note outcome");auto child=a;projectNativeArpEmit(child,*s.lastEmit());a.flags=value8(child.flags,uint8_t(v.finalMode&127));
 }
}
}
