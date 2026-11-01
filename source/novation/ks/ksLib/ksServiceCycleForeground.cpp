#include "ksServiceCycleForeground.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
}
bool ServiceCycleForeground::step(ControllerCpu&h){
 if(!job_){if(!metadataReady_||!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x200000||!h.interruptMode2||h.indent!=0||h.getPC()!=0x4338)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffc00)return false;struct A{std::array<uint32_t,7>values{};uint8_t flags=0;}initial;for(unsigned n=0;n<7;++n)initial.values[n]=uint32_t(h.regs[n].er);initial.flags=h.ccr;NativeServiceCycle::Input in;in.presentation=uint16_t(h.regs[6].r);in.activation=uint8_t(h.regs[2].rh);in.expressionSource=uint32_t(h.regs[5].er);job_.emplace(in,sp,h.indent,h.exr,initial);++started;}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite,previousRead=h.lastread;const auto cost=s.nextClocks(previous,previousRead);const auto wasActive=s.coordinator().awaitingChild();const auto index=s.childIndex();ServiceCycleResolver resolver(metadata_);NativeServiceCycleBindings bindings;
 // Read-only telemetry runs at the real typed owner boundaries. It is an
 // external observer and never returns inputs or decisions to the pure graph.
 auto observe=[&](auto notice,const auto&owner,const auto&...extra){
  if constexpr(std::is_same_v<decltype(notice),NativeServiceCycle::Notice>){
   if(notice==NativeServiceCycle::Notice::Dispatch&&owner.child()==ServiceCycleCoordinator::Child::SlowControls)++tickReturns.started;
   if(notice==NativeServiceCycle::Notice::ChildComplete){if(const auto*tick=owner.template current<StateTickService>()){
    if(!tick->complete())throw std::logic_error("Main Tick completion telemetry before return");
    ++tickReturns.completed;++tickReturns.clock;tickReturns.components|=tick->components();
    if(tick->summary().due){++tickReturns.buttons;++tickReturns.adc;++tickReturns.encoder;++tickReturns.pedal;++tickReturns.refresh;if(tick->summary().sent)++tickReturns.output;}
   }}
  }
  j.external(notice,owner,extra...);
 };
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto at=raw&0xffffff;h.cycles=start+dt;h.lastread=history(at);return h.readResource(at);},[&](uint32_t raw,uint8_t value,unsigned dt){const auto at=raw&0xffffff;h.cycles=start+dt;h.lastwrite=history(at);h.writeResource(at,value);},resolver,bindings,observe,previous,previousRead);
 if(!cost||used!=cost)throw std::logic_error("Native central main phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.priority()==7?7:s.priority()==6?uint8_t(j.priority|6):j.priority;++phases;if(wasActive&&!s.coordinator().awaitingChild())++children[index];
 if(s.complete()){const auto&out=j.external.image();for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(out.values[n]);h.ccr=out.flags;h.exr=j.priority;h.pc=0x4338;
 // Final branch fetch history belongs only to the temporary outer ABI.
 h.lastread=0x4379;job_.reset();++completed;}h.cycles=start+cost;return true;
}
}
