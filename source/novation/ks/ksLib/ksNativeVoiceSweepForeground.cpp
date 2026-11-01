#include "ksNativeVoiceSweepForeground.h"
#include "ksNativeVoiceSweepAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return NativeVoiceSweepService::dataSpan(a,1)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffff61||a==0xffff62;}
}
bool NativeVoiceSweepForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;using K=NativeVoiceSweepService::Kind;K kind;
  switch(h.getPC()){case 0x9484:kind=K::ReleaseAll;break;case 0x94ea:kind=K::RetunePart;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;job_.emplace(kind,NativeVoiceSweepService::Input{},sp,h.indent,h.exr);++started;++kinds[unsigned(kind)];}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto cost=s.nextClocks(h.lastwrite);
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned dt){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported voice sweep read endpoint");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned dt){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported voice sweep write endpoint");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},h.lastwrite);
 if(!cost||used!=cost)throw std::logic_error("Allocation phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=j.priority;++phases;components|=s.components();
 if(s.complete()){
  struct Abi{std::array<uint32_t,7>values{};uint8_t flags=0;}a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;projectNativeVoiceSweep(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }h.cycles=start+cost;return true;
}
}
