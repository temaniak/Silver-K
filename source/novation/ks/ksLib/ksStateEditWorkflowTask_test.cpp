#include "ksStateEditWorkflowTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::StateEditWorkflowTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateEditWorkflow policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
 
 for(unsigned group=0;group<256;++group)for(unsigned key=0;key<256;++key){T t(T::Kind::ResolveNote);unsigned reads=0;auto read=[&](T::Read f,uint32_t)->uint32_t{++reads;return f==T::Read::Group?group:key;};auto write=[](T::Write,uint32_t){};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads;check(cost&&!t.advanceBefore(cost-1,read,write)&&s==t.stage()&&n==reads);check(t.advanceBefore(cost,read,write)==cost);++intervals;}check(t.key()==(group>4&&key<=48?key+36:0));++cases;}
 for(auto kind:{T::Kind::ManualEdit,T::Kind::AutomaticEvent})for(unsigned key=0;key<256;++key)for(unsigned variant=0;variant<256;++variant){T t(kind,key);unsigned handlerRef=0;auto read=[&](T::Read f,uint32_t ref)->uint32_t{if(f==T::Read::Patch)return 0x30000;if(f==T::Read::Handler){handlerRef=ref;return 0;}return ref==0x30000+226?variant:uint8_t(ref);};auto write=[](T::Write,uint32_t){};while(!t.complete()){auto cost=t.nextClocks();check(cost&&t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild(false,0));}const unsigned effective=key==31?uint8_t(key+variant):key;check(t.offset()==effective*8&&handlerRef==0x1013e + uint8_t(uint8_t(0xfd66+8*effective)<<2)&&t.originalKey()==key);++cases;}

 std::cout<<"PASS: "<<cases<<" StateEditWorkflow policies / "<<intervals<<" intervals; logical routes, byte arithmetic, child replies and insufficient budgets\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
