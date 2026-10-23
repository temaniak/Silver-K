#include "ksPanelAdcScanTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::PanelAdcScanTask;using S=T::Stage;
void check(bool b){if(!b)throw std::runtime_error("ADC scan policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned kind=0;kind<4;++kind)for(unsigned group=0;group<256;++group)for(unsigned pair:{0u,3u}){
  T task(kind==0?T::Kind::Initialize:kind==1?T::Kind::Start:kind==2?T::Kind::Mux:T::Kind::ResetLast);unsigned polls=0,starts=0,snapshots=0,resets=0,selects=0,writes=0,delays=0;std::array<unsigned,5> last{},counts{};
  auto read=[&](T::Read field){if(field==T::Read::CompletedGroup)return ++polls%3?32u:33u;return group;};
  auto write=[&](T::Write field,unsigned value){const auto i=unsigned(field);last[i]=value;++counts[i];++writes;};
  while(!task.complete()){
   if(task.awaitingChild()){switch(task.resumeStage()){case S::SaveDelayBase:++starts;break;case S::ResetControls:++snapshots;break;case S::SetLast:++resets;break;case S::SetConversion:++selects;break;default:check(false);}check(task.resumeChild());continue;}
   const auto stage=task.stage();const auto cost=task.nextClocks(pair);const auto before=polls+writes;
   check(!task.advanceBefore(cost-1,read,write,pair)&&task.stage()==stage&&before==polls+writes);check(task.advanceBefore(cost,read,write,pair)==cost);if(stage==S::DecrementDelay)++delays;++intervals;
  }
  if(!kind)check(starts==16&&snapshots==1&&resets==1&&polls==48&&delays==16*2663&&counts[unsigned(T::Write::Group)]==1&&last[unsigned(T::Write::Group)]==36&&last[unsigned(T::Write::Last)]==255);
  if(kind==1)check(selects==unsigned(group>32)&&writes==(group>32?2u:0u));
  if(kind==2)check(writes==2&&last[unsigned(T::Write::Mux)]==group/4&&last[unsigned(T::Write::Settling)]==9);
  if(kind==3)check(writes==1&&last[unsigned(T::Write::Last)]==255&&!starts&&!selects);
  ++cases;
 }
 std::cout<<"PASS: "<<cases<<" ADC scan policies / "<<intervals<<" intervals; all byte groups, mux/settling, sixteen exact startup delays, live completion polls and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
