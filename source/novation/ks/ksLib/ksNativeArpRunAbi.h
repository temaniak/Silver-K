#pragma once
#include "ksNativeArpRunService.h"
#include "ksNativeArpOrderedAbi.h"
#include "ksNativeArpSearchAbi.h"
#include "ksNativeKeyAbi.h"
namespace ks {
template<class A>void projectNativeArpRun(A&a,const NativeArpRunService&s){if(!s.complete())throw std::logic_error("Incomplete arp run projection");const auto&v=s.summary();const auto original=a.values;const auto bindings=[&]{a.values[2]=s.tuningBindingAfter(original[2]);a.values[5]=v.expressionSource;a.values[6]=s.notificationAfter(original[6]);};const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto high=[](uint32_t&x,uint8_t n){x=(x&0xffff00ffu)|(unsigned(n)<<8);};const auto value8=[](uint8_t f,uint8_t n){return uint8_t((f&0xf1)|(n&128?8:!n?4:0));};a.values[0]=0x160d2u+334*v.firstPart;low(a.values[1],0);low(a.values[6],v.mode);a.flags=value8(uint8_t(a.flags&0xd0),v.mode);a.flags=uint8_t((a.flags&~4)|((v.mode&8)?0:4));if(v.countRead){low(a.values[1],v.count);a.flags=value8(a.flags,v.count);}
 if(v.tailOrdered){projectNativeArpOrdered(a,*s.lastOrdered());bindings();return;}
 if(v.orderedChord){const auto&m=v.musical;auto child=a;child.values[2]=m.tuningBindingAfter(original[2]);child.values[3]=m.voiceBindingAfter(original[3]);child.values[5]=v.expressionSource;child.values[6]=m.notificationAfter(a.values[6]);projectNativeArpOrdered(child,*s.lastOrdered());a=child;a.values[0]=0x160d2u+334*v.firstPart;a.values[1]=(uint32_t(m.payloadPrefixAfter(uint16_t(original[1]>>16)))<<16)|v.count;a.values[2]=m.tuningBindingAfter(original[2]);a.values[3]=m.voiceBindingAfter(original[3]);a.values[5]=v.expressionSource;a.values[6]=(m.notificationAfter(original[6])&0xffff0000u)|v.mode;a.flags=uint8_t((a.flags&0xf1)|4);bindings();return;}
 if(v.held){projectNativeKey(a,*s.held());if(v.bitmapBound)a.values[3]=v.bitmap;if(s.lastKey())projectNativeKey(a,*s.lastKey());if(v.highOrder>v.keyOrder)high(a.values[1],v.scalarHigh);if(v.lowOrder>v.keyOrder)low(a.values[1],v.scalarLow);if(s.lastSearch()){a.values[0]=0x160d2u+334*v.part;a.values[5]=v.expressionSource;projectNativeArpSearch(a,*s.lastSearch());a.values[5]=v.expressionSource;}}
 bindings();
}
}
