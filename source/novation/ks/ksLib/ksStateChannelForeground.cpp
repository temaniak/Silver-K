#include "ksStateChannelForeground.h"
#include "ksStateChannelAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return a==0x167ba||a==0x167d5||a==0x167d6||a==0x169ee||a==0x169b1||a==0x174b9;}
}
bool StateChannelForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;
  using K=StateChannelService::Kind;K kind;switch(h.getPC()){case 0x43a8:kind=K::ModeStatus;break;case 0x43c2:kind=K::GlobalMirror;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  job_.emplace(kind,sp,h.indent,h.exr);++started;++kinds[unsigned(kind)];}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned dt){if(!endpoint(a))throw std::logic_error("Unknown channel read endpoint");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned dt){if(!endpoint(a))throw std::logic_error("Unknown channel write endpoint");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("Panel mask phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;++phases;
 if(s.complete()){
  struct Abi{std::array<uint32_t,7>values{};uint8_t flags=0;}a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;projectStateChannel(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }h.cycles=start+cost;return true;
}
}
