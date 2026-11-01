#include "ksPanelSettingsTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::PanelSettingsTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("Panel settings property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(auto kind:{T::Kind::Reset,T::Kind::Select,T::Kind::Show,T::Kind::Poll,T::Kind::Coefficients,T::Kind::First,T::Kind::Second,T::Kind::Load,T::Kind::Save})for(unsigned a=0;a<256;++a)for(unsigned b:{0u,4u,5u,32u,34u,35u,127u,128u,255u}){
  T t(kind,uint8_t(b));unsigned duty=a,notice=a,mode=b,reads=0,writes=0,bytes=0;std::array<unsigned,256> copy{};
  auto read=[&](T::Read f,unsigned at){++reads;return f==T::Read::Notice?notice:f==T::Read::Mode?mode:f==T::Read::Duty?duty:(a+at)&255;};
  auto write=[&](T::Write f,unsigned value){++writes;switch(f){case T::Write::Notice:notice=value;break;case T::Write::Mode:mode=value;break;case T::Write::Duty:duty=value;break;case T::Write::CalibrationByte:check(t.position()<256);copy[t.position()]=value;++bytes;break;default:check(value==0x60a0);break;}};
  unsigned steps=0;while(!t.complete()){const auto before=t.stage();const auto cost=t.nextClocks();const auto access=reads+writes;check(cost&&++steps<2000);check(!t.advanceBefore(cost-1,read,write)&&t.stage()==before&&access==reads+writes);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild({uint8_t(before==S::ReadEncoder?b:34),0,true}));}
  if(kind==T::Kind::Poll){const int expected=int(a)+int(int8_t(b));check(duty==unsigned(expected<0?0:expected>255?255:expected));}
  if(kind==T::Kind::Load){check(bytes==256);for(unsigned i=0;i<256;++i)check(copy[i]==((a+i)&255));}
  if(kind==T::Kind::Reset)check(notice==0);++cases;
 }
 std::cout<<"PASS: "<<cases<<" panel settings policies / "<<intervals<<" intervals; copy256, notices, modes, signed encoder clamp and immutable deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
