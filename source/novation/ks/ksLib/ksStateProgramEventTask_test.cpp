#include "ksStateProgramEventTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateProgramEventTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateProgramEvent policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
for(unsigned bank=0;bank<256;++bank)for(unsigned index=0;index<256;++index){T t(T::Kind::Event,uint16_t((bank<<8)|index));unsigned writes=0,reads=0;auto r=[&](T::Read,uint32_t){++reads;return 1u;};auto w=[&](T::Write,uint32_t,uint8_t){++writes;};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,r,w)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,r,w)==cost);++intervals;if(t.awaitingChild())t.resumeChild(0x30000);}if(bank&64)check(t.exit()==T::Exit::Multi&&writes==0);else if(!(bank&128)&&!(bank&32)&&bank==0)check(writes==0);++cases;}

std::cout<<"PASS: "<<cases<<" StateProgramEvent policies / "<<intervals<<" intervals; logical routes/math/deferred effects and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
