#include "ksStateEncoderForeground.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a,bool writing){return StateStoreService::dataSpan(a,1)||(!writing&&a>=0x800000&&a<0x900000)||(a>=0x1c0&&a<0x300)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xfffeb1||a==0xffff51||a==0xffff61||a==0xffff62||(a>=0xffff80&&a<=0xffffa6)||a==0xfffff6||a==0xfffff7;}
struct Resolver{StateFieldCoordinator::Handler field(uint8_t c,uint32_t p){constexpr unsigned a[]{0x4f4a,0x4f06,0x51d8,0x51f4,0x5252,0x4f22,0x51fe,0x4f0c,0xb554};if(c>=9||p!=a[c])throw std::logic_error("Unsupported encoder field binding");return StateFieldCoordinator::Handler(c);}bool schema(uint8_t c,uint32_t p){switch(c){case 0:return p==0x4f4a;case 2:return p==0x51d8;case 3:return p==0x51f4;case 4:return p==0x5252;case 6:return p==0x51fe;default:return false;}}StatePackFieldsTask::Kind operator()(uint32_t p){constexpr unsigned a[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((p&0xffffff)==a[n])return StatePackFieldsTask::Kind(n);throw std::logic_error("Unsupported encoder packing binding");}};
}
bool StateEncoderForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x18a00||!h.interruptMode2||h.indent<=0||h.getPC()!=0x443a)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;job_.emplace(StateEncoderService::Input{uint16_t(h.regs[6].r)},sp,h.indent,h.exr);++started;++kinds[0];}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);const auto start=h.cycles;Resolver resolve;
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a,false))throw std::logic_error("Unsupported encoder read endpoint");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t v,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a,true))throw std::logic_error("Unsupported encoder write endpoint");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},resolve,previous);
 if(!cost||used!=cost)throw std::logic_error("Encoder phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
