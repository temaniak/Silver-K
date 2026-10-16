#include "ksHardwareSetupTask.h"
#include <array>
#include <vector>
#include <stdexcept>
#include <iostream>
namespace{void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}}
int main(){try{using T=ks::HardwareSetupTask;using F=T::Field;
struct Expected{F field;uint32_t value;unsigned width;};
std::array<std::vector<Expected>,4> expected{{{{F::P3Direction,37u,1},{F::BusControl,24u,1},{F::PBControl,255u,1},{F::InputSelect,48u,1},{F::PCControl,63u,1},{F::PEControl,3u,1},{F::PHControl,16u,1},{F::P9Direction,0u,1},{F::PortFDirection,143u,1},{F::SystemControl,1u,1},{F::PortGDirection,31u,1}},{{F::WaitControl,223u,1},{F::Wait0,85u,1},{F::Wait1,85u,1},{F::WaitEnable,128u,1}},{{F::DramMode,118u,1},{F::DramRefresh,102u,1},{F::P4Direction,25u,1},{F::P4Data,127u,1}},{{F::TimerPeriod,12500u,2},{F::TimerControl,32u,1},{F::TimerEnable,1u,1},{F::ClockCounter,0u,4},{F::InterruptControl,6u,1},{F::PriorityControl,4u,1},{F::ClockMode,2u,2},{F::InterruptMode,7u,1}}}};
const unsigned goldenClocks[]={101,53,47,101};unsigned cases=0;
for(unsigned k=0;k<4;++k)for(unsigned delay=0;delay<4;++delay)for(unsigned budget=0;budget<64;++budget){T task{T::Kind(k)};unsigned used=0,index=0;auto timing=[&](T::Publication p){return p.width*delay;};auto publish=[&](T::Publication p){check(index<expected[k].size(),"Extra hardware publication");auto e=expected[k][index++];check(p.field==e.field&&p.value==e.value&&p.width==e.width,"Hardware field/value/width differs");};
 while(!task.complete()){auto phase=task.phase();auto position=task.position();auto cost=task.nextClocks(task.phase()==T::Phase::Publish?timing(task.publication()):0);check(cost>0,"Hardware progress has no clock cost");check(!task.advanceBefore(cost-1,publish,timing)&&task.phase()==phase&&task.position()==position,"Under-admitted hardware step mutated");auto spent=task.advanceBefore(budget,publish,timing);if(!spent)spent=task.advanceBefore(cost,publish,timing);used+=spent;}
 check(index==expected[k].size(),"Hardware publication count differs");check(used==goldenClocks[k]+(k==0?11u:k==3?13u:4u)*delay,"Hardware total clocks differ");check(!task.advanceBefore(1000,publish,timing),"Completed hardware task advanced");++cases;}
std::cout<<"PASS: "<<cases<<" fixed board-configuration schedules, every deadline/field/value/width and zero reads\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
