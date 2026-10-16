#include "ksGlobalModulationTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::GlobalModulationTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 for(unsigned part=0;part<256;++part)for(unsigned base=0;base<256;++base)for(unsigned shape=0;shape<4;++shape){
  T task;unsigned reads=0,packets=0,clocks=0;std::array<unsigned,6> levels{};for(unsigned i=0;i<6;++i)levels[i]=(part+base*7+i*31+shape*53)&255;
  auto reader=[&](T::Read r)->uint32_t{++reads;return r==T::Read::Part?part:r==T::Read::Base?base:levels[task.output()];};
  const unsigned delta=(part-base)&255,slot=delta&128?0:delta*3&255;
  while(!task.complete()){
   if(task.awaitingWord()){
    const unsigned i=task.output(),v=levels[i];check(task.dspWord()==((v<<8)|(v*2&255))<<8,"Packed level differs");check(task.offset()==0x19+i%3+slot&&task.targetA()==(i>=3),"Wire routing differs");
    check(task.resumeWord(),"Child completion rejected");++packets;continue;
   }
   const unsigned cost=task.nextClocks(),before=reads;check(cost>0,"Task stalled");check(!task.advanceBefore(cost-1,reader)&&reads==before,"Early observation");check(task.advanceBefore(cost,reader)==cost,"Admission differs");clocks+=cost;
  }
  check(clocks==377+((delta&128)?3:0),"Schedule differs");check(reads==8&&packets==6,"Observation count differs");check(!task.resumeWord(),"Stray completion accepted");++cases;
 }
 std::cout<<"PASS: "<<cases<<" complete six-output tasks; all part/base pairs, negative and modular slot arithmetic, live repeated levels, 24-bit packing/B-to-A order, deadlines and exact clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
