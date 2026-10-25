#pragma once
#include "ksNativeArpEmissionService.h"
#include "ksMidiPacketAbi.h"
#include "ksNativeVoiceReleaseAbi.h"
#include "ksNativeNoteProgramAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
// The typed caller supplies its original logical note only to the final ABI.
// No retained processor-stack payload belongs to the live emission graph.
template<class A>void projectNativeArpEmission(A&a,const NativeArpEmissionService&s,uint16_t restoredNoteVelocity){
 if(!s.complete())throw std::logic_error("Incomplete emission projection");const auto&v=s.summary();const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto value8=[](uint8_t f,uint8_t n){return uint8_t((f&0xf1)|(n&128?8:n?0:4));};
 a.values[0]=0x160d2u+334*v.input.part;if(s.kind()==NativeArpEmissionService::Kind::Off)low(a.values[5],uint8_t(v.input.noteVelocity));else word(a.values[5],v.input.noteVelocity);low(a.values[1],uint8_t(v.mask>>1));a.flags=uint8_t((a.flags&0xf0)|(v.mask&1)|(v.mask>>1?0:4));
 if(v.policyObserved){low(a.values[1],v.policy);a.flags=value8(a.flags,v.policy);if(s.midi()){
  const auto savedPatch=uint16_t(a.values[6]);if(s.kind()==NativeArpEmissionService::Kind::Off)word(a.values[5],uint16_t((64u<<8)|s.event().note()));projectMidiPacket(a,*s.midi());if(s.kind()==NativeArpEmissionService::Kind::RoutedEmit)word(a.values[6],savedPatch);a.flags=value8(a.flags,v.policy);
 }
 if(v.policy)a.flags=controlArithmetic::subtract(int8_t(v.policy),int8_t(64),a.flags).flags;
 if(s.release())projectNativeVoiceRelease(a,*s.release());if(s.program())projectNativeNoteProgram(a,*s.program());
 if(v.work){low(a.values[1],1);a.flags=value8(a.flags,1);}}
 if(s.kind()==NativeArpEmissionService::Kind::RoutedEmit){word(a.values[5],restoredNoteVelocity);a.flags=value8(a.flags,uint8_t(restoredNoteVelocity));}
}
template<class A>void projectNativeArpEmission(A&a,const NativeArpEmissionService&s){if(s.kind()!=NativeArpEmissionService::Kind::Off)throw std::logic_error("Routed emission requires caller logical retained note");projectNativeArpEmission(a,s,0);}
}
