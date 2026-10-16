#include "ksPedalTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::PedalTask;using S=T::Stage;using R=T::Read;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 auto verify=[&](bool checked,unsigned controller,unsigned value,unsigned previous,unsigned flags,unsigned latency){
  T task(checked);unsigned clocks=0,reads=0;std::vector<unsigned> writes,children;
  while(!task.complete()&&!task.handoff()){
   if(task.awaitingPart()){check(task.resumePart(uint8_t(value)),"Sustain part reply rejected");children.push_back(0x4000);continue;}
   if(task.awaitingControls()){check(task.patchPart()==uint8_t(value+1),"Patch part not captured");check(task.resumeControls(31),"Sustain controls reply rejected");children.push_back(0xa670);continue;}
   if(task.awaitingRelease()){check(task.resumeRelease(),"Sustain release reply rejected");children.push_back(0x9400);continue;}
   auto read=[&](R field)->uint32_t{++reads;switch(field){case R::Controller:return controller;case R::Value:return value;case R::Previous:check(task.part()==value,"Sustain part lost");return previous;
    case R::PatchPart:return uint8_t(value+1);case R::PatchReference:check(task.controlPart()==31,"Patch controls binding lost");return 0xffa30000u;case R::Flags:check(task.patchReference()==0xffa30000u,"Patch reference lost");return flags;}return 0;};
   auto write=[&](unsigned offset,unsigned data){check(data==unsigned(value<64?0:16),"Wrong sustain state");writes.push_back(offset);};auto timing=[&](unsigned){return latency;};
   const auto cost=task.nextClocks(task.publication()?latency:3);check(cost>0,"Sustain task stalled");const auto before=reads;const auto beforeWrites=writes.size();
   check(!task.advanceBefore(cost-1,read,write,timing)&&reads==before&&writes.size()==beforeWrites,"Early sustain access");check(task.advanceBefore(cost,read,write,timing)==cost,"Sustain deadline differs");clocks+=cost;
  }
  if(checked&&controller!=64){check(task.handoff()&&clocks==6&&writes.empty()&&children.empty(),"Non-sustain handoff differs");++cases;return;}
  const unsigned desired=value<64?0:16,changed=desired^previous,released=previous&changed;const bool release=!(flags&4)&&(released&16);
  std::vector<unsigned> expectedChildren{0x4000,0xa670};if(release)expectedChildren.push_back(0x9400);
  check(task.complete()&&writes==std::vector<unsigned>{309,310}&&children==expectedChildren,"Sustain publication/child order differs");
  check(task.previous()==previous&&task.desired()==desired,"Raw previous state normalized early");
  unsigned duration=196+(checked?6:0)+(value>=64?3:0)-2*(3-latency);if(!(flags&4)){duration+=6;if(changed)duration+=9;if(release)duration+=20;}
  check(clocks==duration,"Whole sustain schedule differs");check(!task.resumePart(0)&&!task.resumeControls(0)&&!task.resumeRelease(),"Spurious sustain reply accepted");++cases;
 };
 for(bool checked:{false,true})for(unsigned value=0;value<256;++value)for(unsigned previous=0;previous<256;++previous)for(unsigned flags:{0u,4u})for(unsigned latency:{0u,3u})verify(checked,64,value,previous,flags,latency);
 for(unsigned flags=0;flags<256;++flags)for(unsigned value=0;value<256;value+=17)for(unsigned previous=0;previous<256;previous+=17)verify(true,64,value,previous,flags,3);
 for(unsigned controller=0;controller<256;++controller)verify(true,controller,0,16,0,3);
 std::cout<<"PASS: "<<cases<<" native sustain full-byte value/previous/bypass/controller properties; raw XOR/intersection falling edge, separate selected/patch parts, exact publications/child order/schedules/deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
