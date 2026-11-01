#include "ksStateScreenActionsTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::StateScreenActionsTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateScreenActions policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
 
 for(auto kind:{T::Kind::Single,T::Kind::Multi,T::Kind::Down,T::Kind::Up,T::Kind::ToggleEdit,T::Kind::SelectEdit,T::Kind::ToggleOne,T::Kind::ToggleTwo,T::Kind::ClearUi})for(unsigned v=0;v<256;++v)for(unsigned p:{0u,1u,2u,3u,255u})for(bool pressed:{false,true}){
  T t(kind,uint16_t(v*257),pressed);unsigned reads=0,writes=0;auto read=[&](T::Read f,uint32_t)->uint32_t{++reads;switch(f){case T::Read::UiPage:return p;case T::Read::PatchBase:return 0x30000;case T::Read::PatchByte:return v;default:return v;}};auto write=[&](T::Write,uint16_t){++writes;};while(!t.complete()){const auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild(bool(v&1)));}
  if(kind==T::Kind::ToggleOne||kind==T::Kind::ToggleTwo){check(t.exit()==(pressed?T::Exit::ApplyPatch:T::Exit::Release));if(pressed)check(t.value()==(v^1));}if(kind==T::Kind::ClearUi)check(writes==11&&t.exit()==T::Exit::ShowUi);if(kind==T::Kind::Down||kind==T::Kind::Up){const auto e=!pressed?T::Exit::Release:p==2?T::Exit::NameStep:p==3?T::Exit::PanelStep:v?T::Exit::SpecialStep:T::Exit::None;check(t.exit()==e);}++cases;
 }
 std::cout<<"PASS: "<<cases<<" StateScreenActions policies / "<<intervals<<" intervals; logical routes, byte arithmetic, child replies and insufficient budgets\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
