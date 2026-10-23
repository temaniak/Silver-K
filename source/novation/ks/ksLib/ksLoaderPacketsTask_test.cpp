#include "ksLoaderPacketsTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::LoaderPacketsTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("LoaderPackets logical policy differs");}
int main(){try{uint64_t cases=0,intervals=0;for(auto kind:{T::Kind::Header,T::Kind::Footer,T::Kind::Block})for(unsigned tag=0;tag<256;++tag){T t(kind);unsigned reads=0,writes=0;auto read=[&](T::Read){++reads;return 3u;};auto write=[&](T::Write,uint32_t){++writes;};unsigned steps=0;while(!t.complete()&&steps++<256){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild()){auto next=t.resumeStage();unsigned byte=next==S::CompareHeaderTag||next==S::CompareFooterTag||next==S::CompareBlockTag?tag:3;t.resumeChild(byte,0,0,0);}}check(t.complete());if(kind==T::Kind::Header)check(writes==(tag==127?4u:0u));++cases;}
std::cout<<"PASS: "<<cases<<" LoaderPackets policies / "<<intervals<<" intervals; logical math/routes/deferred writes and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
