#pragma once
#include "ksLoaderInputsTask.h"
#include "ksLoaderSessionTask.h"
#include "ksStartupHardwareService.h"
#include <array>
#include <optional>
namespace ks {
// Real bounded-key sampling, debounce and stable/read/release ownership.
// Waiting for physical state remains interruptible and is never synthesized.
class StartupLoaderInputsService final {
public:
 using Kind=LoaderInputsTask::Kind;struct Effect{bool read=false,write=false;};
 explicit StartupLoaderInputsService(Kind k=Kind::Stable){push(k);}
 bool complete()const{return !depth_;}uint32_t sample()const{return sample_;}unsigned priority()const{return 0;}const Effect&effect()const{return effect_;}
 unsigned nextClocks(int previous=-1)const{return complete()?0:current().nextClocks(StartupHardwareService::byteClocks(writeTarget(current().stage()),previous));}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;using T=LoaderInputsTask;using S=T::Stage;auto&t=current();const auto stage=t.stage();effect_={true,false};
  t.advanceBefore(cost,[&](T::Read){return read(0xffff51,cost);},[&](T::Write q,uint8_t value){write(q==T::Write::Direction?0xfffeb1u:0xffff62u,value,cost);},StartupHardwareService::byteClocks(writeTarget(stage),previous));
  if(t.complete()){sample_=t.sample();frames_[--depth_].reset();if(depth_&&!current().resumeChild(sample_))throw std::logic_error("Startup key sample return");return cost;}
  if(t.awaitingChild()){Kind k;switch(stage){case S::ReadReleased:case S::ReadReleasedAgain:case S::ReadStable:case S::ReadStableAgain:k=Kind::Sample;break;case S::ReadPressed:k=Kind::Stable;break;case S::WaitRelease:k=Kind::Released;break;case S::WaitPress:k=Kind::Pressed;break;default:throw std::logic_error("Startup key child kind");}push(k);effect_.write=true;}
  return cost;
 }
private:
 static uint32_t writeTarget(LoaderInputsTask::Stage s){using S=LoaderInputsTask::Stage;return s==S::WriteInputDirection||s==S::WriteIdleDirection?0xfffeb1u:0xffff62u;}
 LoaderInputsTask&current(){return *frames_[depth_-1];}const LoaderInputsTask&current()const{return *frames_[depth_-1];}
 void push(Kind k){if(depth_==frames_.size())throw std::logic_error("Startup key ownership capacity");frames_[depth_++].emplace(k);}
 std::array<std::optional<LoaderInputsTask>,4>frames_{};unsigned depth_=0;uint32_t sample_=0;Effect effect_{};
};
// The ordinary cold probe runs the actual stable-input child. A physical
// firmware-update request is an explicit lifecycle capability, not playback.
class StartupLoaderProbeService final {
public:
 enum class Outcome{Running,Ignored,FirmwareUpdateRequested};struct Effect{bool read=false,write=false;};
 bool complete()const{return outcome_!=Outcome::Running;}Outcome outcome()const{return outcome_;}unsigned priority()const{return 0;}const Effect&effect()const{return effect_;}
 unsigned nextClocks(int previous=-1)const{return complete()?0:inputs_?inputs_->nextClocks(previous):policy_.nextClocks();}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};
  if(inputs_){inputs_->advanceBefore(cost,read,write,previous);effect_={inputs_->effect().read,inputs_->effect().write};if(inputs_->complete()){const auto sample=inputs_->sample();inputs_.reset();if(!policy_.resumeChild(sample,0,0,false))throw std::logic_error("Startup loader probe child return");}return cost;}
  const auto stage=policy_.stage();policy_.advanceBefore(cost,[](auto,auto){throw std::logic_error("Unexpected ordinary loader read");return 0u;},[](auto,auto,auto){throw std::logic_error("Unexpected ordinary loader publication");});effect_.read=true;
  if(policy_.awaitingChild()){if(stage!=LoaderSessionTask::Stage::ReadEntryInputs)throw std::logic_error("Unowned ordinary loader child");inputs_.emplace();effect_.write=true;}
  else if(policy_.complete())outcome_=Outcome::Ignored;else if(policy_.stage()==LoaderSessionTask::Stage::BindUpdateNotice)outcome_=Outcome::FirmwareUpdateRequested;return cost;
 }
private:LoaderSessionTask policy_{LoaderSessionTask::Kind::Update};std::optional<StartupLoaderInputsService>inputs_;Outcome outcome_=Outcome::Running;Effect effect_{};
};
}
