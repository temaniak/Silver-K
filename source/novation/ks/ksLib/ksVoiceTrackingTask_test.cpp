#include "ksVoiceTrackingTask.h"
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>
using T=ks::VoiceTrackingTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
using Event=std::tuple<unsigned,unsigned,uint32_t>;
uint32_t coefficient(uint16_t rate){return uint32_t(rate)*0x31415927u+0x8badf00du;}
int main(){try{std::mt19937 random(0x4b535654);uint64_t cases=0;
 for(unsigned scenario=0;scenario<131072;++scenario){
  const auto kind=T::Kind(scenario%3);const bool editable=bool(scenario&4);const uint8_t part=uint8_t(random()),active=uint8_t(scenario%4?part:random());
  const uint8_t voice=uint8_t(random()),tracked=uint8_t(scenario%5?voice:random()),aux=uint8_t(random());const uint16_t base=uint16_t(random());
  std::array<uint8_t,2> sample{uint8_t(scenario%3?random():0),uint8_t(scenario%5?random():0)},time{uint8_t(random()),uint8_t(random())};
  std::array<uint32_t,64> table{};for(auto& v:table)v=uint32_t(random());const unsigned latency=scenario&1?3:0;
  std::vector<Event> actual,wanted;T task(kind,editable);
  if(kind==T::Kind::Observe){if(!editable)wanted.emplace_back(0,0,255);else if(part==active){wanted.emplace_back(0,0,voice);wanted.emplace_back(3,0x8046,voice);}else if(voice==tracked)wanted.emplace_back(0,0,255);}
  else if(kind==T::Kind::Auxiliary)wanted.emplace_back(3,0x8045,aux);
  else if(part==active&&tracked<128){wanted.emplace_back(1,0,tracked);wanted.emplace_back(2,0,0);
   for(unsigned effect=0;effect<2;++effect){const unsigned offset=0x7e + effect*8;
    if(sample[effect]){wanted.emplace_back(4,offset,0);wanted.emplace_back(4,offset+1,table[uint8_t((sample[effect]-1)*4)/4]>>8);}
    else{const uint16_t rate=uint16_t(((uint32_t(time[effect])<<7)*0xaaab>>16)+0x4700 + base-0x4470);wanted.emplace_back(5,0,rate);
     const uint32_t scale=uint32_t(time[effect]>32?32:time[effect])*1024;const uint32_t packed=uint32_t((uint64_t(coefficient(rate))*scale)>>16)<<1;
     wanted.emplace_back(4,offset,packed>>8);wanted.emplace_back(4,offset+1,0);}
   }
  }
  auto reader=[&](T::Read what)->uint32_t{switch(what){case T::Read::Part:return part;case T::Read::ActivePart:return active;case T::Read::Voice:return voice;case T::Read::Tracked:return tracked;
   case T::Read::Auxiliary:return aux;case T::Read::PatchReference:return 0;case T::Read::Sample:return sample[task.effect()];case T::Read::Time:return time[task.effect()];case T::Read::Base:return base;case T::Read::Table:return table[task.sampleIndex()/4];}return 0;};
  auto writer=[&](T::Write what,uint32_t value){actual.emplace_back(what==T::Write::Tracked?0:1,0,value);};auto timing=[&](T::Write){return latency;};unsigned stages=0;
  while(!task.complete()){check(++stages<512,"Tracking stalled");const auto child=task.child();if(child!=T::Child::None){uint32_t result=0;
    switch(child){case T::Child::Absolute:actual.emplace_back(3,task.absoluteOffset(),task.absoluteValue());break;case T::Child::Select:actual.emplace_back(2,0,0);break;
    case T::Child::First:actual.emplace_back(4,task.firstOffset(),task.packed()>>8);break;case T::Child::Second:actual.emplace_back(4,task.secondOffset(),task.packed()>>8);break;
    case T::Child::Coefficient:actual.emplace_back(5,0,task.rate());result=coefficient(task.rate());break;default:break;}
    check(task.resumeChild(result),"Child result rejected");continue;
   }
   const auto cost=task.nextClocks(task.publication()?latency:3u);check(cost>0,"No tracking deadline");check(!task.advanceBefore(cost-1,reader,writer,timing),"Early admission");
   check(task.advanceBefore(cost,reader,writer,timing)==cost,"Tracking cost differs");
  }
  check(actual==wanted,"Tracking or paired envelope messages differ");check(!task.resumeChild(),"Stray child accepted");++cases;
 }
 std::cout<<"PASS: "<<cases<<" native tracking/aux/paired-envelope scenarios; eligibility, active/tracked voices, sample index wrapping, coefficient codes/scaling and ordered packets/deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
