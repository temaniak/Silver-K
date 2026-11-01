#include "ksLoaderSessionTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::LoaderSessionTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("LoaderSession logical policy differs");}
int main(){try{uint64_t cases=0,intervals=0;for(unsigned seed=0;seed<256;++seed){T t(T::Kind::Update);unsigned reads=0,writes=0;auto read=[&](T::Read f,uint32_t){++reads;return f==T::Read::BlockCount?1u:f==T::Read::ImageWords?2u:seed;};auto write=[&](T::Write,uint32_t,uint32_t){++writes;};unsigned steps=0;while(!t.complete()&&t.stage()!=S::StopBlockError&&t.stage()!=S::StopChecksumError&&steps++<100000){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())t.resumeChild(t.resumeStage()==S::CompareEntryInputs?0xffbfbfbfu:seed,seed,0xa04002,true);}check(t.complete());check(writes==132);++cases;}
std::cout<<"PASS: "<<cases<<" LoaderSession policies / "<<intervals<<" intervals; logical math/routes/deferred writes and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
