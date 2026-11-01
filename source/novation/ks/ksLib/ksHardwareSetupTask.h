#pragma once
#include <array>
#include <cstdint>
namespace ks {
// Fixed board reset defaults: ordered typed publications with exact admission
// deadlines. Hardware addresses, CPU projections and return ABI are external.
class HardwareSetupTask final {
public:
 enum class Kind{Pins,WaitStates,Dram,Clock};
 enum class Field{None,P3Direction,BusControl,PBControl,InputSelect,PCControl,PEControl,PHControl,P9Direction,PortFDirection,SystemControl,PortGDirection,WaitControl,Wait0,Wait1,WaitEnable,DramMode,DramRefresh,P4Direction,P4Data,TimerPeriod,TimerControl,TimerEnable,ClockCounter,InterruptControl,PriorityControl,ClockMode,InterruptMode};
 enum class Phase{Prepare,Publish,Finish,Complete};
 struct Publication{Field field;uint32_t value;unsigned width;};
 explicit HardwareSetupTask(Kind k):kind_(k){}
 Kind kind()const{return kind_;}unsigned position()const{return cursor_;}
 bool complete()const{return phase()==Phase::Complete;}
 Phase phase()const{return cursor_>=count()?Phase::Complete:definition().phase;}
 unsigned nextClocks(unsigned bus=0)const{return complete()?0:definition().clocks+(phase()==Phase::Publish?bus:0);}
 Publication publication()const{auto d=definition();return {d.field,d.value,d.width};}
 template<class Writer,class Timing>unsigned advanceBefore(unsigned available,Writer&& write,Timing&& timing){unsigned used=0;
  while(auto cost=nextClocks(phase()==Phase::Publish?timing(publication()):0)){if(cost>available-used)break;auto d=definition();if(d.phase==Phase::Publish)write(Publication{d.field,d.value,d.width});++cursor_;used+=cost;}return used;}
private:
 struct Definition{Phase phase;Field field;uint32_t value;unsigned width,clocks;};
 unsigned count()const{switch(kind_){case Kind::Pins:return 23;case Kind::WaitStates:return 9;case Kind::Dram:return 9;case Kind::Clock:return 20;}return 0;}
 Definition definition()const{switch(kind_){case Kind::Pins:{static constexpr std::array<Definition,23> values{{{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::P3Direction,37u,1,3},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::BusControl,24u,1,3},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::PBControl,255u,1,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::InputSelect,48u,1,3},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::PCControl,63u,1,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::PEControl,3u,1,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::PHControl,16u,1,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::P9Direction,0u,1,3},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::PortFDirection,143u,1,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::SystemControl,1u,1,3},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::PortGDirection,31u,1,6},{Phase::Finish,Field::None,0u,0,17}}};return values[cursor_];}case Kind::WaitStates:{static constexpr std::array<Definition,9> values{{{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::WaitControl,223u,1,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::Wait0,85u,1,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::Wait1,85u,1,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::WaitEnable,128u,1,6},{Phase::Finish,Field::None,0u,0,17}}};return values[cursor_];}case Kind::Dram:{static constexpr std::array<Definition,9> values{{{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::DramMode,118u,1,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::DramRefresh,102u,1,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::P4Direction,25u,1,3},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::P4Data,127u,1,3},{Phase::Finish,Field::None,0u,0,17}}};return values[cursor_];}case Kind::Clock:{static constexpr std::array<Definition,20> values{{{Phase::Prepare,Field::None,0u,0,9},{Phase::Prepare,Field::None,0u,0,6},{Phase::Publish,Field::TimerPeriod,12500u,2,3},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::TimerControl,32u,1,3},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::TimerEnable,1u,1,3},{Phase::Prepare,Field::None,0u,0,9},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::ClockCounter,0u,4,6},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::InterruptControl,6u,1,3},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::PriorityControl,4u,1,3},{Phase::Prepare,Field::None,0u,0,9},{Phase::Prepare,Field::None,0u,0,6},{Phase::Publish,Field::ClockMode,2u,2,3},{Phase::Prepare,Field::None,0u,0,3},{Phase::Publish,Field::InterruptMode,7u,1,3},{Phase::Finish,Field::None,0u,0,17}}};return values[cursor_];}}return {Phase::Complete,Field::None,0,0,0};}
 Kind kind_;unsigned cursor_=0;
};
}
