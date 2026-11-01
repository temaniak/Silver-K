#include "ksStateScreenEditTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateScreenEditTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("Screen edit property differs");}
unsigned expected(unsigned value,unsigned delta,unsigned low,unsigned high){if(low==128)return delta&128?value&~(1u<<(high&7)):value|(1u<<(high&7));if(low==129){if(!delta)return value;if(delta&128)return !value?0:value==127?0:127;return value==63?63:value>63?63:127;}const auto sum=uint8_t(value+delta);return delta&128?(int8_t(sum)<int8_t(low)?low:sum):(sum>high?high:sum);}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned value=0;value<256;++value)for(unsigned delta=0;delta<256;++delta)for(unsigned low:{0u,63u,127u,128u,129u,130u,255u})for(unsigned high:{0u,7u,63u,127u,255u}){
  T t(T::Kind::Edit,uint8_t(delta));unsigned reads=0;auto read=[&](T::Read f,uint32_t ref)->uint32_t{++reads;if(f==T::Read::Descriptor)return 0x30000;if(f==T::Read::EditBase)return 0;if(f==T::Read::PatchBase)return 0x280000;return ref==0x30010?3:ref==0x30012?low:ref==0x30013?high:ref==0x169e0?99:value;};auto write=[](int,unsigned){};
  while(!t.complete()){auto before=t.stage();const auto cost=t.nextClocks(),n=reads;check(cost&&!t.advanceBefore(cost-1,read,write)&&before==t.stage()&&reads==n);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild());}
  check(t.value()==expected(value,delta,low,high)&&t.level()==99);++cases;
 }
 std::cout<<"PASS: "<<cases<<" screen edit policies / "<<intervals<<" intervals; all byte values/deltas, signed and unsigned clamps, bit/tri modes and live reads/deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
