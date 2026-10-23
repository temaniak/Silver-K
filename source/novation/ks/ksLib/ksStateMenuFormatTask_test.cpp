#include "ksStateMenuFormatTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateMenuFormatTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("Menu format property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned type=0;type<256;++type)for(unsigned value=0;value<256;++value)for(unsigned width:{0u,1u,4u,255u}){
  T t(T::Kind::Format,uint8_t(type),uint8_t(value),3,128);unsigned reads=0,writes=0,characters=0,sign=0,digit=0;
  auto read=[&](T::Read f,uint32_t ref)->uint32_t{++reads;if(f!=T::Read::Byte)return 0x30000;return t.stage()==S::ReadWidth?width:t.stage()==S::ReadPatchMode?7:65;};
  auto write=[&](T::Write f,uint32_t ref,unsigned v){++writes;check(ref==0x11522+(value%12)*4+(f==T::Write::Sign?3:4));if(f==T::Write::Sign)sign=v;else digit=v;};
  unsigned steps=0;while(!t.complete()){auto before=t.stage();const auto cost=t.nextClocks(),n=reads+writes;check(cost&&++steps<1600);check(!t.advanceBefore(cost-1,read,write)&&before==t.stage()&&reads+writes==n);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(before==S::ReadCharacter)++characters;if(t.awaitingChild())check(t.resumeChild());}
  if(type==0x26||(type==0x27&&value)){const int octave=int(value/12)-2;check(writes==2&&sign==unsigned(octave<0?45:32)&&digit==unsigned(48+(octave<0?-octave:octave)));}
  else check(writes==0);
  if(type==4||type==8||type==9||type==0x26||(type==0x27&&value)||type==0x23||type==0x24||type==0x2e||type==0x2f)check(characters==(width?width:256));
  if(type==0x23||type==0x24)check(t.value()==unsigned(bool(value&8)));if(type==0x2e)check(t.value()==(value<63?0u:value==63?2u:1u));++cases;
 }
 std::cout<<"PASS: "<<cases<<" menu format policies / "<<intervals<<" intervals; all kind/value bytes, zero/wrapped widths, note octave writes, bit/tri mappings and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
