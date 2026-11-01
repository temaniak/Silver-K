#include "ksNativeKeyForeground.h"
#include "ksNativeKeyAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
std::optional<NativeKeyService::Kind>root(uint32_t p){using K=NativeKeyService::Kind;switch(p){case 0x30e4:return K::Physical;case 0x30fe:return K::Held;case 0x3074:return K::ResetHeld;case 0x30b8:return K::ResetDown;case 0x3024:return K::Compact;case 0x3118:return K::Priority;case 0x317a:return K::SearchNext;case 0x31ac:return K::SearchPrevious;default:return{};}}
}
void NativeKeyForeground::project(CallerAbi&a,const NativeKeyService&s){projectNativeKey(a,s);}
bool NativeKeyForeground::step(ControllerCpu&h){
 if(!job_){const auto kind=root(h.getPC());if(!kind||!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;if((*kind==NativeKeyService::Kind::SearchNext||*kind==NativeKeyService::Kind::SearchPrevious)&&(!NativeKeyService::dataSpan(uint32_t(h.regs[3].er))||!NativeKeyService::dataSpan(uint32_t(h.regs[3].er)+32)))return false;
  job_.emplace(*kind,NativeKeyService::Input{uint16_t(h.regs[5].r),uint32_t(h.regs[3].er)},sp,h.indent,h.exr);++started;++kinds[unsigned(*kind)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!NativeKeyService::dataSpan(a))throw std::logic_error("Unsupported native key read");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t v,unsigned dt){const auto a=raw&0xffffff;if(!NativeKeyService::dataSpan(a))throw std::logic_error("Unsupported native key write");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("Native key admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
