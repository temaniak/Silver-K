#include "ksNativeParameterSupportForeground.h"
#include "ksNativeParameterSupportAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a,bool writing){return NativeParameterSupportService::span(a,1,writing);}
std::optional<NativeParameterSupportService::Kind>root(uint32_t p){using K=NativeParameterSupportService::Kind;switch(p){case 0x6da4:return K::Map;case 0x629e:return K::BindCurrent;case 0x6fcc:return K::BindPart;case 0x7364:return K::Dirty;case 0x6726:return K::Expand;case 0x673a:return K::Fields;default:return{};}}
}
void NativeParameterSupportForeground::project(CallerAbi&a,const NativeParameterSupportService&s){projectNativeParameterSupport(a,s);}
bool NativeParameterSupportForeground::step(ControllerCpu&h){
 if(!job_){const auto kind=root(h.getPC());if(!kind||!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;job_.emplace(*kind,NativeParameterSupportService::Input{uint8_t(h.regs[6].rl),uint8_t(h.regs[6].rh),uint8_t(h.regs[0].rl),uint32_t(h.regs[1].er),uint32_t(h.regs[5].er),uint32_t(h.regs[4].er)},sp,h.indent,h.exr);++started;}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a,false))throw std::logic_error("Unsupported arp reset read");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t v,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a,true))throw std::logic_error("Unsupported arp reset write");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("Arp reset admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
