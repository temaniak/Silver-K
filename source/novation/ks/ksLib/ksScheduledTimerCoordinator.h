#pragma once
#include "ksScheduledTimerTask.h"
#include "ksTimerServiceTask.h"
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
// The ordinary delayed-action registry is decoded once at the resource edge.
// This coordinator carries a typed callback request, never an executable PC.
class ScheduledTimerCoordinator final {
public:
 using Kind=ScheduledTimerTask::Kind;
 enum class Action{FieldClear,NoticeClear,StatusTick,MenuPrevious,MenuNext,OctaveDown,OctaveUp,BrowseSlow,BrowseStop,BrowseFast};
 enum class Component{Timer,Clock,Action};
 struct Input{uint16_t delay=0;Action action=Action::FieldClear;};
 struct Effect{bool read=false,write=false;};
 struct Summary{uint16_t interval=0,started=0,elapsed=0;uint32_t counter=0,actionWord=0;Action action=Action::FieldClear;bool clockObserved=false,fired=false;};
 ScheduledTimerCoordinator(Kind k,unsigned slot):ScheduledTimerCoordinator(k,slot,Input{}){}
 ScheduledTimerCoordinator(Kind k,unsigned slot,Input i):kind_(k),slot_(slot),input_(i),timer_(k,slot){if(slot>=5)throw std::logic_error("Unknown scheduled timer slot");}
 Kind kind()const{return kind_;}unsigned slot()const{return slot_;}const Input&input()const{return input_;}const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}Component component()const{return component_;}bool complete()const{return timer_.complete();}bool awaitingAction()const{return component_==Component::Action;}Action action()const{return summary_.action;}uint16_t actionPresentation()const{return uint16_t(summary_.actionWord);}uint32_t outcome()const{return outcome_;}
 static uint32_t binding(Action a){switch(a){case Action::FieldClear:return 0x5006;case Action::NoticeClear:return 0x55d4;case Action::StatusTick:return 0x55e2;case Action::MenuPrevious:return 0x46da;case Action::MenuNext:return 0x471a;case Action::OctaveDown:return 0x4a80;case Action::OctaveUp:return 0x4ac2;case Action::BrowseSlow:return 0x5e7c;case Action::BrowseStop:return 0x5ebe;case Action::BrowseFast:return 0x5e80;}throw std::logic_error("Unknown delayed action");}
 static Action decode(uint32_t raw){for(unsigned n=0;n<10;++n)if((raw&0xffffff)==binding(Action(n)))return Action(n);throw std::logic_error("Unsupported evolving delayed action binding");}
 bool resumeAction(uint32_t outcome){if(!awaitingAction())return false;outcome_=outcome;timer_.resumeAction();component_=Component::Timer;return true;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,ScheduledTimerTask>)return component_==Component::Timer?&timer_:nullptr;else if constexpr(std::is_same_v<T,ControllerClockReadTask>)return component_==Component::Clock?&*clock_:nullptr;else return nullptr;}
 unsigned nextClocks(int=-1)const{return complete()||awaitingAction()?0:component_==Component::Clock?clock_->nextClocks():timer_.nextClocks();}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int=-1){const auto cost=nextClocks();if(!cost||cost>budget)return 0;effect_={};
 if(component_==Component::Clock){const auto stage=clock_->stage();clock_->advanceBefore(cost,[&](){uint32_t value=0;for(unsigned n=0;n<4;++n)value=(value<<8)|uint8_t(read(0xffec10+n,9+3*n));summary_.clockObserved=true;return summary_.counter=value;});effect_.read=stage==ControllerClockReadTask::Stage::Return;if(clock_->complete()){timer_.resumeClock(uint16_t(summary_.counter));component_=Component::Timer;}return cost;}
 using T=ScheduledTimerTask;using S=T::Stage;const auto stage=timer_.stage();const auto address=[&](T::Field f){return 0x16758u+8*slot_+(f==T::Field::Started?0u:f==T::Field::Interval?2u:4u);};
 timer_.advanceBefore(cost,[&](T::Field f,bool argument)->uint32_t{if(argument)return f==T::Field::Action?binding(input_.action):input_.delay;const auto width=f==T::Field::Action?4u:2u;uint32_t value=0;for(unsigned n=0;n<width;++n)value=(value<<8)|uint8_t(read(address(f)+n,cost-3*((width-1-n)/2)));return value;},[&](T::Field f,uint32_t value){const auto width=f==T::Field::Action?4u:2u;for(unsigned n=0;n<width;++n)write(address(f)+n,uint8_t(value>>(8*(width-1-n))),cost-3*((width-1-n)/2));});
 effect_.write=stage==S::RetainInterval||stage==S::RetainClock||stage==S::DispatchClock||stage==S::DispatchAction;effect_.read=stage==S::ReleaseClock||stage==S::ReleaseInterval||stage==S::Finish;
 if(stage==S::ReadInterval)summary_.interval=timer_.interval();if(stage==S::ReadStarted)summary_.started=timer_.startedTime();if(stage==S::Elapsed)summary_.elapsed=timer_.elapsed();if(stage==S::ReadAction){summary_.actionWord=timer_.action().value;summary_.action=decode(summary_.actionWord);}
 if(timer_.awaitingClock()){clock_.emplace();component_=Component::Clock;}else if(timer_.awaitingAction()){summary_.fired=true;component_=Component::Action;}return cost;
 }
private:
 Kind kind_;unsigned slot_;Input input_;ScheduledTimerTask timer_;std::optional<ControllerClockReadTask>clock_;Component component_=Component::Timer;Effect effect_{};Summary summary_{};uint32_t outcome_=0;
};
}
