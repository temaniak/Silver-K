#pragma once
#include "ksServiceCycleTask.h"
#include <array>
#include <stdexcept>
#include <type_traits>
namespace ks {
// Exactly one background iteration. Complete jobs release at the original
// iteration boundary; no architectural continuation is retained by this policy.
class ServiceCycleCoordinator final {
public:
using Child=ServiceCycleTask::Child;using OutcomeHandle=uint32_t;static constexpr OutcomeHandle NoOutcome=~0u;
struct Input{uint16_t presentation=0;uint8_t activation=0;uint32_t expressionSource=0;};
struct Result{uint16_t presentation=0;uint8_t activation=0;uint32_t expressionSource=0;};
struct ChildResult{OutcomeHandle outcome=NoOutcome;bool presentationKnown=false;uint16_t presentation=0;bool activationKnown=false;uint8_t activation=0;uint32_t expressionSource=0,expressionSourceMask=0;};
struct Effect{bool read=false,write=false;};
ServiceCycleCoordinator():ServiceCycleCoordinator(Input{}){}explicit ServiceCycleCoordinator(Input i):input_(i),result_{i.presentation,i.activation,i.expressionSource}{outcomes_.fill(NoOutcome);}
const Input&input()const{return input_;}const Result&result()const{return result_;}const Effect&effect()const{return effect_;}bool complete()const{return complete_;}bool awaitingChild()const{return !complete_&&task_.stage()==ServiceCycleTask::Stage::AwaitChild;}ServiceCycleTask::Stage policyStage()const{return task_.stage();}Child child()const{return task_.child();}unsigned index()const{return task_.index();}OutcomeHandle outcome(unsigned index)const{if(index>=outcomes_.size())throw std::logic_error("Main service outcome index");return outcomes_[index];}
template<class T>const T*task()const{if constexpr(std::is_same_v<T,ServiceCycleTask>)return &task_;else return nullptr;}
unsigned nextClocks(int=-1)const{return complete_?0:task_.nextClocks();}
unsigned advanceBefore(unsigned budget){const auto cost=nextClocks();if(!cost||cost>budget)return 0;effect_={};const auto stage=task_.stage();task_.advanceBefore(cost);if(stage==ServiceCycleTask::Stage::Dispatch)effect_.write=true;else complete_=true;return cost;}
bool resume(ChildResult r){if(!awaitingChild())return false;outcomes_[task_.index()]=r.outcome;if(r.presentationKnown)result_.presentation=r.presentation;if(r.activationKnown)result_.activation=r.activation;result_.expressionSource=(result_.expressionSource&~r.expressionSourceMask)|(r.expressionSource&r.expressionSourceMask);return task_.resumeChild();}
private:Input input_;Result result_;Effect effect_{};ServiceCycleTask task_;std::array<OutcomeHandle,17>outcomes_{};bool complete_=false;
};}
