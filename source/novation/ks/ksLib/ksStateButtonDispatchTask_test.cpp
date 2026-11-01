#include "ksStateButtonDispatchTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateButtonDispatchTask;using S=T::Stage;
void check(bool b){if(!b)throw std::runtime_error("Button dispatch property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned event=0;event<65536;++event)for(unsigned tag:{0u,1u,2u,3u,127u,128u,255u}){
  T task(T::Kind::Button);unsigned reads=0,actions=0;S finish=S::Complete;
  auto read=[&](T::Read field){++reads;check(task.offset()==((event&31)*3));return field==T::Read::Tag?tag:field==T::Read::Key?17u:23u;};
  unsigned steps=0;while(!task.complete()){
   const auto before=task.stage();const auto cost=task.nextClocks();const auto count=reads;check(cost&&++steps<100);check(!task.advanceBefore(cost-1,read)&&task.stage()==before&&reads==count);check(task.advanceBefore(cost,read)==cost);++intervals;finish=before;
   if(task.awaitingChild()){if(before==S::DispatchAction){++actions;check(task.tag()==tag&&task.key()==17&&task.value()==23);}check(task.resumeChild(uint16_t(event)));}
  }
  const auto group=event&0xe0;const bool valid=!(event&0x8000),descriptor=valid&&group!=0x20&&group!=0x40&&group!=0x60;
  check(reads==(descriptor?3u:0u)&&actions==(descriptor?1u:0u));
  if(!valid)check(finish==S::ReturnEmpty);else if(group==0x20)check(finish==S::SelectPartGroup);else if(group==0x40)check(finish==S::SelectPageGroup);else if(group==0x60)check(finish==S::SelectFunctionGroup);else check(finish==((tag&128)?S::SelectRefresh:S::ReturnButton));++cases;
 }
 std::cout<<"PASS: "<<cases<<" button policies / "<<intervals<<" intervals; full event domain, group/action descriptors, retained refresh flag and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
