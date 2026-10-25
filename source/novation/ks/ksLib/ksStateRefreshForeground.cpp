#include "ksStateRefreshForeground.h"
#include "ksStateRefreshAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool writeEndpoint(uint32_t a){return StateStoreService::dataSpan(a,1)||(a>=0x1c0&&a<0x300)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffff61||a==0xffff62||a==0xfffeb1;}
bool readEndpoint(uint32_t a,const StateRefreshService&s){return writeEndpoint(a)||(s.component()==StateRefreshService::Component::Deferred&&s.deferred()&&s.deferred()->component()==StateDeferredPartsService::Component::Voice&&StateVoiceRefreshService::dataSpan(a,1));}
struct Resolver{bool operator()(uint8_t c,uint32_t raw)const{switch(c){case 0:return raw==0x4f4a;case 2:return raw==0x51d8;case 3:return raw==0x51f4;case 4:return raw==0x5252;case 6:return raw==0x51fe;default:return false;}}};
}
void StateRefreshForeground::project(CallerAbi&a,const StateRefreshService&s){projectStateRefresh(a,s);}
bool StateRefreshForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x18a00||!h.interruptMode2||h.indent<=0||h.getPC()!=0x437a)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  const auto peek=[&](uint32_t raw){const auto a=raw&0xffffff;return a<h.nativeLowSize?h.nativeLowBase[a]:h.memory[a];};Resolver resolver;if(!StateRefreshService::admitted(peek,resolver))return false;job_.emplace(sp,h.indent);++started;++kinds[0];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);Resolver resolver;
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!readEndpoint(a,s))throw std::logic_error("Unsupported evolving refresh read endpoint");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t value,unsigned dt){const auto a=raw&0xffffff;if(!writeEndpoint(a))throw std::logic_error("Unsupported evolving refresh write endpoint");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,value);},resolver,previous);
 if(!cost||used!=cost)throw std::logic_error("Native refresh phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;++phases;components|=s.components();
 if(s.complete()){++returnedPacket;++returnedDeferred;++returnedClock;if(s.summary().panelRefreshed)++returnedPanel;processedParts+=s.deferred()->summary().processed;
  for(auto c:{ClockSyncService::Component::Coefficient,ClockSyncService::Component::PeriodAddress,ClockSyncService::Component::PeriodValue})if(const auto*t=s.clock()->transaction(c)){if(!t->complete())throw std::logic_error("Incomplete refresh Clock child");if(c==ClockSyncService::Component::Coefficient)++returnedCoefficient;else if(c==ClockSyncService::Component::PeriodAddress)++returnedPeriodAddress;else ++returnedPeriodValue;}
  CallerAbi result;for(unsigned n=0;n<7;++n)result.values[n]=uint32_t(h.regs[n].er);result.flags=h.ccr;project(result,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(result.values[n]);h.ccr=result.flags;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
