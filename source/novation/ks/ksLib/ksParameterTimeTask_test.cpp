#include "ksParameterTimeTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::ParameterTimeTask;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 for(unsigned variant=0;variant<3;++variant)for(unsigned base=0;base<65536;++base)for(unsigned value=0;value<256;++value){
  const unsigned multiplier=variant==2?80:107,threshold=variant==0?117:variant==1?127:115;const bool optional=variant==2;T task(uint8_t(multiplier),uint8_t(threshold),optional);unsigned reads=0,baseReads=0,clocks=0,children=0;
  const uint32_t returned=(base*0x9e3779b1u)^(value*0x1234567u)^0xfedcba98;
  auto reader=[&](bool isBase)->uint32_t{++reads;if(isBase){++baseReads;return base;}return value;};
  const unsigned inverted=value^127;const uint16_t wantInput=uint16_t(inverted*multiplier+(inverted>threshold?(inverted-threshold)*256:0)+base);
  while(!task.awaitingWord()){
   if(task.awaitingCoefficient()){check(task.scaleInput()==wantInput,"Coefficient input differs");check(task.resumeCoefficient(returned),"Coefficient rejected");++children;continue;}
   const auto cost=task.nextClocks(),before=reads;check(cost>0,"Stalled");check(!task.advanceBefore(cost-1,reader)&&reads==before,"Early observation");check(task.advanceBefore(cost,reader)==cost,"Admission differs");clocks+=cost;
  }
  const uint32_t want=optional?(value?returned>>1:0):returned;check(task.dspWord()==want>>8,"Packet differs");
  const unsigned expected=optional&&!value?41:106+(inverted>threshold?6:0)+(optional?9:0);
  check(clocks==expected&&reads==unsigned(optional&&!value?1:2)&&baseReads==unsigned(!(optional&&!value))&&children==baseReads,"Schedule/observations differ");
  check(task.resumeWord()&&task.complete()&&!task.resumeWord()&&!task.resumeCoefficient(0),"Lifetime differs");++cases;
 }
 std::cout<<"PASS: "<<cases<<" complete coefficient-time jobs; all256 values x65536 static bases across3 variants, byte correction/wrap, optionalzero/half, opaque native child results, packet/deadline/exact clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
