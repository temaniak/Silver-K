#include "ksVoiceStartTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::VoiceStartTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 auto verify=[&](T::Kind kind,unsigned flags,unsigned middle,unsigned low,unsigned detune,unsigned velocity,unsigned allocation){
  T task(kind);unsigned clocks=0,reads=0;std::vector<T::Child> actual,wanted;
  if(kind==T::Kind::Voice){wanted.push_back(T::Child::Allocation);if(velocity<128){wanted.push_back(T::Child::Tracking);if(allocation<=1){wanted.push_back(T::Child::Prime);wanted.push_back(T::Child::Parameters);wanted.push_back(T::Child::Gate);}}}
  else wanted.push_back(T::Child::Gate);
  const uint32_t word=kind==T::Kind::FullGate?uint32_t((flags&63)|((middle&3)<<6))<<16|((low&7)|0xb8|(detune==64?0:0x40)):
    uint32_t(flags|0xc0)<<16|(uint32_t(uint8_t(middle<<1))<<8)|(detune==64?0xbf:0xff);
  auto reader=[&](T::Read what)->uint32_t{++reads;switch(what){case T::Read::Velocity:check(task.slot()==13,"Allocated voice handle differs");return velocity;
   case T::Read::Allocation:return allocation;case T::Read::Flags:return flags;case T::Read::Middle:return middle;case T::Read::Low:return low;case T::Read::Detune:return detune;}return 0;};
  while(!task.complete()){
   const auto child=task.child();if(child!=T::Child::None){actual.push_back(child);if(child==T::Child::Gate)check(task.dspWord()==word&&task.gateOffset()==0x2b,"Gate word/offset differs");
    if(child==T::Child::Prime)check(task.primeOffset()==0x2c,"Prime offset differs");check(task.resumeChild(13),"Child reply rejected");continue;}
   const auto cost=task.nextClocks();const auto before=reads;check(cost>0,"Voice start stalled");check(!task.advanceBefore(cost-1,reader)&&reads==before,"Early start/packet read");check(task.advanceBefore(cost,reader)==cost,"Start deadline differs");clocks+=cost;
  }
  const bool suppressed=kind==T::Kind::Voice&&velocity>=128,handoff=kind==T::Kind::Voice&&velocity<128&&allocation>1;
  const unsigned gateClocks=(kind==T::Kind::FullGate?133:106)+(detune==64?0:3);
  check(actual==wanted&&task.suppressed()==suppressed&&task.handoff()==handoff,"Start child order/exit decision differs");
  check(clocks==(suppressed?60:handoff?81:kind==T::Kind::Voice?127+gateClocks:gateClocks),"Whole start/gate own clocks differ");check(!task.resumeChild(),"Stray start child accepted");++cases;
 };
 for(auto kind:{T::Kind::ResumeGate,T::Kind::FullGate}){
  for(unsigned flags=0;flags<256;++flags)for(unsigned middle=0;middle<256;++middle)for(unsigned detune:{0u,64u,192u,255u})verify(kind,flags,middle,flags^middle,detune,0,0);
  for(unsigned low=0;low<256;++low)for(unsigned detune=0;detune<256;++detune)verify(kind,255,255,low,detune,0,0);
 }
 for(unsigned velocity=0;velocity<256;++velocity)for(unsigned allocation=0;allocation<256;++allocation)verify(T::Kind::Voice,0x39,0x87,0xac,allocation,velocity,allocation);
 std::cout<<"PASS: "<<cases<<" native start/gate scenarios; complete velocity/allocation decisions, all flags/middle pairs and low/detune pairs, exact child order, packed packets and deadlines/own clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
