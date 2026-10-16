#include "ksParameterPitchTask.h"
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
using T=ks::ParameterPitchTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int s8(unsigned v){return v<128?int(v):int(v)-256;}int s16(unsigned v){return v<32768?int(v):int(v)-65536;}
int main(){try{uint64_t cases=0;std::mt19937 rng(0x4b535049);
 auto verify=[&](unsigned shape,unsigned a,unsigned b,unsigned c){
  std::array<uint8_t,32> patch{};for(auto& v:patch)v=uint8_t(rng());unsigned base=uint16_t(rng()),globalFine=uint8_t(rng()),wheelFine=uint8_t(rng()),wheel=uint16_t(rng()),counter=uint8_t(rng()),spread=uint8_t(rng());
  if(shape==0){patch[6]=uint8_t(a);patch[7]=patch[15]=patch[23]=uint8_t(b);}
  if(shape==1){base=a;wheel=b;patch[9]=patch[17]=patch[25]=uint8_t(c);}
  if(shape==2){globalFine=a;patch[8]=patch[16]=patch[24]=uint8_t(b);wheelFine=c;}
  if(shape==3){patch[0]=uint8_t(a);counter=b;spread=c;}
  for(unsigned channel=0;channel<3;++channel){T task(channel);unsigned reads=0,packets=0,lookups=0,clocks=0;const bool unison=(patch[0]&7)!=0;
  auto reader=[&](T::Read r)->uint32_t{++reads;switch(r){case T::Read::StaticPitch:return base;case T::Read::Octaves:return patch[6];case T::Read::Coarse:return patch[task.coarseOffset()];case T::Read::GlobalFine:return globalFine;case T::Read::PatchFine:return patch[task.fineOffset()];case T::Read::WheelFine:return wheelFine;case T::Read::Flags:return patch[0];case T::Read::UnisonCounter:check(task.slot()==uint8_t(a+b+c),"Voice handle differs");return counter;case T::Read::SelectedPatch:return 0xfedcba98;case T::Read::Spread:return spread;case T::Read::PitchDepth:return patch[task.depthOffset()];case T::Read::Wheel:return wheel;}return 0;};
  while(!task.complete()){
   if(task.awaitingVoice()){check(task.resumeVoice(uint8_t(a+b+c)),"Voice result rejected");++lookups;continue;}
   if(task.awaitingWord()){
    const unsigned o=task.output();unsigned pitch=base;
    auto addHigh=[&](unsigned v){pitch=((pitch+((v&255)<<8))&0xff00)|(pitch&255);};
    addHigh(unsigned((int((patch[6]>>(2*o))&3)-1)*12));addHigh(uint8_t(patch[7+8*o]-64));
    unsigned fine=uint16_t(((globalFine+patch[8+8*o])&255)+wheelFine-192);
    if(unison){const unsigned u=(patch[0]&7)*4,divisor=(u+12)*2,numerator=(((counter&7)*8+1-u)&255)<<8|divisor;
     const unsigned quotient=uint8_t(s16(numerator)/int(divisor));const uint16_t product=uint16_t(s8(spread)*s8(quotient));fine=uint16_t(fine+s8(product>>8));}
    const unsigned scaled=uint16_t(fine*8);const uint32_t product=uint32_t(s16(scaled)*20971);pitch=uint16_t(pitch+(product>>16));
    pitch=uint16_t(pitch+uint32_t(s8(uint8_t(patch[9+8*o]-64))*s16(uint16_t(wheel-256))));
    const uint32_t want=uint32_t(pitch)*0xaaab>>8;check(task.dspWord()==want,"Pitch conversion differs");check(task.offset()==0x32+11*o,"Wire offset differs");check(task.resumeWord(),"Word completion rejected");++packets;continue;
   }
   const unsigned cost=task.nextClocks(),before=reads;check(cost>0,"Task stalled");check(!task.advanceBefore(cost-1,reader)&&reads==before,"Early observation");check(task.advanceBefore(cost,reader)==cost,"Clock admission differs");clocks+=cost;
  }
  check(clocks==338+9*channel+(unison?143:0),"Schedule differs");check(reads==9+(unison?3:0)&&packets==1&&lookups==(unison?1:0),"Observation/child count differs");check(!task.resumeWord()&&!task.resumeVoice(0),"Stray completion accepted");++cases;}
 };
 for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){verify(0,a,b,0);for(unsigned c:{0u,127u,128u,255u})verify(2,a,b,c);}
 for(unsigned a=0;a<65536;++a)for(unsigned c:{0u,1u,63u,64u,65u,127u,128u,192u,255u})verify(1,a,(a*49157+31337)&65535,c);
 for(unsigned a=0;a<8;++a)for(unsigned b=0;b<256;++b)for(unsigned c=0;c<256;++c)verify(3,a,b,c);
 std::cout<<"PASS: "<<cases<<" complete inline channel-pitch tasks; exhaustive octave/coarse pairs, fine pairs/carry edges, static/wheel words and signed depths, all unison count/counter/spread combinations; exact modular arithmetic/packed words/lookup handles/late reads and schedules\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
