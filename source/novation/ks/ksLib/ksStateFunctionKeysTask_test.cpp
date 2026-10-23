#include "ksStateFunctionKeysTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::StateFunctionKeysTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateFunctionKeys policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
 
 for(unsigned key=0;key<256;++key)for(unsigned flags=0;flags<256;++flags)for(unsigned press:{0u,1u}){T t(T::Kind::Event,uint16_t(key|(press<<8)),0x30000);unsigned memory=flags,reads=0,writes=0;auto read=[&](T::Read f,uint32_t)->uint32_t{++reads;return f==T::Read::Flags?memory:f==T::Read::Patch?0x31000:0;};auto write=[&](T::Write f,uint32_t,uint8_t v){++writes;if(f==T::Write::Flags||f==T::Write::RawFlags)memory=v;};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&s==t.stage()&&reads+writes==n);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild(false));}unsigned mask=key&&key<=5?1u<<(key&7):key==9?1u:0u;check(memory==(press?flags|mask:flags&~mask));++cases;}

 std::cout<<"PASS: "<<cases<<" StateFunctionKeys policies / "<<intervals<<" intervals; logical routes, byte arithmetic, child replies and insufficient budgets\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
