#include "ksParameterModulationTask.h"
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
using T=ks::ParameterModulationTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int sign8(unsigned x){return x<128?int(x):int(x)-256;}
int sign16(unsigned x){return x<32768?int(x):int(x)-65536;}
int main(){try{uint64_t cases=0;std::mt19937 rng(0x4b534f4d);
 for(unsigned pass=0;pass<12;++pass)for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){
  std::array<uint8_t,180> patch{};std::array<uint8_t,3> mods{};std::array<uint16_t,65> curve{};
  for(auto& v:patch)v=uint8_t(rng());for(auto& v:mods)v=uint8_t(rng());for(auto& v:curve)v=uint16_t(rng());
  for(unsigned o=0;o<4;++o){const unsigned p=o==3?51:10+8*o,d=o==3?53:34;
   if(pass<3){patch[d+pass]=uint8_t(a);mods[pass]=uint8_t(b);}else if(pass<6){patch[p]=uint8_t(a);patch[d+pass-3]=uint8_t(b);}
   else if(pass<9){curve[(a+o)%65]=uint16_t((a<<8)|b);}else{patch[p]=uint8_t(a);mods[pass-9]=uint8_t(b);}
  }
  for(unsigned channel=0;channel<4;++channel){T task(channel);unsigned reads=0,packets=0,clocks=0,expectedClocks=0;
  auto reader=[&](T::Read r)->uint32_t{++reads;switch(r){case T::Read::Bias:return patch[task.biasOffset()];case T::Read::Depth:return patch[task.depthOffset()];case T::Read::Modulation:return mods[task.modulationOffset()];case T::Read::Curve:check(task.curveIndex()%2==0&&task.curveIndex()<=128,"Invalid curve index");return curve[task.curveIndex()/2];}return 0;};
  while(!task.complete()){
   if(task.awaitingWord()){
    const unsigned o=task.output(),p=o==3?51:10+8*o,d=o==3?53:34;
    unsigned sum=0;for(unsigned t=0;t<3;++t)sum=(sum+unsigned(sign8(uint8_t(patch[d+t]-64))*sign8(mods[t])))&65535;
    const int scaled=sign16(sum)>=0?sign16(sum)/128:-((-sign16(sum)+127)/128);
    const unsigned combined=(unsigned(sign8(uint8_t(patch[p]-64)))+unsigned(scaled))&65535;
    const bool negative=combined>=32768;unsigned magnitude=negative?(65536-combined)&65535:combined;const bool clamp=magnitude>64;magnitude=std::min(magnitude,64u);
    const uint32_t want=(uint32_t(sign16(curve[magnitude])*(negative?-21845:21845))<<1)>>8;
    check(task.dspWord()==want,"Modulation arithmetic differs");expectedClocks+=303+(negative?6:0)+(clamp?6:0);
    constexpr unsigned offsets[]{0x34,0x3f,0x4a,0x61};check(task.offset()==offsets[o],"Wire offset differs");check(task.resumeWord(),"Completion rejected");++packets;continue;
   }
   const unsigned cost=task.nextClocks(),before=reads;check(cost>0,"Task stalled");check(!task.advanceBefore(cost-1,reader)&&reads==before,"Early observation");check(task.advanceBefore(cost,reader)==cost,"Admission clocks differ");clocks+=cost;
  }
  if(clocks!=expectedClocks){std::cerr<<"clocks="<<clocks<<" expected="<<expectedClocks<<'\n';throw std::runtime_error("Schedule differs");}
  check(packets==1&&reads==8,"Observation count differs");check(!task.resumeWord(),"Stray child completion accepted");++cases;}
 }
 std::cout<<"PASS: "<<cases<<" complete inline modulation tasks; exhaustive depth/modulation pairs and bias/depth pairs, arbitrary signed curves, modular accumulation/scaling/sign/clamp/24-bit packing, offsets, observation deadlines and exact schedules\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
