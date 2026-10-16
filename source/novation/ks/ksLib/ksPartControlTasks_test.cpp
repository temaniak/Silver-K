#include "ksPartControlTasks.h"
#include <array>
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
 using T=ks::PartLookupTask;using L=T::Layout;unsigned cases=0;
 for(unsigned index=0;index<256;++index){
  for(L layout:{L::Parameters,L::Controls,L::Voice}){
   T task(layout);unsigned reads=0,clocks=0;auto reader=[&](L which){check(which==layout,"Lookup read wrong selector");++reads;return index;};
   while(task.nextClocks()){const unsigned cost=task.nextClocks(),previous=reads;check(!task.advanceBefore(cost-1,reader)&&reads==previous,"Lookup crossed deadline");check(task.advanceBefore(cost,reader)==cost,"Lookup boundary mismatch");clocks+=cost;}
   check(reads==1&&task.index()==index&&task.offset()==index*(layout==L::Parameters?334u:20u)&&clocks==(layout==L::Parameters?53u:42u),"Lookup result/timing differs");++cases;
  }
  auto direct=T::controlsFor(uint8_t(index));check(direct.advanceBefore(30,[](L)->unsigned{throw std::runtime_error("Provided index was reread");})==30&&direct.offset()==20*index&&!direct.nextClocks(),"Direct control lookup differs");++cases;
  for(bool interruptedWriter:{false,true}){
  ks::PartMirrorTask mirror{uint8_t(index)};unsigned reads=0,writes=0,clocks=0,steps=0;uint8_t latched=0;
  auto read=[&](unsigned part,unsigned at){check(part==index&&at==reads,"Mirror reordered/prefetched source");++reads;latched=uint8_t(index+at*37+steps*13);return latched;};
  auto write=[&](unsigned at,uint8_t byte){check(at==writes&&reads==writes+1&&byte==latched,"Mirror did not preserve latched byte/order");++writes;};
  while(mirror.nextClocks()){auto timing=[&](unsigned at){return interruptedWriter?3u:(at&1?0u:3u);};const auto cost=mirror.nextClocks(timing(mirror.position())),previous=reads,previousWrites=writes;check(!mirror.advanceBefore(cost-1,read,write,timing)&&reads==previous&&writes==previousWrites,"Mirror crossed deadline");check(mirror.advanceBefore(cost,read,write,timing)==cost,"Mirror boundary mismatch");clocks+=cost;++steps;}
  check(reads==20&&writes==20&&mirror.position()==20&&!mirror.remaining()&&clocks==(interruptedWriter?510u:480u)&&steps==105,"Mirror count/duration differs");++cases;
  }
 }
 std::cout<<"PASS: "<<cases<<" native lookup/mirror cases, all selector bytes/layouts and direct index, live byte latching/order/deadlines, exact 53/42/30/480 clocks and interrupted-writer510; no CPU state\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
