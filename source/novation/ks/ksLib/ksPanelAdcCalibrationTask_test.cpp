#include "ksPanelAdcCalibrationTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::PanelAdcCalibrationTask;
void check(bool b){if(!b)throw std::runtime_error("ADC calibration property differs");}
int main(){try{unsigned long long cases=0,intervals=0;
 for(unsigned raw=0;raw<65536;++raw)for(unsigned index:{0u,1u,35u,127u,128u,129u,255u}){
  T task(T::Kind::Read,uint8_t(index));unsigned reads=0,writes=0;
  auto read=[&](T::Read field){check(field==T::Read::History&&task.offset()==((index*2)&255));++reads;return raw;};
  auto write=[&](T::Write,unsigned){++writes;};
  while(!task.complete()){
   const auto state=task.stage();const auto cost=task.nextClocks();const auto count=reads+writes;
   check(!task.advanceBefore(cost-1,read,write)&&task.stage()==state&&reads+writes==count);check(task.advanceBefore(cost,read,write)==cost);++intervals;
  }
  check(reads==1&&!writes&&task.result()==(raw<256?0:(raw-256)>>8));++cases;
 }
 for(unsigned seed=0;seed<256;++seed){
  T task(T::Kind::Snapshot,0);std::array<unsigned,36> history{},armed{};unsigned reads=0,writes=0;bool expectFlag=false;
  auto read=[&](T::Read field){check(field==T::Read::Sample&&!expectFlag&&task.offset()==reads*2);return (seed*971+reads++*149)&65535;};
  auto write=[&](T::Write field,unsigned value){const auto at=task.offset()/2;check(at<36);if(field==T::Write::History){check(!expectFlag);history[at]=value;expectFlag=true;}else{check(expectFlag);armed[at]=value;expectFlag=false;}++writes;};
  while(!task.complete()){const auto cost=task.nextClocks();const auto count=reads+writes;check(!task.advanceBefore(cost-1,read,write)&&reads+writes==count);check(task.advanceBefore(cost,read,write)==cost);++intervals;}
  check(reads==36&&writes==72&&!expectFlag);for(unsigned at=0;at<36;++at)check(history[at]==((seed*971+at*149)&65535)&&armed[at]==1);++cases;
 }
 std::cout<<"PASS: "<<cases<<" ADC calibration policies / "<<intervals<<" intervals; all retained words, wrapped index edges, 36 ordered live samples/armed writes and deadlines\n";
 return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
