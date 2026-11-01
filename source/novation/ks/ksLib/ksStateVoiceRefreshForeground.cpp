#include "ksStateVoiceRefreshForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {namespace {
int resourceHistory(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return StateVoiceRefreshService::dataSpan(a,1)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffff61||a==0xffff62;}
}
bool StateVoiceRefreshForeground::step(ControllerCpu&h){
 if(!job_){if(h.getPC()!=0x9662||!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x18a00||!h.interruptMode2||h.indent<=0)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  const auto peek=[&](uint32_t raw){const auto a=raw&0xffffff;return a<h.nativeLowSize?h.nativeLowBase[a]:h.memory[a];};uint32_t selected=0;const auto control=0x174aeu+20*peek(0x17526);for(unsigned n=0;n<4;++n)selected=(selected<<8)|peek(control+n);if(!GlobalUpdateCoordinator::dataSpan(selected,256))return false;
  job_.emplace(sp,h.indent,h.exr);++started;++kinds[0];}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite);const auto begin=h.cycles;const auto previous=h.lastwrite;
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported full voice read endpoint");h.cycles=begin+dt;h.lastread=resourceHistory(a);return h.readResource(a);},[&](uint32_t raw,uint8_t v,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported full voice write endpoint");h.cycles=begin+dt;h.lastwrite=resourceHistory(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("Native full voice phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=j.priority;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=begin+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;h.exr=j.priority;--h.indent;job_.reset();++completed;}h.cycles=begin+cost;return true;
}
}
