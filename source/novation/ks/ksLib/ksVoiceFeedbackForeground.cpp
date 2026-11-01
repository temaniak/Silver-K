#include "ksVoiceFeedbackForeground.h"
#include "ksVoiceFeedbackAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return (a>=0x174a6&&a<=0x174ab)||(a>=0x1755a&&a<0x18960)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0x159f2||a==0x159f3||a==0xffec22||a==0xffff61||a==0xffff62;}
}
void VoiceFeedbackForeground::project(CallerAbi&a,const VoiceFeedbackService&s){projectVoiceFeedback(a,s);}
bool VoiceFeedbackForeground::step(ControllerCpu&h){
 if(!job_){if((h.getPC()!=0x9f50&&h.getPC()!=0x9f5a)||!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;auto k=h.getPC()==0x9f50?VoiceFeedbackService::Kind::Reset:VoiceFeedbackService::Kind::Poll;job_.emplace(k,sp,h.indent,h.exr);++started;++kinds[unsigned(k)];}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto cost=s.nextClocks(h.lastwrite);
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned dt){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported feedback read endpoint");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned dt){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported feedback write endpoint");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},h.lastwrite);
 if(!cost||used!=cost)throw std::logic_error("Feedback phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=j.priority;++phases;components|=s.components();
 if(s.complete()){
  CallerAbi result;for(unsigned n=0;n<7;++n)result.values[n]=uint32_t(h.regs[n].er);result.flags=h.ccr;project(result,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(result.values[n]);h.ccr=result.flags;
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }h.cycles=start+cost;return true;
}
}
