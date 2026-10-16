#include "ksUnisonTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>
using T=ks::UnisonTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 for(unsigned mode=0;mode<256;++mode)for(unsigned initial=0;initial<256;++initial)for(unsigned latency:{0u,3u}){
  T task;unsigned children=0,clocks=0;uint8_t unison=uint8_t(initial);std::vector<std::tuple<T::Write,unsigned,unsigned>> writes;
  auto read=[&](T::Read what)->uint32_t{switch(what){case T::Read::Note:return 0xff;case T::Read::Velocity:return 0x80;
   case T::Read::PatchReference:check(task.part()==2,"Initial patch binding differs");return 0;case T::Read::Mode:return mode;case T::Read::Unison:return unison;}return 0;};
  auto write=[&](T::Write what,uint32_t value){writes.emplace_back(what,task.part(),value);if(what==T::Write::Unison){check(task.part()==3,"Second live part binding differs");unison=uint8_t(value);}};
  auto timing=[&](T::Write){return latency;};
  while(!task.complete()){
   const auto child=task.child();if(child!=T::Child::None){if(child==T::Child::Single){++children;check(task.resumeChild(),"Single completion rejected");}
    else check(task.resumeChild(child==T::Child::Part?2:3),"Part reply rejected");continue;}
   const auto cost=task.nextClocks(task.publication()?latency:3u);check(cost>0,"Unison stalled");check(!task.advanceBefore(cost-1,read,write,timing),"Early unison admission");check(task.advanceBefore(cost,read,write,timing)==cost,"Unison cost differs");clocks+=cost;
  }
  const unsigned count=mode&7;check(children==count,"Extra-voice count differs");check(task.note()==255&&task.velocity()==128,"MIDI byte retention differs");
  check(writes.size()==2+(count?count+1:0),"Publication count differs");check(std::get<0>(writes[0])==T::Write::Note&&std::get<2>(writes[0])==255&&std::get<0>(writes[1])==T::Write::Velocity&&std::get<2>(writes[1])==128,"Input order differs");
  if(count){check(std::get<2>(writes[2])==((initial&0xf8)|count),"Upper bits/count merge differs");for(unsigned i=0;i<count;++i)check(std::get<2>(writes[3+i])==((initial&0xf8)|count)-i-1,"Full-byte descending count differs");check(unison==(initial&0xf8),"Final unison byte differs");}
  else check(unison==initial,"Zero extra voices changed part state");
  check(clocks==86-(3-latency)*2+(count?56-(3-latency):0)+count*(94-(3-latency)),"Original own schedule differs");check(!task.resumeChild(),"Stray reply accepted");++cases;
 }
 // The field can change during the voice child. The late full-byte reread,
 // including borrow out of the low three bits, determines further dispatch.
 for(unsigned changed=0;changed<256;++changed){T task;unsigned children=0;uint8_t unison=1;
  auto read=[&](T::Read what)->uint32_t{if(what==T::Read::Mode)return 1;if(what==T::Read::Unison)return unison;return 0;};
  auto write=[&](T::Write what,uint32_t value){if(what==T::Write::Unison)unison=uint8_t(value);};auto timing=[](T::Write){return 3u;};
  while(!task.complete()){auto child=task.child();if(child!=T::Child::None){if(child==T::Child::Single&&++children==1)unison=uint8_t(changed);task.resumeChild();}else{auto cost=task.nextClocks();check(task.advanceBefore(cost,read,write,timing)==cost,"Live count admission differs");}check(children<=8,"Live count failed to terminate");}
  const unsigned expected=changed&7;check(children==(expected?expected:8),"Live unison/borrow ignored");++cases;
 }
 std::cout<<"PASS: "<<cases<<" unison mode/upper-byte/latency/live-change cases; exact extra count, final tail, publication/deadline/order/clock rules\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
