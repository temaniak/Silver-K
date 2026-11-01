#include "ksLoaderProgramTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::LoaderProgramTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("LoaderProgram logical policy differs");}
int main(){try{uint64_t cases=0,intervals=0;for(unsigned size=0;size<=0x16004;++size){T t(T::Kind::Image);unsigned reads=0,writes=0;auto read=[&](T::Read f){++reads;return f==T::Read::Size?size:1u;};auto write=[&](T::Write,uint16_t){++writes;};unsigned expectedPages=0;while(!t.complete()&&t.stage()!=S::StopImageError){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())t.resumeChild(0);}check(t.complete()==(size+4<=0x16000));++cases;}
std::cout<<"PASS: "<<cases<<" LoaderProgram policies / "<<intervals<<" intervals; logical math/routes/deferred writes and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
