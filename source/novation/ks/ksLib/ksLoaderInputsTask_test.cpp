#include "ksLoaderInputsTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::LoaderInputsTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("LoaderInputs logical policy differs");}
int main(){try{uint64_t cases=0,intervals=0;for(auto kind:{T::Kind::Sample,T::Kind::Released,T::Kind::Stable,T::Kind::Pressed,T::Kind::Confirm})for(unsigned value=0;value<256;++value){T t(kind);unsigned reads=0,writes=0;auto read=[&](T::Read){++reads;return value;};auto write=[&](T::Write,uint8_t){++writes;};unsigned steps=0;while(!t.complete()&&steps++<300000){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())t.resumeChild(kind==T::Kind::Pressed?value:kind==T::Kind::Confirm&&t.resumeStage()==S::CompareConfirmation?0xffffffef:0xffffffff);}check(t.complete());if(kind==T::Kind::Sample)check(t.sample()==value*0x1010101u&&reads==4&&writes==7);++cases;}
std::cout<<"PASS: "<<cases<<" LoaderInputs policies / "<<intervals<<" intervals; logical math/routes/deferred writes and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
