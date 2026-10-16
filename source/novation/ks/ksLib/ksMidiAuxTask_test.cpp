#include "ksMidiAuxTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::MidiAuxTask;using S=T::Stage;using K=T::Kind;using R=T::Read;using W=T::Write;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 auto verify=[&](K kind,unsigned cc,unsigned value,unsigned selector,unsigned bank,unsigned program,unsigned latency){
  T task(kind);unsigned clocks=0,reads=0;std::vector<unsigned> children;std::vector<std::pair<W,unsigned>> writes;
  while(!task.complete()&&!task.handoff()){
   if(task.awaitingPart()){check(task.resumePart(17),"Aux part reply rejected");children.push_back(0x4000);continue;}
   if(task.awaitingChild()){children.push_back(task.stage()==S::AwaitEntry?0x6cfe:task.stage()==S::AwaitGeneric?0x6d10:task.stage()==S::AwaitStart?0xa936:0xa934);check(task.resumeChild(),"Aux child reply rejected");continue;}
   auto read=[&](R field)->unsigned{++reads;switch(field){case R::Controller:return cc;case R::Value:return value;case R::EventFirst:return kind==K::GlobalControl?cc:program;case R::EventSecond:return value;case R::Selector:check(task.part()==17,"Selector part lost");return selector;default:check(task.part()==17,"Bank part lost");return bank;}};
   auto write=[&](W field,unsigned data){check(task.part()==17,"Aux publication part lost");writes.emplace_back(field,data);};auto timing=[&](unsigned){return latency;};
   const auto cost=task.nextClocks(task.publication()?latency:3);check(cost>0,"Aux task stalled");const auto before=reads;const auto beforeWrites=writes.size();
   check(!task.advanceBefore(cost-1,read,write,timing)&&reads==before&&writes.size()==beforeWrites,"Early aux access");check(task.advanceBefore(cost,read,write,timing)==cost,"Aux admission differs");clocks+=cost;
  }
  std::vector<unsigned> expected;std::vector<std::pair<W,unsigned>> published;unsigned duration=0,pubs=0;
  if(kind==K::Start){expected={0xa936};duration=37;}else if(kind==K::Continue){expected={0xa934};duration=37;}else if(kind==K::Stop){duration=8;check(task.stage()==S::HandoffStop,"Stop did not hand off");}
  else if(kind==K::Program||kind==K::ConditionalProgram){expected={0x4000};if(kind==K::ConditionalProgram&&bank>=128){duration=55;check(task.complete(),"Negative conditional bank loaded");}else{duration=kind==K::Program?67:70;pubs=1;published={{W::InvalidateBank,255}};check(task.bank()==bank&&task.program()==program&&task.stage()==S::HandoffProgram,"Program request changed");}}
  else if(kind==K::GlobalControl&&cc==32){duration=value&0x60?82:47;if(value&0x60){expected={0x4000};published={{W::Bank,value}};pubs=1;}}
  else{
   if(kind==K::GlobalControl)duration=21;
   if(cc==98){duration+=67;expected={0x4000};published={{W::Selector,value}};pubs=1;}
   else if(cc==6){duration+=selector>=128?76:96;expected={0x4000};if(selector<128)expected.push_back(0x6cfe);check(task.selector()==selector&&task.value()==value,"Data-entry selector/value changed");}
   else{duration+=cc==122?(value==63?55:value<63?67:73):61;expected={0x6d10};const unsigned wanted=cc==122?(value==63?63:value<63?0:127):value;check(task.controller()==cc&&task.value()==wanted,"Local-control value threshold changed");}
  }
  check(children==expected&&writes==published,"Aux MIDI child/publication order differs");check(clocks==duration-pubs*(3-latency),"Whole aux own schedule differs");check(!task.resumePart(0)&&!task.resumeChild(),"Spurious aux reply accepted");++cases;
 };
 for(auto kind:{K::Parameter,K::GlobalControl})for(unsigned cc=0;cc<256;++cc)for(unsigned value=0;value<256;++value)for(unsigned latency:{0u,3u})verify(kind,cc,value,uint8_t(cc+value),0,0,latency);
 for(auto kind:{K::Program,K::ConditionalProgram})for(unsigned bank=0;bank<256;++bank)for(unsigned program=0;program<256;++program)verify(kind,0,0,0,bank,program,3);
 for(unsigned value=0;value<256;++value)for(unsigned selector=0;selector<256;++selector)verify(K::Parameter,6,value,selector,0,0,3);
 for(auto kind:{K::Start,K::Continue,K::Stop})verify(kind,0,0,0,0,0,3);
 std::cout<<"PASS: "<<cases<<" native auxiliary MIDI local-control/selector/data-entry/global-bank/program/transport properties; full raw bytes, exact threshold63 behavior/negative bank policy, semantic children, publications, complete schedules/deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
