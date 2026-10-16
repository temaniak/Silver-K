#include "ksParameterScalarTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::ParameterScalarTask;using K=T::Kind;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}int s16(unsigned v){return v<32768?int(v):int(v)-65536;}
int main(){try{uint64_t cases=0;
 for(unsigned kind=0;kind<8;++kind)for(unsigned value=0;value<256;++value){T task{K(kind)};unsigned reads=0,clocks=0;auto read=[&]{++reads;return value;};
  while(!task.awaitingWord()){const auto cost=task.nextClocks(),before=reads;check(cost>0,"Stalled");check(!task.advanceBefore(cost-1,read)&&reads==before,"Early read");check(task.advanceBefore(cost,read)==cost,"Bad admission");clocks+=cost;}
  const unsigned biased=(value-64)&255;uint32_t raw=0;unsigned expectedClocks=0;
  switch(K(kind)){
   case K::Wave:raw=((value<<8)*9+0x40000);expectedClocks=76;break;
   case K::Bipolar:raw=uint32_t(s16(biased<<8)*21845)<<1;expectedClocks=76;break;
   case K::Endpoint:{const unsigned low=biased==63?255:0,word=biased<<8|low;raw=(word<<16)|(low<<8);expectedClocks=53+(low?3:0);break;}
   case K::DoubleEndpoint:{const unsigned low=biased==63?255:0,word=((biased<<8|low)*2)&65535;raw=(word<<16)|(((word>>8)-(word&255))&255)<<8;expectedClocks=56+(low?3:0);break;}
   case K::Square:{const uint32_t word=value<<8|(value*2&255);raw=word*word<<1;expectedClocks=67;break;}
   case K::Quarter:raw=((value<<8)>>2)<<16;expectedClocks=50;break;
   case K::BipolarHalf:{const unsigned word=(biased<<8)>>1|(biased&128?32768:0);raw=word<<16;expectedClocks=50;break;}
   case K::ByteHigh:raw=value<<24;expectedClocks=44;break;
  }
  check(task.dspWord()==raw>>8,"Scalar packet differs");check(clocks==expectedClocks&&reads==1,"Scalar schedule differs");check(task.resumeWord()&&task.complete(),"Word completion differs");check(!task.resumeWord(),"Extra completion accepted");++cases;
 }
 for(unsigned word=0;word<65536;++word)for(unsigned high:{0u,127u,255u}){T task(K::ByteHigh);const unsigned packet=(high<<16)|word;task.pendingWord(packet);unsigned reads=0;auto read=[&]{++reads;return 0;};check(!task.advanceBefore(19,read),"Early pending dispatch");check(task.advanceBefore(20,read)==20&&!reads,"Pending dispatch differs");check(task.dspWord()==packet&&task.resumeWord()&&task.complete(),"Pending packet differs");++cases;}
 std::cout<<"PASS: "<<cases<<" scalar and pending-packet jobs; all8 packing families/all256 values, endpoint63 behavior, word/byte wrap and signed half/multiply, deadlines/clocks/no stale input\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
