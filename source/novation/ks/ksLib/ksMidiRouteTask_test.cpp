#include "ksMidiRouteTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>
using T=ks::MidiRouteTask;using S=T::Stage;using F=T::Flavor;using R=T::Read;using W=T::Write;
using Event=std::tuple<unsigned,unsigned,uint32_t>;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 auto verify=[&](unsigned status,unsigned channel,unsigned initialMask,unsigned matchMask,F flavor,unsigned pattern,unsigned seed,unsigned latency){
  const unsigned initialMode=pattern==0||pattern==2?0:pattern==5?255:1,exitAfter=pattern==3?1:pattern==4?2:99,finalMode=pattern==0?0:1;
  T task;unsigned reads=0,clocks=0,selected=0;std::vector<Event> events;
  const auto controlBinding=[&](unsigned part){return uint8_t(part*17+seed);};const auto parameterBinding=[&](unsigned part){return uint8_t(part*23+seed);};
  std::array<unsigned,256> channels;channels.fill(uint8_t(channel+1));for(unsigned p=0;p<5;++p)channels[controlBinding(p)]=(matchMask&(1u<<p))?channel:uint8_t(channel+1);
  while(!task.complete()&&!task.handoff()){
   if(task.awaitingControls()){events.emplace_back(4,selected,0);check(task.resumeControls(controlBinding(selected)),"Route controls reply rejected");continue;}
   if(task.awaitingPart()){events.emplace_back(5,selected,0);check(task.resumePart(parameterBinding(selected)),"Route parameter reply rejected");continue;}
   if(task.awaitingChild()){events.emplace_back(task.stage()==S::AwaitHandler?6:task.stage()==S::AwaitProgram?7:8,selected,0);check(task.resumeChild(),"Route handler reply rejected");continue;}
   auto read=[&](R field)->unsigned{++reads;switch(field){case R::Status:return status;case R::Flavor:return unsigned(flavor);case R::Channel:return channel;case R::Mask:return initialMask;case R::Mode:return initialMode;case R::ChangedMode:return task.index()-1>=exitAfter?0:initialMode;case R::FinalMode:return finalMode;case R::PartChannel:return channels[task.controlPart()];}return 0;};
   auto write=[&](W field,uint32_t data){switch(field){case W::SelectedPart:selected=data;events.emplace_back(0,selected,0);break;case W::Patch:events.emplace_back(1,task.controlPart(),data);break;case W::Mirror:events.emplace_back(2,0,data);break;case W::UnusedBank:events.emplace_back(3,task.parameterPart(),data);break;}};auto timing=[&](W){return latency;};
   const auto cost=task.nextClocks(task.publication()?latency:3);check(cost>0,"Route task stalled");const auto before=reads;const auto beforeEvents=events.size();
   check(!task.advanceBefore(cost-1,read,write,timing)&&reads==before&&events.size()==beforeEvents,"Early routing access");check(task.advanceBefore(cost,read,write,timing)==cost,"Routing admission differs");clocks+=cost;
  }
  std::vector<Event> expected;unsigned duration=0,bytePubs=0,index=initialMode?1:0,mask=initialMask,remaining=4;bool cleanup=false;
  auto choose=[&](unsigned p){expected.emplace_back(0,p,0);++bytePubs;};auto bindControl=[&](unsigned p){expected.emplace_back(4,p,0);expected.emplace_back(1,controlBinding(p),0x168fc+p*256);};
  auto clearBank=[&](unsigned p){choose(p);expected.emplace_back(5,p,0);expected.emplace_back(3,parameterBinding(p),255);++bytePubs;};
  if(status>=240){duration=21;check(task.handoff(),"System message did not hand off");}
  else{
   duration=75+(initialMode?12:0);
   for(unsigned processed=1;processed<=4;++processed){
    choose(index);bindControl(index);duration+=59;const bool enabled=mask&1;mask>>=1;
    if(enabled){duration+=21;if(matchMask&(1u<<index)){mask|=8;expected.emplace_back(6,index,0);duration+=204;}}
    ++index;duration+=18;
    if(initialMode){duration+=15;if(processed>=exitAfter){cleanup=true;break;}}
    --remaining;duration+=6;
   }
   if(cleanup){duration+=73+62*(remaining-1);while(--remaining){clearBank(index++);mask>>=1;}clearBank(0);}
   else{
    duration+=15;
    if(!finalMode)duration+=17;
    else{
     choose(0);bindControl(0);duration+=86;
     if(!(matchMask&1))duration+=17;
     else{
      duration+=23;
      if(flavor==F::Program){expected.emplace_back(7,0,0);duration+=51;}
      else if(flavor==F::Control){expected.emplace_back(2,0,1);expected.emplace_back(8,0,0);expected.emplace_back(2,0,0);bytePubs+=2;duration+=90;}
      else duration+=40;
     }
    }
   }
   check(task.complete()&&task.mask()==mask&&task.remaining()==0,"Routing mask/remaining completion differs");if(cleanup)check(task.channel()==255,"Mode exit did not invalidate channel");
  }
  if(events!=expected){std::cerr<<"status="<<status<<" channel="<<channel<<" mask="<<initialMask<<" matches="<<matchMask<<" flavor="<<unsigned(flavor)<<" pattern="<<pattern<<'\n';throw std::runtime_error("Native routing publications/semantic child order differ");}
  check(clocks==duration-bytePubs*(3-latency),"Whole routing own schedule differs");check(!task.resumeControls(0)&&!task.resumePart(0)&&!task.resumeChild(),"Spurious routing reply accepted");++cases;
 };
 for(unsigned mask=0;mask<256;++mask)for(unsigned matches=0;matches<32;++matches)for(auto flavor:{F::Other,F::Program,F::Control})for(unsigned pattern=0;pattern<6;++pattern)for(unsigned latency:{0u,3u})verify(0xb0,uint8_t(mask*17),mask,matches,flavor,pattern,uint8_t(mask+matches),latency);
 for(unsigned status=0;status<256;++status)verify(status,status,15,31,F::Control,1,status,3);
 std::cout<<"PASS: "<<cases<<" native MIDI channel/mask/mode/handler-flavor properties; independent four-part iteration/activity masks, selected versus bound part views, fallback program/control mirror, unused-bank cleanup, semantic child/publication order, complete own schedules/deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
