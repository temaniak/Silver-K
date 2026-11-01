#pragma once
#include "ksStateDeferredPartsTask.h"
#include "ksStateVoiceRefreshService.h"
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
// Complete deferred five-part transaction. Working decisions and retained Voice
// outcomes are domain data; no guest registers, flags or execution frames exist.
class StateDeferredPartsService final {
public:
 enum class Kind {Deferred};struct Input{};enum class Component {Deferred,Voice};
 struct Effect {bool read=false,write=false;};
 struct Summary {uint8_t delay=0,delayAfter=0,pending=0,remainingMask=0,selected=0,processed=0,lastPart=0;bool pendingObserved=false,selectedObserved=false;uint64_t refreshed=0,voiceUploads=0;uint32_t partsMask=0;};
 StateDeferredPartsService()=default;explicit StateDeferredPartsService(Kind){}StateDeferredPartsService(Kind,Input){}
 Kind kind()const{return Kind::Deferred;}bool complete()const{return task_.complete();}bool critical()const{return false;}
 Component component()const{return voice_&&!voice_->complete()?Component::Voice:Component::Deferred;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 const StateVoiceRefreshService*voice()const{return voice_?&*voice_:nullptr;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,StateDeferredPartsTask>)return component()==Component::Deferred?&task_:nullptr;else if constexpr(std::is_same_v<T,StateVoiceRefreshService>)return component()==Component::Voice?voice():nullptr;else return component()==Component::Voice?voice_->template task<T>():nullptr;}
 template<class Peek>static bool admitted(Peek&&peek){if(uint8_t(peek(0x1679b)))return true;const auto pending=uint8_t(peek(0x1679a));for(unsigned part=0;part<5;++part)if(pending&(1u<<part)){uint32_t patch=0;for(unsigned n=0;n<4;++n)patch=(patch<<8)|uint8_t(peek(0x174ae + 20*part+n));if(!GlobalUpdateCoordinator::dataSpan(patch,256))return false;}return true;}
 unsigned nextClocks(int previous=-1)const{return complete()?0:component()==Component::Voice?voice_->nextClocks(previous):task_.nextClocks(byteClock(target(task_.stage()),previous));}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};components_|=1u<<unsigned(component());
  if(component()==Component::Voice){voice_->advanceBefore(cost,read,write,previous);effect_={voice_->effect().read,voice_->effect().write};if(voice_->complete()){++summary_.refreshed;summary_.voiceUploads+=voice_->summary().uploads;summary_.partsMask|=1u<<summary_.lastPart;if(!task_.resumeRefresh())throw std::logic_error("Deferred Voice child has no continuation");}return cost;}
  using T=StateDeferredPartsTask;using S=T::Stage;const auto stage=task_.stage();
  task_.advanceBefore(cost,[&](T::Read q){const auto a=q==T::Read::Delay?0x1679bu:q==T::Read::Pending?0x1679au:0x167bau;const auto value=uint8_t(read(a,cost));if(q==T::Read::Delay)summary_.delay=summary_.delayAfter=value;else if(q==T::Read::Pending){summary_.pendingObserved=true;summary_.pending=summary_.remainingMask=value;}else{summary_.selectedObserved=true;summary_.selected=value;}return value;},[&](T::Write q,uint8_t value){write(target(stage),value,cost);if(q==T::Write::Delay)summary_.delayAfter=value;},byteClock(target(stage),previous));
  if(stage==S::ShiftMask)summary_.remainingMask>>=1;if(stage==S::CountProcessed)++summary_.processed;
  effect_.write=stage==S::RetainSelected||stage==S::RetainPending||stage==S::RetainPart||stage==S::RetainProcessed||stage==S::RefreshPart;
  effect_.read=stage==S::RestoreProcessed||stage==S::RestorePart||stage==S::RestorePending||stage==S::RestoreSelected||stage==S::Return;
  if(task_.awaitingRefresh()){summary_.lastPart=uint8_t(task_.partNumber());voice_.emplace();}
  return cost;
 }
private:
 static unsigned byteClock(uint32_t a,int p){return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0u:3u;}
 static uint32_t target(StateDeferredPartsTask::Stage s){using S=StateDeferredPartsTask::Stage;switch(s){case S::WriteDelay:case S::WriteDelayCount:return 0x1679b;case S::WritePending:return 0x1679a;case S::WritePartNumber:return 0x17526;default:return 0x167ba;}}
 StateDeferredPartsTask task_;Summary summary_{};Effect effect_{};uint32_t components_=0;std::optional<StateVoiceRefreshService>voice_;
};}
