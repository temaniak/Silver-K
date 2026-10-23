#include "ksPanelAdcIrqTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::PanelAdcIrqTask;using S=T::Stage;
void check(bool b){if(!b)throw std::runtime_error("ADC IRQ threshold policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
 auto run=[&](unsigned count,unsigned base,unsigned delta,bool armed,unsigned group,unsigned returnCost,unsigned paired){
  T task;std::array<unsigned,4> currents{},histories{},accepted{},writes{},previews{},publications{};unsigned countWrites=0,statusWrites=0,groupWrites=0,samples=0,mux=0;uint8_t last=uint8_t(delta&1?group:255);
  for(unsigned i=0;i<4;++i){currents[i]=(base+i*971)&65535;histories[i]=(currents[i]+delta)&65535;}
  auto read=[&](T::Read field)->unsigned{const unsigned i=task.offset()/2-unsigned(uint8_t(group<<1))/2;switch(field){
   case T::Read::Remaining:return count;case T::Read::Status:return 255;case T::Read::Group:case T::Read::ScaleGroup:case T::Read::NextGroup:return group;
   case T::Read::Current:check(i<4);return currents[i];case T::Read::History:case T::Read::Preview:check(i<4);return histories[i];case T::Read::Armed:return armed?1:0;case T::Read::Last:return last;
  }return 0;};
  auto write=[&](T::Write field,unsigned value){const unsigned i=task.offset()/2-unsigned(uint8_t(group<<1))/2;switch(field){
   case T::Write::Remaining:check(count&&value==count-1);++countWrites;break;
   case T::Write::Status:check(value==(statusWrites?0x73u:count?0x7fu:0x5fu));++statusWrites;break;
   case T::Write::Armed:check(i<4&&!value);break;
   case T::Write::History:check(i<4);accepted[i]=value;++writes[i];break;
   case T::Write::Group:check(value==((group+4)&255));++groupWrites;break;
  }};
  unsigned steps=0;
  while(!task.complete()){
   if(task.awaitingChild()){const auto resume=task.resumeStage();const unsigned i=task.offset()/2-unsigned(uint8_t(group<<1))/2;
    if(resume==S::RestoreShortFourth||resume==S::ClearGroup)++samples;
    else if(resume==S::SetConversion)++mux;
    else{check(i<4);if(resume==S::RestorePreview){check(task.preview()==histories[i]);++previews[i];last=task.channel();}else{check(resume==S::SkipEvent||resume==S::AdvanceWord);++publications[i];if(resume==S::SkipEvent)last=task.channel();}}
    check(task.resumeChild());continue;
   }
   const auto stage=task.stage();const unsigned cost=task.nextClocks(returnCost,paired),before=countWrites+statusWrites+groupWrites;
   auto rejected=task;unsigned callbacks=0;
   check(!rejected.advanceBefore(cost-1,[&](T::Read){++callbacks;return 0;},[&](T::Write,unsigned){++callbacks;},returnCost,paired)&&rejected.stage()==stage&&!callbacks);
   check(task.advanceBefore(cost,read,write,returnCost,paired)==cost);check(countWrites+statusWrites+groupWrites>=before);check(++steps<500);++intervals;
  }
  check(countWrites==unsigned(count!=0)&&samples==unsigned(count<=1));
  if(!count){
   const unsigned magnitude=delta&32768?(65536-delta)&65535:delta;const bool changed=delta&&magnitude>=(armed?2048u:512u);
   for(unsigned i=0;i<4;++i){check(writes[i]==unsigned(changed)&&publications[i]==unsigned(changed));if(changed)check(accepted[i]==((currents[i]+(delta&32768?-256:256))&65535));}
   check(groupWrites==1&&mux==unsigned(((group+4)&255)<=32)&&statusWrites==1+mux);
  }else check(!groupWrites&&!mux&&statusWrites==1);
  ++cases;
 };
 for(unsigned count=0;count<256;++count)for(unsigned irqReturn:{17u,23u})for(unsigned paired:{0u,3u})run(count,0x4000,0,false,0,irqReturn,paired);
 for(unsigned delta=0;delta<65536;++delta)for(unsigned base:{0u,255u,256u,32767u,32768u,65280u,65535u})for(bool armed:{false,true})run(0,base,delta,armed,delta&1?32:28,23,3);
 for(unsigned group:{0u,28u,32u,36u,124u,128u,252u,255u})for(unsigned delta:{0u,1u,511u,512u,2047u,2048u,32767u,32768u,65535u})run(0,0x4000,delta,true,group,23,3);
 std::cout<<"PASS: "<<cases<<" ADC IRQ policies / "<<intervals<<" intervals; all wrapping16 differences, two threshold states, four-channel order, settle counts, return modes, group wrap and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
