#include "ksClockPulseTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::ClockPulseTask;using S=T::Stage;using Values=std::array<uint32_t,13>;
uint64_t cases=0,intervals=0;
void check(bool value){if(!value)throw std::runtime_error("Clock pulse property differs");}
void run(T& task,Values& values,uint16_t tick){unsigned steps=0,reads=0,writes=0;
 auto read=[&](T::Read field){++reads;return values[unsigned(field)];};
 auto write=[&](T::Write field,uint32_t value){++writes;switch(field){case T::Write::Latched:values[0]=value;break;case T::Write::Flags:values[1]=value;break;case T::Write::PreviousTick:values[8]=value;break;case T::Write::Samples:values[7]=value;break;case T::Write::Sum:values[6]=value;break;case T::Write::PulseCount:values[4]=value;break;case T::Write::SumHigh:values[6]=(values[6]&65535)|(value<<16);break;case T::Write::SumLow:values[6]=(values[6]&0xffff0000u)|value;break;case T::Write::Estimate:values[9]=value;break;case T::Write::Period:values[12]=value;break;case T::Write::Integral:values[11]=value;break;case T::Write::Gain:values[10]=value;break;}};
 while(!task.complete()){
  const auto before=task.stage();const auto cost=task.nextClocks();const auto count=reads+writes;check(cost&&++steps<200);check(!task.advanceBefore(cost-1,read,write)&&task.stage()==before&&reads+writes==count);check(task.advanceBefore(cost,read,write)==cost);++intervals;
  if(task.awaitingChild()){if(before==S::ConsumeLatched){T edge(T::Kind::Edge,0,task.source());run(edge,values,tick);}check(task.resumeChild(tick));}
 }
}
Values expected(Values values,unsigned index,unsigned source,uint16_t tick){
 values[1]|=1u<<(index&7);
 if(!values[2]||source==255){values[8]=tick;values[7]=values[6]=0;values[4]=values[3];return values;}
 values[4]=uint16_t(values[4]+1);uint32_t difference=values[5]-(values[4]<<16);difference=(difference&0x800000)?difference|0xff000000u:difference&0xffffffu;values[6]+=difference;values[7]=uint16_t(values[7]+512);
 const auto elapsed=uint16_t(tick-values[8]);if(elapsed<16000)return values;
 const int64_t n=int32_t(values[6]),d=int16_t(values[7]);const uint16_t average=!d&&n?uint16_t(n):uint16_t(d?n/d:1);
 const uint16_t measured=uint16_t(uint32_t(values[7])*50000u/elapsed);const auto error=int16_t(uint16_t(measured-uint16_t(values[9]>>16)));
 values[8]=tick;values[7]=values[6]=0;values[9]+=uint32_t(int64_t(error)*int16_t(values[10])*4);values[12]=(values[9]>>2)-uint32_t(int32_t(int16_t(average))*16384);
 const int16_t integral=values[10]==0x4000?0:int16_t(uint16_t(values[11]+uint16_t(error)));const uint32_t product=uint32_t(int64_t(integral)*0x7e00*2);values[11]=uint16_t(product>>16);uint16_t magnitude=uint16_t(values[11]);if(product&0x80000000u)magnitude=uint16_t(0u-magnitude);
 if(magnitude>0x1800)values[10]=0x4000;else{const auto gain=uint16_t(values[10]-0x800);values[10]=(gain&0x8000)||gain<0x80?0x80:gain;}
 return values;
}
int main(){try{
 for(uint32_t seed:{0u,1u,0x7fffu,0x8000u,0xffffu,0x7fffffu,0x800000u,0xffffffu,0x80000000u,0xffffffffu})for(unsigned gain:{0u,1u,0x80u,0x800u,0x4000u,0x7fffu,0xffffu})for(unsigned active:{0u,1u,255u})for(unsigned source:{0u,127u,128u,255u})for(unsigned index=0;index<16;++index)for(auto kind:{T::Kind::Edge,T::Kind::Latched}){
  const uint16_t tick=uint16_t(seed+16000);const uint32_t timer=seed^0xdeadbeefu;Values values{{source,0x55,active,timer>>16,uint16_t(seed),timer,seed,uint16_t(seed),uint16_t(seed>>1),seed,gain,uint16_t(seed),0xdeadbeef}};
  auto target=expected(values,kind==T::Kind::Latched?0:index,source,tick);if(kind==T::Kind::Latched)target[0]=0;T task(kind,uint8_t(index),uint8_t(source));run(task,values,tick);check(values==target);++cases;
 }
 for(unsigned seed=0;seed<65536;++seed){const uint32_t timer=seed*0x10001u;Values values{{0,1,1,timer>>16,seed,timer,seed*971u,seed,0,seed*149u,seed,uint16_t(seed*13),0}};auto target=expected(values,0,0,65535);T task(T::Kind::Edge,0,0);run(task,values,65535);check(values==target);++cases;}
 std::cout<<"PASS: "<<cases<<" clock pulse policies / "<<intervals<<" intervals; gain/sample words, source/index/tick wraps, signed24 phase, original average and integral clamps, ordered writes and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
