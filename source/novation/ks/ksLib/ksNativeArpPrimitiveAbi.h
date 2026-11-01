#pragma once
#include "ksNativeArpPrimitiveService.h"
#include "ksNativeMonoArpBootstrapAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeArpPrimitive(A&a,const NativeArpPrimitiveService&s){if(!s.complete())throw std::logic_error("Incomplete arp primitive projection");using K=NativeArpPrimitiveService::Kind;const auto&t=s.primitive();const auto&v=s.summary();if(s.kind()==K::NullResult){a.values[0]=0;a.flags=uint8_t((a.flags&0xd0)|4);return;}
 if(s.kind()==K::Pattern){a.values[1]=t.nextReference();a.values[4]=t.step();a.flags=controlArithmetic::subtract(int8_t(uint8_t(t.cursor()+1)),int8_t(t.length()),a.flags).flags;const auto saved=int16_t(a.values[2]);a.flags=uint8_t((a.flags&0xf1)|(saved<0?8:!saved?4:0));return;}
 if(s.kind()==K::Tempo){a.values[0]=0x160d2u+334*v.part;if(!v.changed){a.values[1]=(a.values[1]&0xffff00ffu)|(uint32_t(t.tempo())<<8);a.flags=controlArithmetic::subtract(int8_t(v.input.tempo),int8_t(t.tempo()),a.flags).flags;return;}}
 const auto&r=s.resetTask()?*s.resetTask():t;a.values[1]=r.mode();a.flags=uint8_t((a.flags&0xd0)|((r.mode()&128)?8:0)|((r.mode()&64)?0:4));if(s.mono())projectNativeMonoArpBootstrap(a,*s.mono());
}
}
