#include "ksPanelAdcScaleTask.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using T=ks::PanelAdcScaleTask;
void check(bool b){if(!b)throw std::runtime_error("ADC curve property differs");}
int main(){try{unsigned long long cases=0,intervals=0;
 for(unsigned raw=0;raw<65536;++raw)for(unsigned curve=0;curve<3;++curve)for(unsigned channel:{0u,31u,32u,35u,127u,128u,255u}){
  const auto kind=curve==2?T::Kind::Special:T::Kind::Ordinary;T task(kind,uint16_t(raw),uint8_t(channel));unsigned writes=0,reads=0,steps=0;
  auto read=[&](T::Read){++reads;return curve==1?255:0;};auto write=[&](T::Write,uint8_t value){++writes;check(value==channel);};
  while(!task.complete()){
   const auto stage=task.stage();const auto cost=task.nextClocks();const auto value=task.value(),beforeReads=reads,beforeWrites=writes;
   check(task.advanceBefore(cost-1,read,write)==0&&task.stage()==stage&&task.value()==value&&reads==beforeReads&&writes==beforeWrites);
   check(task.advanceBefore(cost,read,write)==cost);check(++steps<60);++intervals;
  }
  const unsigned factor=curve==2?0x8207:curve==1?0x43cc:0x4144,offset=curve==2?512:256,limit=curve==2?255:curve==1?132:127;
  const unsigned product=unsigned((uint64_t(raw)*factor)>>16)*2;unsigned expected=product<offset?0:(product-offset)>>8;expected=std::min(expected,limit);
  if(curve==1&&expected>64)expected-=std::min(expected-64,5u);
  check(task.scaled()==expected&&task.channel()==channel&&writes==(curve==2?0u:1u)&&reads==(curve==2?0u:1u));++cases;
 }
 std::cout<<"PASS: "<<cases<<" ADC curve policies / "<<intervals<<" intervals; exhaustive16 raw words, linear/bipolar/special curves, center plateau, clipping, channel edges and deadlines\n";
 return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
