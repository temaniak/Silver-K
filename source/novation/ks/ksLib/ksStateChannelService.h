#pragma once
#include "ksStateChannelTask.h"
#include <stdexcept>
#include <type_traits>
namespace ks {
class StateChannelService final {
public:
 enum class Kind{ModeStatus,GlobalMirror};
 struct Effect{bool read=false,write=false;};
 explicit StateChannelService(Kind k):kind_(k),task_(k==Kind::ModeStatus?StateChannelTask::Kind::ModeStatus:StateChannelTask::Kind::GlobalMirror){}
 Kind kind()const{return kind_;}bool complete()const{return task_.complete();}bool critical()const{return false;}
 const StateChannelTask&channelTask()const{return task_;}const Effect&effect()const{return effect_;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,StateChannelTask>)return &task_;else if constexpr(std::is_same_v<T,StateChannelService>)return this;else return nullptr;}
 unsigned nextClocks(int previous=-1)const{const auto a=task_.stage()==StateChannelTask::Stage::WriteMessage?0x167d6u:task_.stage()==StateChannelTask::Stage::WriteGlobalChannel?0x169b1u:0x174b9u;return task_.nextClocks(!(previous&1)&&a==(uint32_t(previous)|1u)?0:3);}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;using T=StateChannelTask;const auto stage=task_.stage();effect_={};
  task_.advanceBefore(cost,[&](T::Read f){const auto at=f==T::Read::Mode?0x167bau:f==T::Read::MultiStatus?0x167d5u:0x169eeu;return uint8_t(read(at,cost));},[&](T::Write f,uint8_t v){write(f==T::Write::Message?0x167d6u:f==T::Write::GlobalChannel?0x169b1u:0x174b9u,v,cost);},cost-(stage==T::Stage::WriteMessage?9:12));
  effect_.read=stage==T::Stage::ReturnMode||stage==T::Stage::ReturnGlobal;return cost;
 }
private:Kind kind_;StateChannelTask task_;Effect effect_{};
};}
