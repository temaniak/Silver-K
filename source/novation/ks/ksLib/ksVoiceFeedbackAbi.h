#pragma once
#include "ksVoiceFeedbackService.h"
#include "ksControllerDspAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
// Final external compatibility projection only. No running graph stores this.
template<class A>void projectVoiceFeedback(A&a,const VoiceFeedbackService&s){
 if(!s.complete())throw std::logic_error("Incomplete feedback projection");
 const auto&v=s.summary();
 const auto nz8=[](uint8_t x){return uint8_t((x&128?8:0)|(!x?4:0));};
 const auto nz16=[](uint16_t x){return uint8_t((x&0x8000?8:0)|(!x?4:0));};
 const auto word=[](uint32_t&x,uint16_t v){x=(x&0xffff0000u)|v;};
 if(s.kind()==VoiceFeedbackService::Kind::Reset){a.values[0]&=0xffffff00u;a.flags=uint8_t((a.flags&0xd0)|4);return;}
 word(a.values[0],uint16_t((unsigned(v.count)<<8)|v.cursor));
 a.values[3]=0x17560u+20*v.slot;
 a.flags=controlArithmetic::add(int32_t(20*v.slot),int32_t(0x17560),a.flags).flags;
 a.values[6]=(uint32_t(v.observedAge)<<16)|(a.values[6]&65535);
 a.flags=uint8_t((a.flags&0xf1)|nz16(v.observedAge));
 if(!v.observedAge)return;
 if(!s.selection()||!s.statusRead())throw std::logic_error("Missing feedback DSP outcomes");
 projectControllerDsp(a,*s.selection());word(a.values[4],0x8085);projectControllerDsp(a,*s.statusRead());
 if(v.activated){a.values[6]=0x7fffffff;a.flags=uint8_t(a.flags&0xf1);return;}
 if(v.ageRead){
  if(!s.ageRead())throw std::logic_error("Missing feedback age outcome");
  word(a.values[4],0x808f);projectControllerDsp(a,*s.ageRead());
  word(a.values[4],uint16_t(v.feedback>>1));
  a.flags=uint8_t((a.flags&0xf0)|(v.feedback&1)|nz16(v.age));
  a.values[6]=(uint32_t(v.age)<<16)|(a.values[6]&65535);
 }
 if(v.countdownRead){word(a.values[6],v.nextCountdown);a.flags=uint8_t((a.flags&0xf1)|nz16(v.nextCountdown));}
}
}
