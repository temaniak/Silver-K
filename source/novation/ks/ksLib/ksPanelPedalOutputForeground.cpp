#include "ksPanelPedalOutputForeground.h"
#include "ksPanelPedalOutputAbi.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {
bool PanelPedalOutputForeground::step(ControllerCpu&h){
 if(!job_){
  if(!h.nativeFetchClocksOnly||!h.nativeLowBase||!h.interruptMode2||h.indent<=0||h.nativeLowSize<0x30000)return false;
  PanelPedalOutputService::Kind kind;switch(uint32_t(h.getPC())){case 0x2a76:kind=PanelPedalOutputService::Kind::Pedal;break;case 0x2aea:kind=PanelPedalOutputService::Kind::Byte;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  const auto word=[&](unsigned a){return (unsigned(h.nativeLowBase[a])<<8)|h.nativeLowBase[a+1];};
  if(word(0x15b04)>255||word(0x15b06)>255||h.memory[0xffed6c]||h.memory[0xffed6e])return false;
  job_.emplace(kind,uint16_t(h.regs[6].r),sp,h.indent,h.exr);++started;++kinds[unsigned(kind)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite);const auto start=h.cycles;const auto previous=h.lastwrite;
 const auto history=[](uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;};
 const auto endpoint=[](uint32_t a,bool writing){a&=0xffffff;if(writing)return(a>=0x15b04&&a<=0x15c08)||(a>=0xffed6c&&a<=0xffee6f)||a==0xffff8a;return a==0x16798||a==0x169ed||(a>=0x174b9&&a<=0x188a5)||(a>=0x15b04&&a<=0x15b07)||(a>=0xffed6c&&a<=0xffed6f);};
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned dt){if(!endpoint(a,false))throw std::logic_error("Pedal read endpoint invalid");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned dt){if(!endpoint(a,true))throw std::logic_error("Pedal write endpoint invalid");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("Pedal phase differs");const auto effect=s.effect();if(effect.read)h.lastread=-1;if(effect.write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=s.components();
 if(s.complete()){
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}
  struct A{std::array<uint32_t,7>values;uint8_t flags;};A a{};for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;projectPanelPedalOutput(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;
  h.exr=j.priority;h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }h.cycles=start+cost;return true;
}
}
