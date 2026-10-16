#include "ksGlideTask.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>
using T=ks::GlideTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
using Event=std::tuple<unsigned,unsigned,uint32_t>;
uint32_t coefficient(uint16_t code){return uint32_t(code)*0x8badf00du+0x31415927u;}
int main(){try{std::mt19937 random(0x4b53474c);uint64_t cases=0;
 for(unsigned scenario=0;scenario<262144;++scenario){
  const bool relative=bool(scenario&1);const uint8_t patchPitch=uint8_t(scenario%3?random():0),slotPitch=uint8_t(random()),detune=uint8_t(scenario%5?random():64);
  const uint8_t time=uint8_t(scenario%7?random():0),mode=uint8_t(random()),allocation=uint8_t(scenario%3?random():scenario%5),gate=uint8_t(scenario%2?random():scenario%5),distanceMode=uint8_t(scenario%3),initialDistance=uint8_t(random()),initialPrevious=uint8_t(random());
  const uint16_t base=uint16_t(random());const unsigned latency=scenario&2?3:0;uint8_t distance=initialDistance,previous=initialPrevious;
  std::vector<Event> actual,wanted;const uint8_t pitch=uint8_t((patchPitch?patchPitch:slotPitch)-60);wanted.emplace_back(2,0x8c,uint32_t(pitch)<<16);
  const uint8_t delta=uint8_t(detune-64);uint8_t wantedDistance=distance,wantedPrevious=previous;
  if(delta){wantedDistance=delta&128?uint8_t(0-delta):delta;wantedPrevious=uint8_t(slotPitch+delta);wanted.emplace_back(0,13,wantedDistance);wanted.emplace_back(1,0,wantedPrevious);}
  const uint8_t effectiveTime=((mode>>3)&3)==1&&(allocation==1||allocation==2)?0:time;
  if(!effectiveTime){wanted.emplace_back(2,0x8d,0);wanted.emplace_back(2,0x8e,0x7fffff);}
  else{const unsigned inverted=effectiveTime^127;const uint16_t code=uint16_t(inverted*93+(inverted>118?(inverted-118)*256:0)+base);const uint32_t value=coefficient(code);wanted.emplace_back(3,0,code);
   if(distanceMode&&wantedDistance){const uint32_t packed=std::max(uint32_t((uint64_t(value)*(uint32_t(wantedDistance)<<8))>>16),uint32_t(512));wanted.emplace_back(2,0x8d,packed>>8);wanted.emplace_back(2,0x8e,0);}
   else{wanted.emplace_back(2,0x8d,value>>13);wanted.emplace_back(2,0x8e,value>>8);}}
  wanted.emplace_back(2,0x8f,uint32_t(uint8_t(wantedPrevious-60))<<16);
  T task(relative);unsigned reads=0,stages=0;
  auto reader=[&](T::Read what)->uint32_t{++reads;switch(what){case T::Read::PatchPitch:return patchPitch;case T::Read::SlotPitch:case T::Read::Transposed:return slotPitch;
   case T::Read::Detune:return detune;case T::Read::Time:return time;case T::Read::Mode:return mode;case T::Read::Allocation:return task.stage()==T::Stage::ReadGateAllocation?gate:allocation;
   case T::Read::Base:return base;case T::Read::DistanceMode:return distanceMode;case T::Read::Distance:return distance;case T::Read::Previous:return previous;}return 0;};
  auto writer=[&](T::Write what,uint32_t value){if(what==T::Write::Distance){distance=uint8_t(value);actual.emplace_back(0,task.slot(),value);}else{previous=uint8_t(value);actual.emplace_back(1,0,value);}};
  auto timing=[&](T::Write){return latency;};
  while(!task.complete()){check(++stages<1024,"Glide stalled");const auto child=task.child();if(child!=T::Child::None){uint32_t result=0;
    if(child==T::Child::PitchVoice||child==T::Child::DetuneVoice||child==T::Child::DistanceVoice)result=13;
    else if(child==T::Child::Coefficient){actual.emplace_back(3,0,task.rate());result=coefficient(task.rate());}
    else{check(task.dspWord()<=0xffffff,"Glide payload exceeds 24 bits");actual.emplace_back(2,task.offset(),task.dspWord());}
    check(task.resumeChild(result),"Glide child result rejected");continue;
   }
   const auto cost=task.nextClocks(task.publication()?latency:3u);const auto before=reads;const auto events=actual.size();check(cost>0,"No glide deadline");
   check(!task.advanceBefore(cost-1,reader,writer,timing)&&reads==before&&actual.size()==events,"Early glide observation/publication");check(task.advanceBefore(cost,reader,writer,timing)==cost,"Glide admission differs");
  }
  check(actual==wanted,"Glide field/coefficient/packet order/value differs");check(distance==wantedDistance&&previous==wantedPrevious,"Glide mutable state differs");
  if(relative)check(task.resumeGate()==(gate<=2),"Late gate allocation not observed");check(!task.resumeChild(),"Stray glide completion accepted");++cases;
 }
 std::cout<<"PASS: "<<cases<<" native glide scenarios; pitch/detune/previous memory, legato/time code, distance clamp/split products, balanced coefficient, independent late gate allocation, packets and deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
