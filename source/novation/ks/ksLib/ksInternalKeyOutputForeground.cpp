#include "ksInternalKeyOutputForeground.h"
#include "ksInternalKeyOutputAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return(a>=0x15000&&a<0x30000)||(a>=0xffec00&&a<0xffefff)||a==0xffff8a;}
std::optional<InternalKeyOutputService::Kind>root(uint32_t p){using K=InternalKeyOutputService::Kind;switch(p){case 0xa8a2:return K::Set;case 0xa8b8:return K::Clear;case 0xa8ce:return K::Empty;case 0xa8e6:return K::Release;default:return{};}}
}
void InternalKeyOutputForeground::project(CallerAbi&a,const InternalKeyOutputService&s){projectInternalKeyOutput(a,s);}
bool InternalKeyOutputForeground::step(ControllerCpu&h){
 if(!job_){const auto kind=root(h.getPC());if(!kind||!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  const auto word=[&](uint32_t a){return(unsigned(h.nativeLowBase[a])<<8)|h.nativeLowBase[a+1];};if(word(0x15b04)>255||word(0x15b06)>255||h.memory[0xffed6c]||h.memory[0xffed6e])return false;
  const auto address=uint32_t(h.regs[0].er);unsigned part=0;if(*kind==InternalKeyOutputService::Kind::Set||*kind==InternalKeyOutputService::Kind::Clear){if(address<0x160d2||address>=0x160d2+256*334||(address-0x160d2)%334)return false;part=(address-0x160d2)/334;}
  job_.emplace(*kind,InternalKeyOutputService::Input{uint8_t(part),uint16_t(h.regs[5].r)},sp,h.indent,h.exr);++started;++kinds[unsigned(*kind)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported MIDI packet read");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t v,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported MIDI packet write");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("MIDI packet admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
