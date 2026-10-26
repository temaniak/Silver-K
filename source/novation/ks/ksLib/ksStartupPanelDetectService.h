#pragma once
#include "ksPanelDetectTask.h"
namespace ks {
class StartupPanelDetectService final {
public:
 struct Effect{bool read=false,write=false;};
 bool complete()const{return task_.stage()==PanelDetectTask::Stage::Complete;}
 unsigned priority()const{return 0;}const Effect&effect()const{return effect_;}
 bool present()const{return task_.present();}unsigned nextClocks(int=-1)const{return task_.nextClocks();}
 template<class Read,class Write>unsigned advanceBefore(unsigned budget,Read&&read,Write&&write,int=-1){
  const auto cost=nextClocks();if(!cost||budget<cost)return 0;using T=PanelDetectTask;using S=T::Stage;const auto stage=task_.stage();
  effect_={true,stage==S::HoldOuter||stage==S::HoldInner};
  return task_.advanceBefore(cost,[&]{return read(0xffff5d,cost);},[&](T::Write q,uint8_t value){write(q==T::Write::Presence?0xffec14u:q==T::Write::Gate?0xfffebdu:0xffff6du,value,cost);});
 }
private:PanelDetectTask task_;Effect effect_{};
};
}
