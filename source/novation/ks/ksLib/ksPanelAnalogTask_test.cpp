#include "ksPanelAnalogTask.h"
#include <array>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using T=ks::PanelAnalogTask;using S=T::Stage;
void check(bool value){if(!value)throw std::runtime_error("Panel analog property differs");}
uint8_t expectedScale(unsigned raw,unsigned offset,unsigned factor,unsigned bias){
 const int product=int(int8_t(uint8_t(offset+raw+128)))*int(factor);unsigned word=unsigned(product)&65535;
 if(product<0){word=(word+bias)&65535;if(int16_t(word)>=0)word=0;else if(int16_t(word)<-16384)word=0xc000;}
 else{word=(word-bias)&65535;if(int16_t(word)<0)word=0;else if(int16_t(word)>16256)word=16256;}
 return uint8_t((((word<<1)&65535)>>8)+128);
}
int main(){try{uint64_t cases=0,intervals=0;
 auto run=[&](T& task,auto&& read,auto&& write){unsigned steps=0;while(!task.complete()){auto before=task.stage();auto cost=task.nextClocks();check(cost&&++steps<100);check(!task.advanceBefore(cost-1,read,write)&&task.stage()==before);check(task.advanceBefore(cost,read,write)==cost);++intervals;check(!task.awaitingChild());}++cases;};
 for(unsigned raw=0;raw<256;++raw)for(unsigned offset=0;offset<256;++offset)for(unsigned factor:{0u,1u,63u,64u,127u,128u,255u})for(unsigned bias:{0u,0x400u,0x8000u,0xffffu}){
  T task(T::Kind::Scale,uint8_t(raw),0,uint16_t(bias));auto read=[&](T::Read field){return field==T::Read::Factor?factor:offset;};auto write=[](T::Write,unsigned){check(false);};run(task,read,write);check(task.input()==expectedScale(raw,offset,factor,bias));
 }
 for(unsigned minimum=0;minimum<256;++minimum)for(unsigned maximum=0;maximum<256;++maximum){
  for(unsigned raw:{0u,1u,63u,127u,128u,254u,255u}){T task(T::Kind::Range,uint8_t(raw),0,0);unsigned low=minimum,high=maximum;auto read=[&](T::Read field){return field==T::Read::Minimum?low:high;};auto write=[&](T::Write field,unsigned value){if(field==T::Write::Minimum)low=value;else high=value;};run(task,read,write);check(low==std::min(raw,minimum)&&high==std::max(raw,maximum));}
  for(unsigned dividend:{0u,0x8800u,0x9000u,0xffffu}){T task(T::Kind::Coefficients,0,0,uint16_t(dividend));unsigned offset=999,factor=999;auto read=[&](T::Read field){return field==T::Read::Minimum?minimum:maximum;};auto write=[&](T::Write field,unsigned value){if(field==T::Write::Offset)offset=value;else factor=value;};run(task,read,write);const auto span=uint8_t(maximum-minimum);check(offset==uint8_t(128-uint8_t(minimum+(span>>1)))&&factor==(span?uint8_t(dividend/span):0));}
 }
 for(unsigned raw=0;raw<256;++raw){T task(T::Kind::Level,uint8_t(raw),0,0);unsigned output=999;auto read=[](T::Read){return 0;};auto write=[&](T::Write field,unsigned value){check(field==T::Write::ControlCount);output=value;};run(task,read,write);check(output==uint8_t(uint8_t(64-raw)*2+64));T square(T::Kind::Square,uint8_t(raw),0,0);auto reject=[](T::Write,unsigned){check(false);};run(square,read,reject);check(square.input()==raw*raw/512);}
 std::cout<<"PASS: "<<cases<<" panel analog policies / "<<intervals<<" intervals; signed wrapping scale, all offsets/min-max, clipping, zero divisor, output bytes and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
