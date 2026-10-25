#pragma once
#include "ksNativeVoiceSweepService.h"
#include "ksControllerDspAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeVoiceSweep(A&a,const NativeVoiceSweepService&s){if(!s.complete())throw std::logic_error("Incomplete voice sweep projection");const auto&t=s.sweep();const auto&v=s.summary();const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};
 if(v.mirrorObserved){a.values[0]=0x174aeu+20*v.mirrorPart+20;a.values[1]=0x17526;a.values[2]=(a.values[2]&0xff00u)|v.mirrorByte;}
 if(s.lastSelect())projectControllerDsp(a,*s.lastSelect());
 if(s.lastCut()){if(t.retune()){a.values[1]=(uint32_t(s.lastCut()->summary().word)<<8)|uint8_t(a.values[1]);}else a.values[1]=0x10000000u|(a.values[1]&65535u);projectControllerDsp(a,*s.lastCut());}
 if(s.lastGate()){a.values[1]=0;a.flags=uint8_t((a.flags&0xd0)|4);projectControllerDsp(a,*s.lastGate());}
 if(t.retune()){word(a.values[0],uint16_t((0x174aeu+20*v.mirrorPart+20)&0xff00u));word(a.values[1],uint16_t(0x7500|t.match()));}
 else {word(a.values[0],0);a.values[2]=(v.released?uint32_t(v.lastReleasedAge)<<16:a.values[2]&0xffff0000u)|(v.lastInspectionReleased?0xffffu:v.lastInspectedAge);}
 if(v.voiceObserved)a.values[3]=0x17560u+20*v.voice;if(v.patchObserved)a.values[6]=v.patch;if(s.lastCut())a.values[5]=v.port==DspPort::A?0x400000:0x600000;
 uint8_t hc=0;if(t.age()){if(t.retune()&&t.owner()!=t.match())hc=controlArithmetic::subtract(int8_t(t.match()),int8_t(t.owner()),0).flags&33;else hc=a.flags&33;}a.flags=uint8_t((a.flags&0xd0)|hc|4);
}
}
