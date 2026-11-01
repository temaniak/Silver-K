#include "ksClockSyncForeground.h"
#include "ksClockSyncAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}}
void ClockSyncForeground::project(CallerAbi&a,const ClockSyncService&s){projectClockSync(a,s);}
bool ClockSyncForeground::step(ControllerCpu&h){
 if(!job_){if(h.getPC()!=0x267a||!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x18a00||!h.interruptMode2||h.indent<=0)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;job_.emplace(sp,h.indent);++started;++kinds[0];}
 auto&j=*job_;if(j.depth!=h.indent)return false;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=j.work.nextClocks(previous);
 const auto used=j.work.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t v,unsigned dt){const auto a=raw&0xffffff;h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("Clock-sync foreground phase admission differs");if(j.work.effect().read)h.lastread=-1;if(j.work.effect().write)h.lastwrite=-1;++phases;components|=j.work.components();
 if(j.work.complete()){CallerAbi result;for(unsigned n=0;n<7;++n)result.values[n]=uint32_t(h.regs[n].er);result.flags=h.ccr;project(result,j.work);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(result.values[n]);h.ccr=result.flags;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
