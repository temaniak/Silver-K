#include "ksNativeVoiceModulationForeground.h"
#include "ksNativeVoiceModulationAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return NativeVoiceModulationService::dataSpan(a,1)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffff61||a==0xffff62;}
}
bool NativeVoiceModulationForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;using K=NativeVoiceModulationService::Kind;K kind;
  switch(h.getPC()){case 0x9e42:kind=K::Cutoff;break;case 0x9e62:kind=K::First;break;case 0x9e82:kind=K::Second;break;case 0x9ea2:kind=K::Square;break;case 0x958e:kind=K::Oscillators;break;case 0x95f4:kind=K::Filter;break;case 0x7872:kind=K::Modulation;break;case 0x7b48:kind=K::Pitch;break;case 0x7a9c:kind=K::Global;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;const auto patch=uint32_t(h.regs[6].er);if((kind==K::Modulation||kind==K::Pitch)&&(!NativeVoiceModulationService::dataSpan(patch,256)||(h.regs[5].er!=0x400000&&h.regs[5].er!=0x600000)))return false;job_.emplace(kind,NativeVoiceModulationService::Input{uint8_t(h.regs[5].rl),(kind==K::Modulation||kind==K::Pitch)?patch:0,h.regs[5].er==0x400000?DspPort::A:DspPort::B,uint16_t(h.regs[4].r)},sp,h.indent,h.exr);++started;++kinds[unsigned(kind)];}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto cost=s.nextClocks(h.lastwrite);
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned dt){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported voice modulation read endpoint");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned dt){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported voice modulation write endpoint");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},h.lastwrite);
 if(!cost||used!=cost)throw std::logic_error("Allocation phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=j.priority;++phases;components|=s.components();
 if(s.complete()){
  struct Abi{std::array<uint32_t,7>values{};uint8_t flags=0;}a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;projectNativeVoiceModulation(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }h.cycles=start+cost;return true;
}
}
