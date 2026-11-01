#include "ksStateEditContextTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateEditContextTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("Edit context property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(auto kind:{T::Kind::Bind,T::Kind::Clear,T::Kind::Display})for(unsigned type=0;type<256;++type)for(unsigned screen=0;screen<256;++screen)for(bool edit:{false,true}){
  T t(kind,3);unsigned published=999,reads=0,writes=0,referencePrints=0,matches=0;
  auto read=[&](T::Read f,uint32_t ref){++reads;return f==T::Read::ContextKind?type:f==T::Read::Screen?screen:f==T::Read::EditBase?(edit?0x168fcu:0u):f==T::Read::PatchBase?0x280000u:f==T::Read::ValueKind?type:screen;};auto write=[&](T::Write,uint32_t v){++writes;published=v;};
  while(!t.complete()){auto before=t.stage();const auto cost=t.nextClocks(),access=reads+writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&t.stage()==before&&access==reads+writes);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(before==S::PrintMatchText)++matches;if(before==S::DisplayReferenceValue)++referencePrints;if(t.awaitingChild())check(t.resumeChild(uint8_t(type)));}
  if(kind==T::Kind::Bind)check(published==((type>=126||screen==0||screen==9)?0x168fcu:0u));if(kind==T::Kind::Clear)check(published==0);if(kind==T::Kind::Display)check(referencePrints==unsigned(type&&type!=screen)&&matches==unsigned(type&&type==screen));++cases;
 }
 std::cout<<"PASS: "<<cases<<" edit context policies / "<<intervals<<" intervals; context kind/screen, live bases, retained comparison and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
