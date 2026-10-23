#include "ksStateBankNavigationTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateBankNavigationTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateBankNavigation policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
for(unsigned value=0;value<256;++value)for(unsigned delta=1;delta<256;++delta){T t(T::Kind::BankEntry,uint16_t(delta<<8));unsigned reads=0,writes=0,result=999;auto r=[&](T::Read f,uint32_t){++reads;return f==T::Read::Number?value:0u;};auto w=[&](T::Write f,uint32_t,uint8_t v){++writes;if(f==T::Write::Number)result=v;};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,r,w)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,r,w)==cost);++intervals;if(t.awaitingChild())t.resumeChild(0);}unsigned expected=uint8_t(value+delta);if(expected&128)expected=uint8_t(expected+100);if(expected>=100)expected-=100;check(result==expected);++cases;}

std::cout<<"PASS: "<<cases<<" StateBankNavigation policies / "<<intervals<<" intervals; logical routes/math/deferred effects and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
