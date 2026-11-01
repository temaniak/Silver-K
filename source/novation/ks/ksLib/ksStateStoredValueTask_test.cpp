#include "ksStateStoredValueTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateStoredValueTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateStoredValue policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
for(unsigned field=0;field<256;++field){T t(T::Kind::Read,field);unsigned reads=0;uint32_t observed=0;auto r=[&](T::Read f,uint32_t ref){++reads;observed=ref;return f==T::Read::Multi?0x34000u:ref&255;};auto w=[](int,int){};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads;check(cost&&!t.advanceBefore(cost-1,r,w)&&s==t.stage()&&n==reads);check(t.advanceBefore(cost,r,w)==cost);++intervals;if(t.awaitingChild()){const auto next=t.resumeStage();const bool classify=field==126||(field>=84&&field<=124)||field>=212;t.resumeChild(0x33000,next==S::SelectPatchField?classify:false);}}check(t.value()==(field>=222&&field<234?0:(field>=234?(0x81fe16+field)&255:field)));++cases;}

std::cout<<"PASS: "<<cases<<" StateStoredValue policies / "<<intervals<<" intervals; logical routes/math/deferred effects and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
