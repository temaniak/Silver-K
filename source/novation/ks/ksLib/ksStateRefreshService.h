#pragma once
#include "ksStateForegroundTask.h"
#include "ksControllerDspService.h"
#include "ksStateReloadPanelService.h"
#include "ksStateDeferredPartsService.h"
#include "ksClockSyncService.h"
#include <optional>
#include <type_traits>
#include <stdexcept>
namespace ks {
// One complete refresh transaction. Current resources, decisions and native
// child outcomes are semantic data; no controller execution frame is retained.
class StateRefreshService final {
public:
 enum class Kind{Refresh};struct Input{};
 enum class Component{Refresh,Packet,Panel,Deferred,Clock};
 struct Effect{bool read=false,write=false;};
 struct PanelFacts{PanelControlTask indicator;uint8_t input=0,set=0;};
 struct Summary{uint8_t refreshFlags=0,dirty=0;bool panelRefreshed=false;};
 StateRefreshService():StateRefreshService(Kind::Refresh,{}){}
 explicit StateRefreshService(Kind k):StateRefreshService(k,{}){}
 StateRefreshService(Kind,Input){}
 Kind kind()const{return Kind::Refresh;}bool complete()const{return refresh_.complete();}
 Component component()const{return component_;}uint32_t components()const{return components_;}
 const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 bool critical()const{return component_==Component::Deferred?deferred_->critical():component_==Component::Clock?clock_->critical():false;}
 const ControllerDspService*packet()const{return packet_?&*packet_:nullptr;}
 const StateReloadPanelService*panel()const{return panel_?&*panel_:nullptr;}
 const StateDeferredPartsService*deferred()const{return deferred_?&*deferred_:nullptr;}
 const ClockSyncService*clock()const{return clock_?&*clock_:nullptr;}
 const PanelFacts&panelFacts()const{return panelFacts_;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,StateForegroundTask>)return component_==Component::Refresh?&refresh_:nullptr;
  if constexpr(std::is_same_v<T,StateReloadPanelService>)return component_==Component::Panel?panel():nullptr;
  if constexpr(std::is_same_v<T,StateDeferredPartsService>)return component_==Component::Deferred?deferred():nullptr;
  if constexpr(std::is_same_v<T,ClockSyncService>)return component_==Component::Clock?clock():nullptr;
  if constexpr(std::is_same_v<T,ControllerDspService>||std::is_same_v<T,DspRoutingTask>||std::is_same_v<T,DspAddressTask>||std::is_same_v<T,DspWordTask>||std::is_same_v<T,DspFaultTask>){if(component_==Component::Packet){if constexpr(std::is_same_v<T,ControllerDspService>)return packet();else return packet_->template task<T>();}}
  if(component_==Component::Deferred)return deferred_->template task<T>();
  if(component_==Component::Clock)return clock_->template task<T>();
  return nullptr;
 }
 template<class Peek,class Resolver>static bool admitted(Peek&&peek,Resolver&&resolver){if(!StateDeferredPartsService::admitted(peek))return false;if(uint8_t(peek(0x1679c))){uint32_t base=0;for(unsigned n=0;n<4;++n)base=(base<<8)|uint8_t(peek(0x167bc+n));if(!StateStoreService::dataSpan(base,256))return false;}return StateReloadPanelService::admittedSchema(peek,[&](uint8_t c,uint32_t raw){return resolver(c,raw);});}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;switch(component_){case Component::Refresh:return refresh_.nextClocks(byteClock(0x1679c,previous));case Component::Packet:return packet_->nextClocks(previous);case Component::Panel:return panel_->nextClocks(previous);case Component::Deferred:return deferred_->nextClocks(previous);case Component::Clock:return clock_->nextClocks(previous);}return 0;}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolver,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  if(component_==Component::Refresh){using T=StateForegroundTask;using S=T::Stage;const auto stage=refresh_.stage();
   refresh_.advanceBefore(cost,[&](T::Read field)->uint16_t{if(field==T::Read::RefreshFlags)return summary_.refreshFlags=uint8_t(read(0x167d3,cost));if(field==T::Read::Dirty)return summary_.dirty=uint8_t(read(0x1679c,cost));throw std::logic_error("Unexpected refresh resource read");},[&](T::Write field,uint16_t value){if(field!=T::Write::Dirty)throw std::logic_error("Unexpected refresh resource write");write(0x1679c,uint8_t(value),cost);},byteClock(0x1679c,previous));
   if(stage==S::ReturnRefresh)effect_.read=true;
   if(refresh_.awaitingChild()){effect_.write=true;switch(stage){
    case S::SendRefreshWord:{ControllerDspService::Input in;in.routing=DspRoutingTask::Kind::WriteA;in.address=0x8056;in.word=summary_.refreshFlags;packet_.emplace(ControllerDspService::Kind::Routing,in);select(Component::Packet);break;}
    case S::RefreshDirty:summary_.panelRefreshed=true;panel_.emplace();select(Component::Panel);break;
    case S::RefreshParts:deferred_.emplace();select(Component::Deferred);break;
    case S::RefreshClock:clock_.emplace();select(Component::Clock);break;
    default:throw std::logic_error("Unexpected refresh child");}}
  }else if(component_==Component::Packet){packet_->advanceBefore(cost,read,write,previous);effect_={packet_->effect().read,packet_->effect().write};if(packet_->complete())resume();
  }else if(component_==Component::Panel){if(const auto*t=panel_->indicatorTask()){panelFacts_.indicator=*t;panelFacts_.input=uint8_t(panel_->panelTask().value());if(t->stage()==PanelControlTask::Stage::CompareToggle)panelFacts_.set=t->set();}panel_->advanceBefore(cost,read,write,resolver,previous);effect_={panel_->effect().read,panel_->effect().write};if(panel_->complete())resume();
  }else if(component_==Component::Deferred){deferred_->advanceBefore(cost,read,write,previous);effect_={deferred_->effect().read,deferred_->effect().write};if(deferred_->complete())resume();
  }else {clock_->advanceBefore(cost,read,write,previous);effect_={clock_->effect().read,clock_->effect().write};if(clock_->complete())resume();}
  return cost;
 }
private:
 static unsigned byteClock(uint32_t a,int previous){return a<0x400000&&!(previous&1)&&a==(uint32_t(previous)|1u)?0u:3u;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void resume(){if(!refresh_.resumeChild(0,false))throw std::logic_error("Invalid refresh continuation");select(Component::Refresh);}
 StateForegroundTask refresh_{StateForegroundTask::Kind::Refresh};Component component_=Component::Refresh;uint32_t components_=1;Summary summary_{};PanelFacts panelFacts_{};Effect effect_{};
 std::optional<ControllerDspService>packet_;std::optional<StateReloadPanelService>panel_;std::optional<StateDeferredPartsService>deferred_;std::optional<ClockSyncService>clock_;
};}
