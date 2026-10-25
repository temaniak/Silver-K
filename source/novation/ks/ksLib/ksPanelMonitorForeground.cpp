#include "ksPanelMonitorForeground.h"
#include "ksPanelMonitorAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}}
bool PanelMonitorForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;
 using K=PanelMonitorService::Kind;K k;switch(h.getPC()){case 0x29a6:k=K::Clock;break;case 0x2262:k=K::Bar;break;case 0xbde:k=K::Version;break;case 0xbda:k=K::WithPatch;break;case 0xc00:k=K::Patch;break;case 0xb2c:k=K::Percent;break;default:return false;}
 const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;PanelMonitorService::Input i;i.presentation=uint16_t(h.regs[6].r);i.source=uint32_t(h.regs[6].er);i.level=uint8_t(i.presentation);i.patch=uint8_t(h.regs[5].rl);i.numerator=uint16_t(h.regs[0].r);i.divisor=uint16_t(h.regs[0].e);
 if((k==K::Version||k==K::WithPatch)&&!StateDisplayService::dataSpan(i.source,2)&&!((i.source&0xffffff)>=0x800000&&(i.source&0xffffff)<0x8fffff))return false;
 job_.emplace(k,i,sp,h.indent);++started;}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);
 auto read=[&](uint32_t a,unsigned dt){h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);};auto write=[&](uint32_t a,uint8_t v,unsigned dt){h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);};
 const auto used=s.advanceBefore(cost,read,write,previous);if(!cost||used!=cost)throw std::logic_error("Monitor phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;++phases;
 if(s.complete()){struct Abi{std::array<uint32_t,7>values{};uint8_t flags=0;}a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;projectPanelMonitor(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;
 uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
