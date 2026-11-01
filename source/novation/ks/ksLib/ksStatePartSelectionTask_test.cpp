#include "ksStatePartSelectionTask.h"
#include <iostream>
#include <stdexcept>
#include <array>
using T=ks::StatePartSelectionTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("Part selection property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b)for(unsigned previous:{0u,1u,15u,128u,255u}){
  T t(T::Kind::Resolve);unsigned request=999,current=999,reads=0,writes=0;
  auto read=[&](T::Read f)->uint8_t{++reads;return f==T::Read::Preferred?a:f==T::Read::Available?b:previous;};
  auto write=[&](T::Write f,uint8_t v){++writes;if(f==T::Write::Request)request=v;else if(f==T::Write::Current)current=v;else check(false);};
  while(!t.complete()){auto before=t.stage();const auto cost=t.nextClocks(),r=reads,w=writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&before==t.stage()&&reads==r&&writes==w);check(t.advanceBefore(cost,read,write)==cost);++intervals;}
  unsigned expected=(a&b)?a&b:(previous&b)?previous&b:(b&1)?1:(b&2)?2:(b&4)?4:8;check(request==expected&&current==expected&&writes==2);++cases;
 }
 for(unsigned before=0;before<256;++before)for(unsigned after=0;after<256;++after)for(unsigned mode:{0u,1u,255u}){
  T t(T::Kind::Changed,0,uint8_t(before),uint8_t(after));std::array<unsigned,4> parts{};unsigned count=0,children=0;
  auto read=[&](T::Read)->uint8_t{return mode;};auto write=[&](T::Write f,uint8_t v){check(f==T::Write::Part&&count<4);parts[count++]=v;};
  while(!t.complete()){auto stage=t.stage();const auto cost=t.nextClocks();check(cost&&!t.advanceBefore(cost-1,read,write)&&stage==t.stage());check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild()){++children;check(t.resumeChild());}}
  unsigned expected=0;for(unsigned bit=0;bit<4;++bit)if(mode&&(before&~after&(1u<<bit)))check(parts[expected++]==bit+1);check(count==expected&&children==2*expected);++cases;
 }
 for(unsigned event=0;event<65536;++event){T t(T::Kind::Held,uint16_t(event));unsigned held=event^0x73,request=event^0xa5,current=0;auto read=[&](T::Read f)->uint8_t{return f==T::Read::Held?held:f==T::Read::HeldRequest?request:0;};auto write=[&](T::Write f,uint8_t v){if(f==T::Write::Held)held=v;if(f==T::Write::HeldRequest)request=v;if(f==T::Write::HeldCurrent)current=v;};
  const auto initial=uint8_t(held);while(!t.complete()){const auto cost=t.nextClocks();check(cost&&t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild());}
  check(uint8_t(held)==(event>>8?uint8_t(initial|event):uint8_t(initial&(uint8_t(event)^15))));if(event>>8)check(current==uint8_t((initial?uint8_t(event^0xa5):0)|event));++cases;
 }
 std::cout<<"PASS: "<<cases<<" part-selection policies / "<<intervals<<" intervals; all byte masks/previous choices and before/after modes, held word events, exact reset-part order and insufficient budgets\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
