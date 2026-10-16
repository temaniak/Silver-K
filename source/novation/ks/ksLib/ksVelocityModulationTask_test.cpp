#include "ksVelocityModulationTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::VelocityModulationTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int sign(unsigned v){return v<128?int(v):int(v)-256;}
int main(){try{uint64_t cases=0;
 for(unsigned offset:{0u,0x6bu,0x6fu,0x77u})for(unsigned velocity=0;velocity<256;++velocity)for(unsigned depth=0;depth<256;++depth){
  const bool relative=offset!=0;T task(relative,uint16_t(offset));unsigned clocks=0,reads=0,lookups=0,packets=0;
  unsigned v=velocity,d=(depth+192)&255;const bool negative=d>=128;if(negative){v^=127;d=(256-d)&255;}v=(v+129)&255;
  const uint16_t scaled=uint16_t(sign(v)*sign(d)*4);const uint32_t wanted=scaled?uint32_t((((unsigned(scaled)>>8)+127)&255)<<8|(scaled&255))<<8:0x7fffff;
  auto reader=[&](T::Read what)->uint32_t{++reads;check(task.slot()==uint8_t(velocity+depth),"Voice binding differs");return what==T::Read::Velocity?velocity:depth;};
  while(!task.complete()){
   const auto child=task.child();if(child!=T::Child::None){if(child==T::Child::Voice){++lookups;check(task.resumeChild(uint8_t(velocity+depth)),"Voice result rejected");}
    else{++packets;check(task.dspWord()==wanted,"Packed modulation word differs");check(task.relative()==relative&&task.offset()==offset,"Transport request differs");check(task.resumeChild(),"DSP completion rejected");}continue;}
   const auto cost=task.nextClocks();const auto before=reads;check(cost>0,"Modulation stalled");check(!task.advanceBefore(cost-1,reader)&&reads==before,"Early input observation");check(task.advanceBefore(cost,reader)==cost,"Modulation clocks differ");clocks+=cost;
  }
  check(lookups==1&&packets==1&&reads==2,"Native request count differs");check(clocks==83+(negative?6:0)+(scaled?9:12)+(relative?6:0)+20,"Original modulation schedule differs");check(!task.resumeChild(),"Stray completion accepted");++cases;
 }
 std::cout<<"PASS: "<<cases<<" complete velocity/depth/transport cases; signed byte inversion/negation, multiply/16-bit wrap, zero shortcut, high-byte bias, exact packed word and deadlines/clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
