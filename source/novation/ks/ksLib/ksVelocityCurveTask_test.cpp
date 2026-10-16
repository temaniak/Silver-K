#include "ksVelocityCurveTask.h"
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
 uint64_t cases=0;
 for(unsigned velocity=0;velocity<256;++velocity)for(unsigned curve=0;curve<256;++curve)for(unsigned power:{0u,1u,2u,3u,5u,255u}){
  const uint8_t threshold=uint8_t(velocity*37+curve*11+power*17);const unsigned rounds=power?power:256;unsigned expected=velocity;
  for(unsigned round=1;round<rounds;++round)expected=((expected*velocity)&0x7fff)>>7;
  const unsigned inverse=expected^0x7f;bool suppress=false;
  if(curve==1)expected=inverse;else if(curve==2)expected=((expected*expected)&0x7fff)>>7;
  else if(curve==3)expected=((expected*inverse)&0x7fff)>>7;else if(curve>=4){suppress=threshold&1?velocity<threshold:velocity>threshold;if(suppress)expected=0x80;}
  ks::VelocityCurveTask task{uint8_t(velocity),uint8_t(curve),uint8_t(power)};unsigned reads=0,elapsed=0;
  auto read=[&](unsigned at){check(curve>=4&&at==curve-4&&!reads,"Velocity threshold read early/repeated/wrong index");++reads;return threshold;};
  while(!task.complete()){
   const unsigned cost=task.nextClocks(),previous=reads;const auto shortStep=task.advanceBefore(cost-1,read);check(!shortStep.clocks&&!shortStep.boundaries&&reads==previous,"Velocity task crossed deadline");
   const auto step=task.advanceBefore(cost,read);check(step.clocks==cost&&step.boundaries==1,"Velocity task boundary differs");elapsed+=step.clocks;
  }
  const unsigned extra=curve==0?0:curve==1?9:curve==2?33:curve==3?36:54+(suppress?3:0);
  check(task.velocity()==expected&&reads==(curve>=4?1u:0u)&&elapsed==21+30*(rounds-1)+extra,"Velocity integer result/timing differs");++cases;
 }
 auto empty=ks::VelocityCurveTask::empty();check(empty.complete()&&!empty.nextClocks(),"Empty velocity completion differs");
 std::cout<<"PASS: "<<cases<<" native velocity/curve/power cases, all velocity and curve bytes, zero/full power wrap, independent quantized-product formula, live threshold/deadline timing; no CPU state\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
