#include "ksClockSyncTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::ClockSyncTask;using S=T::Stage;
void check(bool value){if(!value)throw std::runtime_error("Clock sync property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned source=0;source<256;++source)for(uint32_t seed:{0u,1u,0x7fffu,0x8000u,0xffffu,0x7fffffffu,0x80000000u,0xffffffffu})for(unsigned gain:{0u,1u,0x80u,0x800u,0x4000u,0xffffu})for(unsigned flags:{0u,1u,2u,255u})for(unsigned divider:{0u,1u}){
  std::array<uint32_t,11> values{{divider,source,flags,uint8_t(seed),seed,255,uint16_t(seed),seed,gain,seed^0x1000,seed&0xfffff000u}};auto expected=values;
  unsigned sent=0,writes=0,reads=0;bool coefficientSent=false,periodSent=false;
  expected[0]^=1;
  if(!(expected[0]&1)){
   unsigned active;if(source){const auto mode=uint8_t(source-1);expected[2]=(flags&~2u)|(mode&2);active=mode&1;}else{expected[2]=flags&~2u;active=flags&1;}expected[5]=active;
   uint32_t rate;if(!active){expected[4]=uint32_t(uint8_t(uint8_t(seed)+64))*32u*0xd1b7u;rate=expected[4]<<2;}else{const auto error=int16_t(uint16_t(uint16_t(seed)-uint16_t(seed>>16)));expected[7]=seed+uint32_t(int64_t(error)*int16_t(gain)*4);rate=expected[7];}
   uint32_t resolution=0x4000000;for(unsigned n=0;n<5&&!(rate&0x80000000u);++n){rate<<=1;resolution<<=1;}
   const auto divisor=rate>>16;const uint32_t coefficient=divisor?uint32_t((uint64_t(resolution)<<16)/divisor):0;
   if(active&&gain!=0x80){if(gain==0x800)expected[8]=0x7ff;}
   else{uint32_t difference=expected[9]-coefficient;if(difference&0x80000000u)difference=0u-difference;if(difference>=0x1000){expected[9]=coefficient;coefficientSent=true;}}
   if((expected[4]&0xfffff000u)!=expected[10]){expected[10]=expected[4]&0xfffff000u;periodSent=true;}
  }
  T task(T::Kind::Update);auto read=[&](T::Read field){++reads;return values[unsigned(field)];};
  auto write=[&](T::Write field,uint32_t value){++writes;constexpr unsigned indices[]{0,2,5,4,7,8,9,10};values[indices[unsigned(field)]]=value;};unsigned steps=0;
  while(!task.complete()){
   const auto before=task.stage();const auto cost=task.nextClocks();const auto count=reads+writes;check(cost&&++steps<200);check(!task.advanceBefore(cost-1,read,write)&&task.stage()==before&&reads+writes==count);check(task.advanceBefore(cost,read,write)==cost);++intervals;
   if(task.awaitingChild()){if(before==S::SendCoefficient){check(coefficientSent&&task.coefficient()==expected[9]);++sent;}else{check(periodSent&&task.period()==(expected[10]<<2));++sent;}check(task.resumeChild());}
  }
  check(values==expected&&sent==unsigned(coefficientSent)+2*unsigned(periodSent));++cases;
 }
 std::cout<<"PASS: "<<cases<<" clock sync policies / "<<intervals<<" intervals; source/rate/gain/flags, packed division versus wide rational, wrapping feedback, publication order and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
