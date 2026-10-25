#include "ksPeriodicForeground.h"
#include "ksPeriodicAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return (a>=0x1532e&&a<=0x1534d)||(a>=0x159f2&&a<=0x159f3)||(a>=0x168fc&&a<=0x169fb)||a==0x1755c||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffec22||a==0xffff61||a==0xffff62;}
}
bool PeriodicForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;
  using K=PeriodicService::Kind;K kind;switch(h.getPC()){case 0x9fe4:kind=K::Mark;break;case 0xa008:kind=K::Fast;break;case 0xa00e:kind=K::Triggered;break;case 0xa028:kind=K::Manual;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  job_.emplace(kind,sp,h.indent,h.exr);++started;++kinds[unsigned(kind)];}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned dt){if(!endpoint(a))throw std::logic_error("Unknown periodic read endpoint");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned dt){if(!endpoint(a))throw std::logic_error("Unknown periodic write endpoint");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("Panel mask phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;++phases;
 if(s.complete()){
  struct Abi{std::array<uint32_t,7>values{};uint8_t flags=0;}a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;projectPeriodic(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }h.cycles=start+cost;return true;
}
}
