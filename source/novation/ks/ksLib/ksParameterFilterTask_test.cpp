#include "ksParameterFilterTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::ParameterFilterTask;
void check(bool b,const char* w){if(!b)throw std::runtime_error(w);}int sign(unsigned v){return v<128?int(v):int(v)-256;}
int main(){try{uint64_t cases=0;auto verify=[&](unsigned velocity,unsigned depth,unsigned cutoff,unsigned base){T task;unsigned reads=0,clocks=0;auto read=[&](T::Read r)->unsigned{++reads;check(task.slot()==uint8_t(velocity+depth),"Slot differs");return r==T::Read::Velocity?velocity:r==T::Read::Depth?depth:r==T::Read::Cutoff?cutoff:base;};
 while(!task.awaitingWord()){if(task.awaitingVoice()){check(task.resumeVoice(uint8_t(velocity+depth)),"Voice rejected");continue;}const auto cost=task.nextClocks(),before=reads;check(cost>0,"Stalled");check(!task.advanceBefore(cost-1,read)&&reads==before,"Early read");check(task.advanceBefore(cost,read)==cost,"Admission differs");clocks+=cost;}
 const unsigned sum=uint16_t(cutoff*128+sign((velocity-64)&255)*sign((depth-64)&255)*2);const bool negative=sum&32768,high=!negative&&sum>0x3f80;const unsigned bounded=negative?0:high?0x3f80:sum;const uint32_t want=(uint32_t(bounded*2)*0xaaab-0x48001400+(uint32_t(base)<<16))>>8;
 check(task.dspWord()==want,"Filter coefficient differs");check(clocks==189+(negative?3:0)+(high?6:0)&&reads==4,"Schedule differs");check(task.resumeWord()&&task.complete()&&!task.resumeVoice(0)&&!task.resumeWord(),"Lifetime differs");++cases;};
 for(unsigned velocity=0;velocity<256;++velocity)for(unsigned depth=0;depth<256;++depth)for(unsigned cutoff=0;cutoff<256;++cutoff)verify(velocity,depth,cutoff,(velocity*257+depth*43+cutoff*173)&65535);
 for(unsigned base=0;base<65536;++base)verify(base&255,(base>>8)&255,(base*13)&255,base);
 std::cout<<"PASS: "<<cases<<" complete filter jobs; all velocity/depth/cutoff byte triples and static16 values, signed wrapped modulation, sign/max clamps, unsigned product/origin/highword base, late observations and exact clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
