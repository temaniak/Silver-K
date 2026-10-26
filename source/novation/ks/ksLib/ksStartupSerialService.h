#pragma once
#include "ksSerialSetupTask.h"
#include "ksSerialForegroundService.h"
#include <optional>
namespace ks {
class StartupSerialService final {
public:
 using Kind=SerialSetupTask::Kind;struct Effect{bool read=false,write=false;};
 explicit StartupSerialService(Kind kind):policy_(kind){}
 bool complete()const{return policy_.stage()==SerialSetupTask::Stage::Complete;}unsigned priority()const{return child_&&child_->critical()?7u:0u;}const Effect&effect()const{return effect_;}
 unsigned nextClocks(int previous=-1)const{return child_?child_->nextClocks():policy_.nextClocks([&](SerialSetupTask::Port p){return clock(p,previous);});}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};
  if(child_){child_->advanceBefore(cost,read,write);if(child_->complete()){child_.reset();if(!policy_.resumeChild())throw std::logic_error("Startup serial reset return");effect_.read=true;}return cost;}
  using T=SerialSetupTask;const auto stage=policy_.stage();effect_={true,stage==T::Stage::Hold};policy_.advanceBefore(cost,[&](T::Port p,uint8_t value){write(address(p),value,cost);},[&](T::Port p){return clock(p,previous);});
  if(policy_.awaiting()){child_.emplace(policy_.stage()==T::Stage::AwaitRx?SerialForegroundService::Kind::ResetRx:SerialForegroundService::Kind::ResetTx);effect_.write=true;}return cost;
 }
private:
 static uint32_t address(SerialSetupTask::Port p){return p==SerialSetupTask::Port::Control?0xffff8au:p==SerialSetupTask::Port::Mode?0xffff88u:0xffff89u;}
 static unsigned clock(SerialSetupTask::Port p,int previous){const auto a=address(p);return(a&1)&&previous==int(a-1)?0u:1u;}
 SerialSetupTask policy_;std::optional<SerialForegroundService>child_;Effect effect_{};
};
}
