#include "ksStateStatusDisplayTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateStatusDisplayTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateStatusDisplay policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
for(unsigned program:{0x0010u,0x0530u,0x0531u})for(unsigned mismatch=0;mismatch<260;++mismatch){T t(T::Kind::Compare);unsigned reads=0,writes=0;bool changed=false;auto r=[&](T::Read f,uint32_t ref){++reads;if(f==T::Read::Program)return program;if(f==T::Read::Patch)return 0x34000u;if(f==T::Read::Byte)return (ref&255)^unsigned(ref>=0x34000&&ref-0x34000==mismatch);return 0u;};auto w=[&](T::Write,uint32_t){++writes;};while(!t.complete()){changed|=t.stage()==S::BindChangedMarker;auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,r,w)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,r,w)==cost);++intervals;if(t.awaitingChild())t.resumeChild(false,0,0x33000);}bool expected=program==0x0010?mismatch<179:program==0x0531?(mismatch>=84&&mismatch<125)||mismatch==126:mismatch<84||mismatch==125||(mismatch>=127&&mismatch<209);check(changed==expected&&writes==0);++cases;}

std::cout<<"PASS: "<<cases<<" StateStatusDisplay policies / "<<intervals<<" intervals; logical routes/math/deferred effects and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
