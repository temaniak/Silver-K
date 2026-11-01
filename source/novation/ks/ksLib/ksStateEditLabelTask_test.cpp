#include "ksStateEditLabelTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateEditLabelTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateEditLabel policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
for(unsigned index=0;index<256;++index){T t(T::Kind::Label,index);unsigned reads=0,writes=0;auto r=[&](T::Read f,uint32_t ref){++reads;return f==T::Read::Patch?0x32000u:(ref*17+index)&255;};auto w=[&](T::Write,uint32_t,uint32_t){++writes;};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,r,w)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,r,w)==cost);++intervals;if(t.awaitingChild())t.resumeChild();}check(writes>=17);++cases;}

std::cout<<"PASS: "<<cases<<" StateEditLabel policies / "<<intervals<<" intervals; logical routes/math/deferred effects and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
