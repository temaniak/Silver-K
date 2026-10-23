#include "ksClockDisplayTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::ClockDisplayTask;
void check(bool v){if(!v)throw std::runtime_error("Clock display property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned status=0;status<65536;++status)for(uint32_t period:{0u,1u,0xd1b7u,0x7fffffffu,0x80000000u,0xffffffffu}){
  T t(T::Kind::Display);unsigned reads=0;auto read=[&](T::Read field){++reads;return field==T::Read::Period?period:field==T::Read::Active?status&255:status>>8;};
  while(!t.complete()){const auto stage=t.stage();const auto cost=t.nextClocks();const auto before=reads;check(cost&&!t.advanceBefore(cost-1,read)&&t.stage()==stage&&reads==before);check(t.advanceBefore(cost,read)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild());}
  uint8_t active=status;const uint8_t flags=status>>8;if(active&&!(flags&1))++active;const auto q=uint16_t(period/0xd1b7u);const auto n=uint16_t(q>>5);const auto expected=uint16_t((n&0xff00)|uint8_t(n+((q>>4)&1)));
  check(t.index()==uint8_t(active<<2)&&t.number()==expected&&reads==(uint8_t(status)?3u:2u));++cases;
 }
 std::cout<<"PASS: "<<cases<<" clock display policies / "<<intervals<<" intervals; byte status/index wrap, quotient and low-byte rounding, children and immutable deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
