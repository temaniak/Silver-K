#pragma once
#include "ksControllerDspService.h"
namespace ks {
// External return projection only. Semantic protocol observations remain the
// sole state used by the live transaction; no register bank is held by it.
template<class A>void projectControllerDsp(A&a,const ControllerDspService&s){
 using J=ControllerDspService;using K=DspRoutingTask::Kind;const auto&v=s.summary();
 const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};
 const auto subHC=[](uint32_t x,uint32_t y,unsigned sign,unsigned half){const auto r=x-y;const auto bits=(~x&y)|(~x&r)|(y&r);return uint8_t((bits&sign?1:0)|(bits&half?32:0));};
 const auto addHC=[](uint32_t x,uint32_t y){const auto r=x+y;const auto bits=(x&y)|(x&~r)|(y&~r);return uint8_t((bits&0x8000?1:0)|(bits&0x800?32:0));};
 uint8_t hc=a.flags&33;
 if(v.offsetAdded){word(a.values[0],v.offsetValue);hc=addHC(v.offsetBase,v.offsetValue);word(a.values[4],v.address);}
 const bool voice=v.kind==J::Kind::Routing&&(v.input.routing==K::VoiceA||v.input.routing==K::VoiceB);
 if(voice){a.values[0]=v.alternateProduct;const auto*t=s.task<DspRoutingTask>();word(a.values[1],v.input.routing==K::VoiceB?v.voice:t->difference());if(v.voiceDifference)hc=subHC(v.voice,v.baseVoice,0x80,8);}
 if(v.cacheCompared){word(a.values[0],v.comparedCached);hc=subHC(v.comparedAddress,v.comparedCached,0x8000,0x800);}
 if(v.clearedRetry)a.values[0]&=0xffffu;
 if(v.retried){a.values[0]=(uint32_t(v.remaining)<<16)|(a.values[0]&0xffffu);hc=0;}
 if(v.selectedPort)a.values[5]=v.port==DspPort::A?0x400000:0x600000;
 uint8_t nz=0;
 if(v.wordTransmitted){a.values[1]=(v.word<<8)|(a.values[1]&255u);word(a.values[0],uint16_t(v.word>>8));const auto*t=s.task<DspWordTask>();if(t&&t->advancesAddress()){word(a.values[4],v.address);nz=uint8_t(v.address&0x8000?8:v.address?0:4);}else nz=uint8_t(v.word&0x80?8:uint8_t(v.word)?0:4);}
 else if(v.addressRequested){const auto*t=s.task<DspAddressTask>();if(t&&t->receives()){word(a.values[4],uint16_t(v.reply>>8));a.values[0]=(a.values[0]&0xffffff00u)|uint8_t(v.reply);nz=uint8_t(v.reply&0x80?8:uint8_t(v.reply)?0:4);}else{a.values[0]&=0xffffff00u;nz=4;}}
 else{word(a.values[0],0xffff);nz=8;}
 a.flags=uint8_t((a.flags&0xd0)|hc|nz);
}
}
