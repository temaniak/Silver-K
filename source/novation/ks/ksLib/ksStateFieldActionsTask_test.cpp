#include "ksStateFieldActionsTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateFieldActionsTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateFieldActions policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
for(unsigned value=0;value<256;++value)for(unsigned limit=0;limit<256;++limit){T t(T::Kind::Cycle,0x30000,0x31000,uint16_t(value<<8),0,0,1);unsigned reads=0;auto r=[&](T::Read,uint32_t){++reads;return limit;};auto w=[](T::Write,uint32_t,uint32_t){};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads;check(cost&&!t.advanceBefore(cost-1,r,w)&&s==t.stage()&&reads==n);check(t.advanceBefore(cost,r,w)==cost);++intervals;}check(t.value()==(uint8_t(value+1)<=limit?uint8_t(value+1):0));++cases;}

std::cout<<"PASS: "<<cases<<" StateFieldActions policies / "<<intervals<<" intervals; logical routes/math/deferred effects and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
