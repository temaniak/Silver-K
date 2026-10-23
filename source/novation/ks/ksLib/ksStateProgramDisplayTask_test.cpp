#include "ksStateProgramDisplayTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateProgramDisplayTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateProgramDisplay policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
for(auto kind:{T::Kind::Program,T::Kind::Heading,T::Kind::Category})for(unsigned value=0;value<256;++value){T t(kind);unsigned reads=0,writes=0,children=0;auto r=[&](T::Read,uint32_t){++reads;return value;};auto w=[&](T::Write,uint8_t){++writes;};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,r,w)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,r,w)==cost);++intervals;if(t.awaitingChild()){++children;t.resumeChild();}}if(kind==T::Kind::Category)check(reads==11&&children==21&&writes==0);++cases;}

std::cout<<"PASS: "<<cases<<" StateProgramDisplay policies / "<<intervals<<" intervals; logical routes/math/deferred effects and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
