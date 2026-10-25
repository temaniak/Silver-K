#pragma once
#include "ksStateFactoryDspService.h"
#include "ksControllerDspAbi.h"
namespace ks {
// Final external results are computed from completed transport observations.
// None of these values is used to drive the native factory operation.
template<class A>void projectStateFactoryDsp(A&a,const StateFactoryDspService&s){
 using T=StateFactoryDspService;const auto&o=s.summary();if(!s.complete())throw std::logic_error("Factory DSP ABI requires completed work");
 const auto word=[](uint32_t&v,uint16_t x){v=(v&0xffff0000u)|x;};
 if(s.kind()==T::Kind::Poll){word(a.values[4],0x803e);projectControllerDsp(a,*s.lastTransport());a.flags=uint8_t((a.flags&0xd0)|4);return;}
 if(s.kind()==T::Kind::Stream){word(a.values[4],0x8c00);projectControllerDsp(a,*s.streamPrime());word(a.values[0],uint16_t(s.input().descriptor>>16));a.values[1]=o.streamPayload;a.values[2]=o.sourceAfter;a.values[3]&=0xffffu;projectControllerDsp(a,*s.streamWord());if(o.streamRetried)a.values[0]=(a.values[0]&0xffffu)|(uint32_t(o.streamRetryRemaining)<<16);uint8_t hc=o.byteMode?uint8_t(o.lastByte&128?1:0):uint8_t(a.flags&33);if(o.byteMode&&s.streamWord()->summary().retried)hc=0;if(!o.byteMode&&o.streamRetried)hc=0;a.flags=uint8_t((a.flags&0xd0)|hc|4);return;}
 a.values[0]=0x15200;a.values[1]=0xffffff00u;a.values[2]=o.sourceAfter;a.values[3]=o.lastShape;a.values[5]=0x600000;projectControllerDsp(a,*s.finalSelect());word(a.values[4],0x803e);projectControllerDsp(a,*s.finalWrite());if(o.doneRetried){a.values[0]=(a.values[0]&0xffffu)|(uint32_t(o.doneRetryRemaining)<<16);a.flags&=uint8_t(~33u);}word(a.values[4],0x803d);projectControllerDsp(a,*s.finalRead());
 if(s.kind()==T::Kind::Reset){word(a.values[0],0x40ff);a.values[1]=0x0e00;word(a.values[2],uint16_t(unsigned(o.lastUpper)<<8|o.lastMapping));a.values[3]=0x17678;a.values[4]=2;a.values[5]=0x600000;a.flags&=0xd0;projectControllerDsp(a,*s.lastTransport());}
}
}
