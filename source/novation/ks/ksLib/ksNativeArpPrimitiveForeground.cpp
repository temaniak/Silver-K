#include "ksNativeArpPrimitiveForeground.h"
#include "ksNativeArpPrimitiveAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return NativeMonoArpBootstrapService::span(a,1,false)||(a>=0xffec00&&a<0xffefff)||a==0xffff8c||a==0xffff61||a==0xffff62;}
}
bool NativeArpPrimitiveForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;using K=NativeArpPrimitiveService::Kind;K kind;
  switch(h.getPC()){case 0x36d2:kind=K::NullResult;break;case 0x36d6:kind=K::Tempo;break;case 0x3a38:kind=K::Reset;break;case 0x3a6e:kind=K::Pattern;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;uint8_t part=0;const auto record=uint32_t(h.regs[0].er);if(kind==K::Reset||kind==K::Pattern){if(record<0x160d2||record>=0x160d2+256*334||(record-0x160d2)%334)return false;part=uint8_t((record-0x160d2)/334);}job_.emplace(kind,NativeArpPrimitiveService::Input{part,uint8_t(h.regs[1].rl),uint32_t(h.regs[4].er)},sp,h.indent,h.exr);++started;++kinds[unsigned(kind)];}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto cost=s.nextClocks(h.lastwrite);
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned dt){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported mono bootstrap read endpoint");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned dt){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported mono bootstrap write endpoint");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},h.lastwrite);
 if(!cost||used!=cost)throw std::logic_error("Allocation phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=j.priority;++phases;components|=s.components();
 if(s.complete()){
  struct Abi{std::array<uint32_t,7>values{};uint8_t flags=0;}a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;projectNativeArpPrimitive(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }h.cycles=start+cost;return true;
}
}
