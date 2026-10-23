#include "ksStateBrowseWorkflowTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateBrowseWorkflowTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateBrowseWorkflow policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
for(auto kind:{T::Kind::Save,T::Kind::Restore})for(unsigned seed=0;seed<256;++seed){T t(kind);unsigned reads=0,writes=0;auto r=[&](T::Read f,uint32_t ref){++reads;return f==T::Read::Patch?0x30001u:(ref+seed)&255;};auto w=[&](T::Write f,uint32_t ref,uint8_t v){check(f==T::Write::Byte);check(ref==(kind==T::Kind::Save?0x167fc:0x30001)+writes);check(v==uint8_t((kind==T::Kind::Save?0x30001:0x167fc)+writes+seed));++writes;};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,r,w)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,r,w)==cost);++intervals;if(t.awaitingChild())t.resumeChild(0);}check(reads==257&&writes==256);++cases;}

std::cout<<"PASS: "<<cases<<" StateBrowseWorkflow policies / "<<intervals<<" intervals; logical routes/math/deferred effects and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
