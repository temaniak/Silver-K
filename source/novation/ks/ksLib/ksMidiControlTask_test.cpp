#include "ksMidiControlTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::MidiControlTask;using S=T::Stage;using R=T::Read;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 auto verify=[&](unsigned kind,unsigned first,unsigned second,unsigned remap,unsigned latency){
  T task(kind==0?T::Kind::Pitch:kind==1?T::Kind::Pressure:T::Kind::Control);unsigned clocks=0,reads=0;std::vector<unsigned> children,writes;
  while(!task.complete()&&!task.handoff()){
   if(task.awaitingPart()){check(task.resumePart(31),"Bank part reply rejected");children.push_back(0x4000);continue;}
   if(task.awaitingChild()){children.push_back(task.stage()==S::AwaitPitch?0x9e42:task.stage()==S::AwaitPressure?0x9ea2:task.stage()==S::AwaitSound?0x9484:task.stage()==S::AwaitRelease?0x9374:0x3074);check(task.resumeChild(),"Control child reply rejected");continue;}
   auto read=[&](R field)->unsigned{++reads;switch(field){case R::EventFirst:return first;case R::EventSecond:case R::Value:return second;default:return remap;}};
   auto write=[&](unsigned offset,unsigned data){check(task.part()==31&&offset==331&&data==second,"Bank publication differs");writes.push_back(offset);};auto timing=[&](unsigned offset){check(offset==331,"Unexpected control publication");return latency;};
   const auto cost=task.nextClocks(task.publication()?latency:3);check(cost>0,"Control task stalled");const auto before=reads;const auto beforeWrites=writes.size();
   check(!task.advanceBefore(cost-1,read,write,timing)&&reads==before&&writes.size()==beforeWrites,"Early control access");check(task.advanceBefore(cost,read,write,timing)==cost,"Control admission differs");clocks+=cost;
  }
  std::vector<unsigned> expected;unsigned duration=0;
  if(kind==0){const unsigned wanted=uint8_t((second<<1)|((first>>6)&1));check(task.argument()==wanted,"Pitch reduction arithmetic differs");expected={0x9e42};duration=67;}
  else if(kind==1){check(task.argument()==first,"Pressure byte changed");expected={0x9ea2};duration=46;}
  else{
   const unsigned cc=first==11?remap:first;duration=first==11?15:0;
   switch(cc){
    case 1:duration+=38;check(task.stage()==S::HandoffWheel&&task.argument()==second,"Wheel tail differs");break;
    case 2:duration+=44;check(task.stage()==S::HandoffBreath&&task.argument()==second,"Breath tail differs");break;
    case 32:duration+=94-(3-latency);expected={0x4000};check(writes==std::vector<unsigned>{331},"Bank field omitted");break;
    case 120:duration+=68+(second?0:20);if(!second)expected={0x9484};break;case 121:duration+=68;break;
    case 123:case 124:case 125:duration+=80+(cc-123)*6+(second?0:40);if(!second)expected={0x9374,0x3074};break;
    case 126:duration+=132;expected={0x9374,0x3074};break;case 127:duration+=104+(second?0:40);if(!second)expected={0x9374,0x3074};break;
    default:duration+=81;check(task.stage()==S::HandoffPedal,"Unconsumed controller lost sustain handoff");break;
   }
   if(cc!=32)check(writes.empty(),"Unexpected controller publication");
  }
  check(children==expected,"MIDI control child order differs");check(clocks==duration,"Whole MIDI control own schedule differs");check(!task.resumePart(0)&&!task.resumeChild(),"Spurious control reply accepted");++cases;
 };
 for(unsigned first=0;first<256;++first)for(unsigned second=0;second<256;++second){verify(0,first,second,0,3);verify(2,first,second,64,3);}
 for(unsigned remap=0;remap<256;++remap)for(unsigned second=0;second<256;++second)for(unsigned latency:{0u,3u})verify(2,11,second,remap,latency);
 for(unsigned value=0;value<256;++value)verify(1,value,0,0,3);
 std::cout<<"PASS: "<<cases<<" native full-byte pitch/CC/remap/pressure properties; independent pitch arithmetic, exact remap/bank/value-gated all-note/mono decisions, child and tail order, complete own schedules/deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
