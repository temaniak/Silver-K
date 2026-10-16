#pragma once
#include "ksDspAddressTask.h"
#include "ksDspWordTask.h"
#include "ksDspFaultTask.h"
#include <variant>
#include <type_traits>
#include <cstdint>
namespace ks {
// Direct native DSP jobs for the typed coordinator. Requests and results are
// protocol values; the caller supplies time and interrupt scheduling.
// This interface has no architectural controller or emulated caller state.
class NativeDspJob final {
public:
 enum class Port {A,B};
 struct Address {uint16_t value;bool receive=false;DspAddressTask::PortSelection select=DspAddressTask::PortSelection::Keep;};
 struct Word {uint32_t value;bool advanceAddress=false;uint16_t address=0;};
 struct Progress {unsigned clocks=0;bool completed=false,faultRequested=false;};
 explicit NativeDspJob(Address request,Port port):work_(std::in_place_type<DspAddressTask>,request.value,request.receive,request.select),port_(port){}
 explicit NativeDspJob(Word request,Port port):work_(std::in_place_type<DspWordTask>,request.value,request.advanceAddress,request.address),port_(port){}
 bool complete()const{return complete_;}Port port()const{return port_;}
 uint32_t result()const{const auto*p=std::get_if<DspAddressTask>(&work_);return p?p->result():0;}
 bool awaitingFault()const{return std::visit([](const auto&t){using T=std::decay_t<decltype(t)>;return t.stage()==T::Stage::AwaitFault;},work_);}
 bool resumeFault(){return std::visit([](auto&t){return t.resumeFault();},work_);}
 unsigned nextClocks()const{if(complete_)return 0;return std::visit([](const auto&t){return t.complete()?17u:t.nextBoundaryClocks();},work_);}
 // One complete interval at a time lets the coordinator latch external events
 // at the original boundary. Callbacks receive the offset within that interval.
 template<class Reader,class Writer,class Publish>
 Progress advanceBefore(unsigned available,Reader&&read,Writer&&write,Publish&&publish){
  const auto cost=nextClocks();if(!cost||available<cost)return {};
  Progress out{cost};
  std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;
   if(t.complete()){complete_=true;out.completed=true;return;}
   if constexpr(std::is_same_v<T,DspAddressTask>){
    if(t.stage()==T::Stage::SelectPort)port_=t.portSelection()==T::PortSelection::A?Port::A:Port::B;
    t.advanceBefore(cost,[&](unsigned offset){return read(port_,offset,cost);},[&](unsigned offset,uint8_t value){write(port_,offset,value,cost);});
   }else t.advanceBefore(cost,[&]{return read(port_,4,cost);},[&](unsigned offset,uint8_t value){write(port_,offset,value,cost);},[&](uint16_t address){publish(address,cost);});
   out.faultRequested=t.stage()==T::Stage::AwaitFault;
  },work_);return out;
 }
private:std::variant<DspAddressTask,DspWordTask> work_;Port port_;bool complete_=false;
};
// Typed timeout child: fresh latch/control observations, native pulse/delay and
// completion. Read and write timestamps inside a strobe remain distinct.
class NativeDspFaultJob final {public:
 bool complete()const{return complete_;}
 unsigned nextClocks()const{return complete_?0:task_.complete()?17:task_.nextBoundaryClocks();}
 template<class Reader,class Writer>NativeDspJob::Progress advanceBefore(unsigned available,Reader&&read,Writer&&write){
  const auto cost=nextClocks();if(!cost||cost>available)return {};
  if(task_.complete()){complete_=true;return {cost,true,false};}
  const auto stage=task_.stage();task_.advanceBefore(cost,[&](DspFaultTask::Read what){return read(what,(stage==DspFaultTask::Stage::BeginPulse||stage==DspFaultTask::Stage::EndPulse)?cost-1:cost);},[&](DspFaultTask::Write what,uint8_t value){write(what,value,cost);});return {cost,false,false};
 }
private:DspFaultTask task_;bool complete_=false;
};}
