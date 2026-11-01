#include "ksStateProgramBrowseTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateProgramBrowseTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateProgramBrowse policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
for(unsigned value=0;value<65536;++value)for(unsigned step:{0u,1u,0xffffu}){T t(T::Kind::Step,step,value,600);auto r=[](T::Read,uint32_t){return 0u;};auto w=[](T::Write,uint8_t){};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks();check(cost&&!t.advanceBefore(cost-1,r,w)&&s==t.stage());check(t.advanceBefore(cost,r,w)==cost);++intervals;}unsigned expected=uint16_t(value+step);if(expected&32768)expected=uint16_t(expected+600);if(expected>=600)expected=uint16_t(expected-600);check(t.value()==expected);++cases;}

// The original parser advances phase from the observed ASCII bank byte.
for(unsigned first=0;first<256;++first)for(unsigned digit=0;digit<256;++digit){T t(T::Kind::Entry,digit);unsigned phase=999,reads=0,writes=0;auto r=[&](T::Read f,uint32_t){++reads;return f==T::Read::Latch?1u:f==T::Read::First?first:0u;};auto w=[&](T::Write f,uint8_t value){++writes;if(f==T::Write::Latch)phase=value;};while(!t.complete()){auto stage=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,r,w)&&stage==t.stage()&&reads+writes==n);check(t.advanceBefore(cost,r,w)==cost);++intervals;if(t.awaitingChild())t.resumeChild(0);}check(phase==(first<=52||digit<=4?uint8_t(first+1):999u));++cases;}
std::cout<<"PASS: "<<cases<<" StateProgramBrowse policies / "<<intervals<<" intervals; logical routes/math/deferred effects and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
