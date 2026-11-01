#include "ksScheduledTimerTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::ScheduledTimerTask;using F=T::Field;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{unsigned cases=0;
 for(unsigned slot=0;slot<5;++slot)for(auto kind:{T::Kind::Cancel,T::Kind::Arm,T::Kind::Service})for(unsigned interval:{0u,1u,127u,128u,32767u,32768u,65535u})for(unsigned now=0;now<65536;now+=17){
 const unsigned began=0xfff0;T task(kind,slot);std::array<uint32_t,3> fields{interval,began,0x12345678};unsigned clocks=0,reads=0,writes=0,clockCalls=0,actionCalls=0;
 auto read=[&](F f,bool argument){++reads;return argument?(f==F::Action?0x5b004000u:interval):fields[unsigned(f)];};
 auto write=[&](F f,uint32_t value){++writes;fields[unsigned(f)]=value;};
 while(!task.complete()){
  if(task.awaitingClock()){++clockCalls;check(task.resumeClock(uint16_t(now)),"Clock reply rejected");continue;}
  if(task.awaitingAction()){++actionCalls;check(task.action().value==0x12345678,"Action identity differs");check(task.resumeAction(),"Action reply rejected");continue;}
  const auto cost=task.nextClocks();check(cost>0,"Stalled task");unsigned oldReads=reads,oldWrites=writes;check(!task.advanceBefore(cost-1,read,write),"Early admission");check(reads==oldReads&&writes==oldWrites,"Early effect");check(task.advanceBefore(cost,read,write)==cost,"Wrong budget");clocks+=cost;
 }
 const bool expires=interval&&uint16_t(now-began)>=interval;
 if(kind==T::Kind::Cancel)check(fields[0]==0&&fields[1]==began&&fields[2]==0x12345678&&clocks==32&&!clockCalls&&!actionCalls,"Cancel differs");
 else if(kind==T::Kind::Arm)check(fields[0]==interval&&fields[1]==now&&fields[2]==0x5b004000&&clocks==79&&clockCalls==1&&!actionCalls,"Arm differs");
 else {check(fields[0]==(expires?0:interval)&&fields[1]==began&&fields[2]==0x12345678,"Service publication differs");check(clockCalls==unsigned(interval!=0)&&actionCalls==unsigned(expires),"Service child admission differs");check(clocks==unsigned(!interval?54:expires?183:135),"Service clocks differ");}
 check(!task.resumeAction()&&!task.resumeClock(0),"Stray child accepted");++cases;
 }
 std::cout<<"PASS: "<<cases<<" native timer operation/slot/deadline-wrap scenarios, exact actions, modular admission and own clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
