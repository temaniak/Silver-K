#include "ksDspStartupTasks.h"
#include <array>
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
 unsigned resetCases=0;
 for(unsigned seed=0;seed<256;++seed){
  ks::DspResetTask task;unsigned reads=0,writes=0,boundaries=0;uint64_t elapsed=0;std::array<unsigned,2> values{};std::array<uint64_t,2> times{};
  while(!task.complete()){
   auto read=[&]{++reads;return seed;};auto write=[&](uint8_t value){check(writes<2,"Too many reset writes");values[writes]=value;times[writes++]=elapsed;};
   const auto cost=task.nextClocks();auto shortStep=task.advanceBefore(cost-1,read,write);
   check(!shortStep.clocks&&!shortStep.boundaries,"Reset crossed deadline");
   const auto step=task.advanceBefore(cost,read,write);check(step.clocks==cost&&step.boundaries==1,"Reset step duration differs");elapsed+=step.clocks;++boundaries;
  }
  check(reads==1&&writes==2&&values[0]==(seed&~8u)&&values[1]==(seed|8u),"Reset latching or bit preservation differs");
  check(times[1]-times[0]==160155&&elapsed==320336&&task.elapsedClocks()==elapsed&&boundaries==106633,"Reset settling schedule differs");++resetCases;
 }
 for(auto port:{ks::DspPort::A,ks::DspPort::B}){
  ks::DspSelectTask task(port);unsigned writes=0,elapsed=0;
  auto publish=[&](ks::DspPort p,uint16_t invalid){check(p==port&&invalid==0xffff,"Selector publication differs");++writes;};
  while(task.nextClocks()){const auto cost=task.nextClocks();check(!task.advanceBefore(cost-1,publish),"Selector crossed deadline");check(task.advanceBefore(cost,publish)==cost,"Selector clocks differ");elapsed+=cost;}
  check(writes==1&&elapsed==27,"Selector count/duration differs");
 }
 ks::DspStartupTask task;std::array<ks::DspStartupTask::Request,4> expected{{ks::DspStartupTask::Request::Reset,ks::DspStartupTask::Request::SetupA,ks::DspStartupTask::Request::SetupB,ks::DspStartupTask::Request::SelectA}};
 unsigned children=0,elapsed=0;
 while(task.stage()!=ks::DspStartupTask::Stage::Complete){
  if(task.request()!=ks::DspStartupTask::Request::None){check(children<4&&task.request()==expected[children++],"Startup child order differs");check(!task.nextClocks()&&!task.advanceBefore(100000),"Startup advanced pending child");check(task.resumeChild(),"Startup child release differs");}
  else {const auto cost=task.nextClocks();check(cost&&!task.advanceBefore(cost-1),"Startup crossed deadline");check(task.advanceBefore(cost),"Startup did not advance");elapsed+=cost;}
 }
 check(children==4&&elapsed==106&&!task.resumeChild(),"Startup request/duration differs");
 std::cout<<"PASS: "<<resetCases<<" control values, exact settling deadlines/latched bits; both selector ports and complete child orchestration; no CPU state\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
