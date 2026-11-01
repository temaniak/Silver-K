#pragma once
#include "ksParameterScale.h"
namespace ks {
class ControllerCoefficientService final {
public:
 struct Effect{bool read=false,write=false;};
 struct Summary{int16_t encoded=0;int32_t coefficient=0;};
 explicit ControllerCoefficientService(int16_t encoded):task_(encoded),summary_{encoded,task_.result().coefficient}{}
 bool complete()const{return complete_;}int32_t value()const{return summary_.coefficient;}
 unsigned nextClocks()const{return complete_?0:task_.complete()?17:task_.nextBoundaryClocks();}
 unsigned advanceBefore(unsigned available){const auto cost=nextClocks();if(!cost||available<cost)return 0;effect_={};if(task_.complete()){complete_=true;effect_.read=true;}else task_.advanceBefore(cost);return cost;}
 const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}const ParameterScaleTask*task()const{return &task_;}
private:ParameterScaleTask task_;Summary summary_;Effect effect_{};bool complete_=false;
};
}
