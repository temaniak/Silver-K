#pragma once
#include "ksArpSelectTask.h"
#include "ksTimerServiceTask.h"
#include <optional>
#include <type_traits>
#include <stdexcept>
namespace ks {
class NativeArpRandomService final {
public:
 enum class Component{Select,Clock};struct Input{uint8_t part=0;};struct Effect{bool read=false,write=false;};struct Summary{Input input{};uint32_t clock=0,product=0;uint16_t seed=0,increment=0,published=0,index=0;uint8_t count=0;};
 explicit NativeArpRandomService(Input in):select_(ArpSelectTask::Kind::Random,in.part){summary_.input=in;}
 bool complete()const{return complete_;}bool critical()const{return false;}Component component()const{return component_;}uint32_t components()const{return clock_?3:1;}const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}const ArpSelectTask&selection()const{return select_;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,NativeArpRandomService>)return this;else if constexpr(std::is_same_v<T,ArpSelectTask>)return component_==Component::Select?&select_:nullptr;else if constexpr(std::is_same_v<T,ControllerClockReadTask>)return component_==Component::Clock?&*clock_:nullptr;else return nullptr;}
 unsigned nextClocks(int=-1)const{return complete_?0:component_==Component::Clock?clock_->nextClocks():select_.nextClocks();}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int=-1){const auto cost=nextClocks();if(!cost||cost>budget)return 0;effect_={};if(component_==Component::Clock){const auto stage=clock_->stage();clock_->advanceBefore(cost,[&]{uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(read(0xffec10+n,cost-3*(3-n)));return v;});effect_.read=stage==ControllerClockReadTask::Stage::Return;if(clock_->complete()){summary_.clock=clock_->counter();select_.resumeClock(summary_.clock);component_=Component::Select;}return cost;}
  using T=ArpSelectTask;using S=T::Stage;const auto stage=select_.stage();select_.advanceBefore(cost,[&](S q)->uint32_t{if(q==S::RandomReadCount)return summary_.count=uint8_t(read(0x160d2u+334*summary_.input.part+307,cost));const auto at=q==S::RandomReadSeed?0x15c30u:q==S::RandomReadIncrement?0x15c2eu:0;if(!at)throw std::logic_error("Unsupported arp random observation");const auto v=uint16_t((unsigned(uint8_t(read(at,cost)))<<8)|uint8_t(read(at+1,cost)));if(q==S::RandomReadSeed)summary_.seed=v;else summary_.increment=v;return v;},[&](S q,uint32_t v){if(q!=S::RandomPublishSeed)throw std::logic_error("Unsupported arp random publication");summary_.published=uint16_t(v);write(0x15c30,uint8_t(v>>8),cost);write(0x15c31,uint8_t(v),cost);},[](S){return 3u;});effect_.write=stage==S::RandomClock;effect_.read=stage==S::RandomFinish;if(select_.awaitingChild()){clock_.emplace();component_=Component::Clock;}else if(select_.complete()){summary_.product=uint16_t(summary_.clock)*uint32_t(summary_.seed);summary_.index=select_.randomIndex();complete_=true;}return cost;
 }
private:ArpSelectTask select_;Summary summary_{};Effect effect_{};Component component_=Component::Select;std::optional<ControllerClockReadTask>clock_;bool complete_=false;
};
}
