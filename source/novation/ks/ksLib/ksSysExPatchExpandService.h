#pragma once
#include "ksPatchExpandTask.h"
#include "ksStateStoreService.h"
#include "ksControlArithmetic.h"
#include <stdexcept>
#include <type_traits>
namespace ks {
// Expansion of an ordinary compact received program into its full data page.
// Source bytes remain live at their original observation phases.
class SysExPatchExpandService final {
public:
 struct Input {uint32_t patch=0;};
 struct Effect {bool read=false,write=false;};
 struct Summary {uint8_t command=0;};
 explicit SysExPatchExpandService(Input in):input_(in),task_(in.patch){if(!StateStoreService::dataSpan(in.patch,256))throw std::logic_error("Unsupported SysEx expansion data span");}
 bool complete()const{return task_.complete();}bool critical()const{return false;}const Input&input()const{return input_;}const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,PatchExpandTask>)return complete()?nullptr:&task_;else return nullptr;}
 unsigned nextClocks(int previous=-1)const{return complete()?0:task_.nextClocks(task_.publication()?byteClock(target(task_.stage()),previous):3);}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};using T=PatchExpandTask;using S=T::Stage;const auto stage=task_.stage();
  task_.advanceBefore(cost,[&](S s)->uint32_t{const auto a=s==S::ReadTemplate?0xf428u+task_.index():s==S::ReadOscillator?input_.patch+5:s==S::ReadUnison?input_.patch+78:s==S::ReadFlags?input_.patch+79:s==S::ReadGate?input_.patch+80:s==S::ReadOffset?input_.patch+125:0x1603bu;if(!StateStoreService::dataSpan(a,1))throw std::logic_error("Unsupported evolving SysEx expansion read");const auto v=uint8_t(read(a&0xffffff,cost-(s==S::ReadTemplate?2:0)));if(s==S::ReadCommand)summary_.command=v;return v;},[&](S s,uint32_t value){const auto a=target(s);if(!StateStoreService::dataSpan(a,1))throw std::logic_error("Unsupported evolving SysEx expansion write");write(a&0xffffff,uint8_t(value),cost);},[&](S s){return byteClock(target(s),previous);});
  effect_.write=stage==S::RetainData||stage==S::RetainCursor||stage==S::RetainValue;effect_.read=stage==S::ReleaseData||stage==S::ReleaseCursor||stage==S::ReleaseValue||stage==S::Finish;return cost;
 }
private:
 static unsigned byteClock(uint32_t raw,int previous){const auto a=raw&0xffffff;return a<0x400000&&!(previous&1)&&a==(uint32_t(previous)|1u)?0:3;}
 uint32_t target(PatchExpandTask::Stage s)const{using S=PatchExpandTask::Stage;switch(s){case S::PublishTemplate:case S::PublishZero:return input_.patch+128+task_.index();case S::PublishOscillator0:return input_.patch+128;case S::PublishOscillator1:return input_.patch+129;case S::PublishOscillator2:return input_.patch+130;case S::PublishOscillator6:return input_.patch+5;case S::PublishUnison0:return input_.patch+131;case S::PublishUnison1:return input_.patch+132;case S::PublishFlags:return input_.patch+79;case S::PublishGate:return input_.patch+147;case S::PublishOffset:return input_.patch+125;case S::PublishLegacyField:return input_.patch+43;case S::PublishLegacyMode:return input_.patch+151;default:throw std::logic_error("Invalid SysEx expansion publication");}}
 Input input_;PatchExpandTask task_;Summary summary_{};Effect effect_{};
};
// The original whole-register saves belong only to the outer caller contract.
// The graph stores neither their values nor any internal CPU frame.
template<class A>void projectSysExPatchExpand(A&a,const SysExPatchExpandService&s){if(!s.complete())throw std::logic_error("Incomplete SysEx expansion projection");const auto v=a.values[0];a.flags=uint8_t((a.flags&0xd0)|(controlArithmetic::subtract(int8_t(s.summary().command),int8_t(64),0).flags&0x21)|(v&0x80000000?8:0)|(!v?4:0));}
}
