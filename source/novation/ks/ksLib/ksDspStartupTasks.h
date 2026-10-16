#pragma once
#include <cstdint>
namespace ks {
// Calibrated settling interval. Native loop counters retain observable timer
// admission points until the remaining caller scheduling is migrated.
class DspSettlingTask final {
public:
 enum class Stage {Admission,OuterSetup,InnerAdmission,InnerSetup,Tick,Gate,InnerRelease,OuterTick,OuterGate,Release,Complete};
 Stage stage()const{return stage_;}
 unsigned outer()const{return outer_;}unsigned inner()const{return inner_;}
 unsigned nextClocks()const{
  switch(stage_){case Stage::Admission:case Stage::InnerAdmission:case Stage::InnerRelease:case Stage::Release:return 11;
  case Stage::OuterSetup:case Stage::InnerSetup:return 6;case Stage::Complete:return 0;default:return 3;}
 }
 bool complete()const{return stage_==Stage::Complete;}
 void advance(){switch(stage_){
  case Stage::Admission:stage_=Stage::OuterSetup;break;
  case Stage::OuterSetup:outer_=10;stage_=Stage::InnerAdmission;break;
  case Stage::InnerAdmission:stage_=Stage::InnerSetup;break;
  case Stage::InnerSetup:inner_=2663;stage_=Stage::Tick;break;
  case Stage::Tick:--inner_;stage_=Stage::Gate;break;
  case Stage::Gate:stage_=inner_?Stage::Tick:Stage::InnerRelease;break;
  case Stage::InnerRelease:stage_=Stage::OuterTick;break;
  case Stage::OuterTick:--outer_;stage_=Stage::OuterGate;break;
  case Stage::OuterGate:stage_=outer_?Stage::InnerAdmission:Stage::Release;break;
  case Stage::Release:stage_=Stage::Complete;break;case Stage::Complete:break;
 }}
private:Stage stage_=Stage::Admission;unsigned outer_=0,inner_=0;
};
// One live control read is latched across both delays, exactly as on the
// admitted board. The callback publishes only the original control bits; this
// task adds no reset effect to the DSP implementation or JIT.
class DspResetTask final {
public:
 enum class Stage {Admission,ReadControl,Clear,WriteLow,SettleLow,Set,WriteHigh,SettleHigh,Release,Complete};
 struct Advance {unsigned clocks=0,boundaries=0;};
 Stage stage()const{return stage_;}const DspSettlingTask& settling()const{return settling_;}
 uint8_t control()const{return control_;}uint64_t elapsedClocks()const{return elapsed_;}
 bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks()const{
  switch(stage_){case Stage::Admission:case Stage::Release:return 11;
  case Stage::ReadControl:case Stage::WriteLow:case Stage::WriteHigh:return 4;
  case Stage::SettleLow:case Stage::SettleHigh:return settling_.nextClocks();
  case Stage::Complete:return 0;default:return 3;}
 }
 template<class Reader,class Writer>Advance advanceBefore(unsigned available,Reader&& read,Writer&& write){
  Advance p;while(const unsigned cost=nextClocks()){
   if(cost>available)break;
   switch(stage_){
   case Stage::Admission:stage_=Stage::ReadControl;break;
   case Stage::ReadControl:control_=uint8_t(read());stage_=Stage::Clear;break;
   case Stage::Clear:control_&=uint8_t(~8u);stage_=Stage::WriteLow;break;
   case Stage::WriteLow:write(control_);stage_=Stage::SettleLow;break;
   case Stage::SettleLow:case Stage::SettleHigh:{
    const bool low=stage_==Stage::SettleLow;settling_.advance();
    if(settling_.complete()){stage_=low?Stage::Set:Stage::Release;settling_=DspSettlingTask{};}break;
   }
   case Stage::Set:control_|=8;stage_=Stage::WriteHigh;break;
   case Stage::WriteHigh:write(control_);stage_=Stage::SettleHigh;break;
   case Stage::Release:stage_=Stage::Complete;break;case Stage::Complete:break;
   }
   available-=cost;p.clocks+=cost;++p.boundaries;elapsed_+=cost;
  }return p;
 }
private:Stage stage_=Stage::Admission;DspSettlingTask settling_;uint8_t control_=0;uint64_t elapsed_=0;
};
enum class DspPort {A,B};
// A selection always invalidates the sequential address mirror. Its callback
// owns publication; no processor state is used to decide which port is active.
class DspSelectTask final {
public:
 enum class Stage {Select,Invalidate,Publish,Complete};
 explicit DspSelectTask(DspPort port):port_(port){}
 Stage stage()const{return stage_;}DspPort port()const{return port_;}
 static constexpr uint16_t invalidAddress(){return 0xffff;}
 unsigned nextClocks()const{return stage_==Stage::Select?9:stage_==Stage::Invalidate?6:stage_==Stage::Publish?12:0;}
 template<class Writer>unsigned advanceBefore(unsigned available,Writer&& publish){
  unsigned total=0;while(const unsigned cost=nextClocks()){
   if(cost>available)break;
   switch(stage_){case Stage::Select:stage_=Stage::Invalidate;break;
   case Stage::Invalidate:stage_=Stage::Publish;break;
   case Stage::Publish:publish(port_,invalidAddress());stage_=Stage::Complete;break;case Stage::Complete:break;}
   total+=cost;available-=cost;
  }return total;
 }
private:DspPort port_;Stage stage_=Stage::Select;
};
// Orchestration of the two original firmware images. A child is acknowledged
// only on completion; no speculative source or DSP observations are made here.
class DspStartupTask final {
public:
 enum class Stage {Reset,AwaitReset,PortA,ImageA,SetupA,AwaitA,PortB,ImageB,SetupB,AwaitB,SelectA,AwaitSelect,Complete};
 enum class Request {None,Reset,SetupA,SetupB,SelectA};
 Stage stage()const{return stage_;}
 Request request()const{
  switch(stage_){case Stage::AwaitReset:return Request::Reset;case Stage::AwaitA:return Request::SetupA;
  case Stage::AwaitB:return Request::SetupB;case Stage::AwaitSelect:return Request::SelectA;default:return Request::None;}
 }
 bool resumeChild(){switch(stage_){case Stage::AwaitReset:stage_=Stage::PortA;return true;
  case Stage::AwaitA:stage_=Stage::PortB;return true;case Stage::AwaitB:stage_=Stage::SelectA;return true;
  case Stage::AwaitSelect:stage_=Stage::Complete;return true;default:return false;}}
 unsigned nextClocks()const{
  switch(stage_){case Stage::Reset:case Stage::SelectA:return 20;case Stage::PortA:case Stage::ImageA:case Stage::PortB:case Stage::ImageB:return 9;
  case Stage::SetupA:case Stage::SetupB:return 15;default:return 0;}
 }
 bool advanceBefore(unsigned available){if(!nextClocks()||available<nextClocks())return false;
  switch(stage_){case Stage::Reset:stage_=Stage::AwaitReset;break;case Stage::PortA:stage_=Stage::ImageA;break;
  case Stage::ImageA:stage_=Stage::SetupA;break;case Stage::SetupA:stage_=Stage::AwaitA;break;
  case Stage::PortB:stage_=Stage::ImageB;break;case Stage::ImageB:stage_=Stage::SetupB;break;
  case Stage::SetupB:stage_=Stage::AwaitB;break;case Stage::SelectA:stage_=Stage::AwaitSelect;break;default:return false;}return true;
 }
private:Stage stage_=Stage::Reset;
};
}
