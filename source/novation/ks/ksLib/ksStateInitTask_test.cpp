#include "ksStateInitTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::StateInitTask;using W=T::Write;using R=T::Read;using C=T::Child;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
struct Event {W field;unsigned index,value;};
int main(){try{unsigned cases=0;
 auto verify=[&](unsigned clock,unsigned preferred,unsigned selected,unsigned mask,unsigned latency){
  T task;std::array<Event,59> expected{},actual{};unsigned out=0,used=0,child=0,clocks=0;
  auto add=[&](W f,unsigned value,unsigned index=0){expected[out++]={f,index,value};};
  for(unsigned i=0;i<40;++i)add(W::ResetCell,0,i);add(W::Default,5);add(W::ActiveGlobalPatch,0);add(W::Patch0,0);add(W::Patch1,0);add(W::Patch2,0);add(W::PreviewGate,0);add(W::PatchBias,64);add(W::Clock0,clock);add(W::Clock1,clock^65535);add(W::ActiveMask,mask);add(W::ReceiveMask,mask^255);add(W::Sentinel0,255);add(W::Sentinel1,255);add(W::SelectedMode,0);add(W::SelectedMode,1);add(W::SelectedMode,preferred);add(W::Sentinel0,255);add(W::Sentinel1,255);add(W::LastClock,uint16_t(clock+selected));check(out==59,"Reference publications differ");
  const std::array<C,24> children{C::PanelDefault,C::Timer0,C::Timer1,C::Timer2,C::Timer3,C::Timer4,C::Banks,C::Notes,C::Midi,C::PanelTimers,C::Pin,C::PanelValue,C::PanelState,C::GlobalBank,C::SingleMode,C::Single,C::MultiMode,C::Multi,C::PreferredMode,selected?C::PreferredMulti:C::PreferredSingle,C::Configuration,C::Display,C::Clock,C::Hardware};
  auto read=[&](R f)->unsigned{switch(f){case R::HardwarePin:return mask;case R::Clock0High:return clock>>8;case R::Clock0Low:return clock&255;case R::Clock1High:return (clock^65535)>>8;case R::Clock1Low:return (clock^65535)&255;case R::ActiveMask:return mask;case R::ReceiveMask:return mask^255;case R::PreferredMode:return preferred;case R::SelectedMode:return selected;case R::CurrentClock:return uint16_t(clock+selected);}return 0;};
  auto write=[&](W f,unsigned index,unsigned value){check(used<actual.size(),"Excess publication");actual[used++]={f,f==W::ResetCell?index:0,value};};auto timing=[&](W,unsigned){return latency;};
  while(!task.complete()){
   if(task.awaiting()){check(child<24&&task.child()==children[child++],"Child service order differs");check(task.resumeChild(),"Reply refused");continue;}
   const auto cost=task.nextClocks(task.publication()?latency:3u);check(cost>0,"Stalled task");auto before=used;check(!task.advanceBefore(cost-1,read,write,timing),"Early admission");check(used==before,"Early publication");check(task.advanceBefore(cost,read,write,timing)==cost,"Wrong admission");check(used<=before+1,"Multiple publication");clocks+=cost;
  }
  check(used==59&&child==24&&task.position()==40&&task.remaining()==0,"Initialization lifetime differs");
  for(unsigned i=0;i<59;++i)check(actual[i].field==expected[i].field&&actual[i].index==expected[i].index&&actual[i].value==expected[i].value,"Initialization ordered data differs");
  check(clocks==(selected?1347u:1350u)+55*latency,"Whole own schedule differs");check(!task.resumeChild(),"Stray reply accepted");++cases;
 };
 for(unsigned clock=0;clock<65536;++clock)for(unsigned selected:{0u,1u,255u})for(unsigned latency:{0u,3u})verify(clock,clock&255,selected,clock&255,latency);
 for(unsigned preferred=0;preferred<256;++preferred)for(unsigned mask=0;mask<256;mask+=17)for(unsigned latency:{0u,3u})verify(0xabcd,preferred,(preferred+1)&255,mask,latency);
 for(unsigned mask=0;mask<256;++mask)for(unsigned preferred:{0u,1u,255u})for(unsigned latency:{0u,3u})verify(0xffff,preferred,preferred,mask,latency);
 std::cout<<"PASS: "<<cases<<" native initialization clock/late-mode/patch/mask/store-delay scenarios, ordered59 publications/24 children, exact own clocks and deadline admission\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
