#pragma once
#include "ksControllerDeviceState.h"
#include "ksServiceCycleService.h"
#include "ksServiceCycleBindings.h"
#include "ksServiceCycleResolver.h"
#include "ksNativeArpOwnedService.h"
#include "ksMidiFileTransportService.h"
#include "ksInterruptScheduler.h"
#include <array>
#include <optional>
#include <stdexcept>

namespace ks {
class BoardNativeOwner;
// Owns semantic playback and peripheral continuations. Allocate once before
// worker execution: the fixed-capacity Main graph is intentionally large.
class NativeControllerExecutor final {
public:
 using Main=ServiceCycleService<ScheduledTimerService,NativeMidiPumpService,NativeArpOwnedService,MidiFileTransportService>;
 using Bindings=ServiceCycleBindings<Main,NativeArpOwnedService,MidiFileTransportService>;
 using Input=Main::Input;using Result=Main::Result;
 enum class Blocked {None,Metadata,ResourceMap,InterruptMode,Vector,Ring,Capacity,Mask};
private:
 friend class BoardNativeOwner;
 friend struct ArpArithmeticRouteTestAccess;
 struct IrqPlan {unsigned cost=0;int vector=-1;uint8_t priority=0,previousMask=0;InterruptService::Kind kind=InterruptService::Kind::Unused;Blocked blocked=Blocked::None;};
 struct PreparedPhase {enum class Phase {None,Main,Interrupt};Phase phase=Phase::None;unsigned cost=0;Blocked blocked=Blocked::None;IrqPlan irq{};};
public:
 struct Metrics {uint64_t initialMain=0,mainStarts=0,mainReturns=0,mainPhases=0,initialInterrupts=0,interruptStarts=0,interruptReturns=0,interruptPhases=0;std::array<uint64_t,17>children{};std::array<uint64_t,6>interruptKinds{};uint32_t timer=0,adc=0,receive=0;};
 bool bindVerifiedRom(const uint8_t*verified){
  if(metadataReady_||playback_||main_||interrupts_.active()||!verified)return false;
  ServiceCycleResolver::Metadata data;data.assign(verified);
  for(unsigned n=0;n<Vectors.size();++n){uint32_t target=0;for(unsigned b=0;b<4;++b)target=(target<<8)|verified[4*Vectors[n]+b];if((target&0xffffff)!=Targets[n])return false;}
  metadata_=data;metadataReady_=true;return true;
 }
 // Startup must call this only after its final reset has returned. A semantic
 // ready pulse alone is deliberately insufficient to start the Main graph.
 bool beginPlayback(Input in){if(!metadataReady_||playback_||main_||interrupts_.active())return false;input_=in;result_={in.presentation,in.activation,in.expressionSource};playback_=true;stop_=false;return true;}
 void requestBoundaryStop(){stop_=true;if(!main_)playback_=false;}
 bool playbackEnabled()const{return playback_;}bool activeMain()const{return bool(main_);}bool activeInterrupt()const{return interrupts_.active();}
 const Main*main()const{return main_?&*main_:nullptr;}const InterruptScheduler&interrupts()const{return interrupts_;}
 const Input&nextInput()const{return input_;}const Result&result()const{return result_;}const Metrics&metrics()const{return metrics_;}
 uint8_t semanticMask(uint8_t foreground=0)const{return interrupts_.active()?interrupts_.mask():uint8_t(foreground&7);}
 uint8_t mainMask()const{return main_?uint8_t(main_->priority()):0;}
 void resetMetrics(){metrics_={};metrics_.initialMain=bool(main_);metrics_.initialInterrupts=interrupts_.depth();}
 unsigned interruptNextClocks(const ControllerDeviceState&d,uint8_t foregroundMask)const{return prepareInterruptPhase(d,foregroundMask).cost;}
 Blocked interruptBlocked(const ControllerDeviceState&d,uint8_t foregroundMask)const{return prepareInterruptPhase(d,foregroundMask).blocked;}
 Blocked playbackBlocked(const ControllerDeviceState&d)const{if(!metadataReady_)return Blocked::Metadata;if(!d.interruptMode2)return Blocked::InterruptMode;if(!d.nativeLowBase||d.nativeLowSize<0x200000)return Blocked::ResourceMap;return Blocked::None;}
 unsigned nextClocks(const ControllerDeviceState&d)const{return preparePlaybackPhase(d).cost;}
 unsigned advanceBefore(ControllerDeviceState&d,unsigned available){const auto p=preparePlaybackPhase(d);return available<p.cost?0:consumePreparedPhase(d,p);}
 // Preparation and consumption stay in one call; external queries cannot leave
 // a retained plan that becomes stale when resources or IRQs change.
 unsigned advanceNextPhase(ControllerDeviceState&d){const auto p=preparePlaybackPhase(d);return consumePreparedPhase(d,p);}
 // The startup owner supplies its own semantic priority while it owns the
 // foreground. No architectural mask or saved machine frame crosses here.
 unsigned advanceInterruptBefore(ControllerDeviceState&d,unsigned available,uint8_t foregroundMask){const auto p=prepareInterruptPhase(d,foregroundMask);return available<p.cost?0:consumePreparedPhase(d,p);}
private:
 static constexpr std::array<unsigned,7>Vectors{{28,32,42,88,89,90,91}};
 static constexpr std::array<uint32_t,7>Targets{{0x165c,0x12b0,0x170,0x105e,0x1072,0x114e,0x170}};
 static constexpr std::array<InterruptService::Kind,7>Kinds{{InterruptService::Kind::Adc,InterruptService::Kind::Tpu0,InterruptService::Kind::Unused,InterruptService::Kind::SerialError,InterruptService::Kind::SerialReceive,InterruptService::Kind::SerialTransmit,InterruptService::Kind::Unused}};
 // These tokens never escape the owner/executor boundary. The owner must not
 // mutate controller resources between preparing and consuming an admitted phase.
 PreparedPhase prepareInterruptPhase(const ControllerDeviceState&d,uint8_t foregroundMask)const{
  const auto irq=irqPlan(d,foregroundMask);return {irq.cost?PreparedPhase::Phase::Interrupt:PreparedPhase::Phase::None,irq.cost,irq.blocked,irq};
 }
 // Common admission keeps priority/IRQ/blocked ordering identical for both paths.
 PreparedPhase preparePlaybackAdmission(const ControllerDeviceState&d)const{
  auto p=prepareInterruptPhase(d,mainMask());if(p.blocked!=Blocked::None||p.cost||!playback_)return p;
  p.blocked=playbackBlocked(d);if(p.blocked!=Blocked::None)return p;
  p.phase=PreparedPhase::Phase::Main;return p;
 }
 PreparedPhase preparePlaybackPhase(const ControllerDeviceState&d)const{
  auto p=preparePlaybackAdmission(d);if(p.phase==PreparedPhase::Phase::Main)
   p.cost=main_?main_->nextClocks(d.lastwrite,d.lastread):ServiceCycleTask{}.nextClocks();return p;
 }
 struct PreparedBoardPhase {PreparedPhase phase{};Main::ArithmeticRoute arithmetic{};};
 PreparedBoardPhase prepareBoardPhase(const ControllerDeviceState&d){
  PreparedBoardPhase p;p.phase=preparePlaybackAdmission(d);
  if(p.phase.phase!=PreparedPhase::Phase::Main)return p;
  if(main_)p.arithmetic=main_->prepareArithmeticRoute();
  p.phase.cost=p.arithmetic?p.arithmetic.cost():main_?main_->nextClocks(d.lastwrite,d.lastread):ServiceCycleTask{}.nextClocks();return p;
 }
 unsigned consumeBoardPhase(ControllerDeviceState&d,const PreparedBoardPhase&p){return consumePreparedPhase(d,p.phase,p.arithmetic?&p.arithmetic:nullptr);}

 unsigned consumePreparedPhase(ControllerDeviceState&d,const PreparedPhase&p,const Main::ArithmeticRoute*arithmetic=nullptr){
  if(p.blocked!=Blocked::None||!p.cost)return 0;
  if(p.phase==PreparedPhase::Phase::Interrupt)return advanceInterrupt(d,p.cost,p.irq);
  if(p.phase!=PreparedPhase::Phase::Main)return 0;
  const auto cost=p.cost;
  if(!main_){main_.emplace(input_);++metrics_.mainStarts;}
  auto&s=*main_;const auto start=d.cycles;const auto previous=d.lastwrite,previousRead=d.lastread;const auto index=s.childIndex();const bool awaiting=s.coordinator().awaitingChild();ServiceCycleResolver resolver(metadata_);Bindings bindings;
  NativeCompletionNoop observer;
  const auto read=[&](uint32_t raw,unsigned offset){const auto at=raw&0xffffff;d.cycles=start+offset;d.lastread=history(at);return d.readResource(at);};
  const auto write=[&](uint32_t raw,uint8_t value,unsigned offset){const auto at=raw&0xffffff;d.cycles=start+offset;d.lastwrite=history(at);d.writeResource(at,value);};
  const auto used=arithmetic?s.advanceArithmeticPhase(*arithmetic,cost,read,write,resolver,bindings,observer,previous,previousRead):s.advancePlannedPhase(cost,read,write,resolver,bindings,observer,previous,previousRead);
  if(used!=cost)throw std::logic_error("Native Main phase admission differs");
  if(s.effect().read)d.lastread=-1;if(s.effect().write)d.lastwrite=-1;++metrics_.mainPhases;
  if(awaiting&&!s.coordinator().awaitingChild())++metrics_.children[index];
  if(s.complete()){result_=s.result();input_={result_.presentation,result_.activation,result_.expressionSource};main_.reset();++metrics_.mainReturns;if(stop_)playback_=false;}
  d.cycles=start+cost;return cost;
 }
 static int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
 IrqPlan irqPlan(const ControllerDeviceState&d,uint8_t foregroundMask)const{
  IrqPlan p;if(!metadataReady_){p.blocked=Blocked::Metadata;return p;}if(foregroundMask>7){p.blocked=Blocked::Mask;return p;}
  if(!d.interruptMode2){p.blocked=Blocked::InterruptMode;return p;}
  const auto mask=semanticMask(foregroundMask);const auto selected=InterruptScheduler::select(d.pending_irqs,d.pending_irqs_high,d.interruptPriority,mask);
  if(selected.vector>=0){if(selected.priority>7){p.blocked=Blocked::Mask;return p;}unsigned n=0;while(n<Vectors.size()&&Vectors[n]!=unsigned(selected.vector))++n;if(n==Vectors.size()){p.blocked=Blocked::Vector;return p;}
   if(!d.nativeLowBase||unsigned(selected.vector)*4+4>d.nativeLowSize){p.blocked=Blocked::ResourceMap;return p;}
   uint32_t mappedTarget=0;for(unsigned byte=0;byte<4;++byte)mappedTarget=(mappedTarget<<8)|d.nativeLowBase[unsigned(selected.vector)*4+byte];if((mappedTarget&0xffffff)!=Targets[n]){p.blocked=Blocked::Vector;return p;}
   p.kind=Kinds[n];if((p.kind==InterruptService::Kind::SerialTransmit&&d.memory[0xffed6e])||(p.kind==InterruptService::Kind::Tpu0&&d.memory[0xffed6c])||(p.kind==InterruptService::Kind::SerialReceive&&d.memory[0xffec64])){p.blocked=Blocked::Ring;return p;}
   if(interrupts_.depth()==InterruptScheduler::Capacity){p.blocked=Blocked::Capacity;return p;}
   p.vector=selected.vector;p.priority=selected.priority;p.previousMask=mask;p.cost=InterruptScheduler::AdmissionClocks+InterruptService(p.kind).nextClocks();return p;
  }
  if(interrupts_.active())p.cost=interrupts_.nextClocks();return p;
 }
 unsigned advanceInterrupt(ControllerDeviceState&d,unsigned available,const IrqPlan&p){
  if(!p.cost||available<p.cost)return 0;
  const auto wholeStart=d.cycles;
  if(p.vector>=0){if(!interrupts_.admit(p.kind,p.priority,p.previousMask))throw std::logic_error("Native interrupt admission differs");if(p.vector<64)d.pending_irqs&=~(uint64_t(1)<<p.vector);else d.pending_irqs_high&=~(uint64_t(1)<<(p.vector-64));d.cycles+=InterruptScheduler::AdmissionClocks;d.lastwrite=-1;++metrics_.interruptStarts;++metrics_.interruptKinds[unsigned(p.kind)];}
  const auto start=d.cycles;const auto cost=p.cost-(p.vector>=0?InterruptScheduler::AdmissionClocks:0);
  const auto step=interrupts_.advancePlannedPhase(cost,[&](uint32_t raw,unsigned offset){const auto at=raw&0xffffff;d.cycles=start+offset;d.lastread=history(at);return d.readResource(at);},[&](uint32_t raw,uint8_t value,unsigned offset){const auto at=raw&0xffffff;d.cycles=start+offset;d.lastwrite=history(at);d.writeResource(at,value);});
  if(!cost||step.clocks!=cost)throw std::logic_error("Native interrupt phase differs");
  ++metrics_.interruptPhases;metrics_.timer|=step.timer;metrics_.adc|=step.adc;metrics_.receive|=step.receive;
  if(step.returned){d.lastread=-1;++metrics_.interruptReturns;}
  d.cycles=wholeStart+p.cost;return p.cost;
 }
 ServiceCycleResolver::Metadata metadata_{};bool metadataReady_=false,playback_=false,stop_=false;
 Input input_{};Result result_{};std::optional<Main>main_;InterruptScheduler interrupts_{};Metrics metrics_{};
};
}
