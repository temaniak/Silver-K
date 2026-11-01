#include "ksVoiceReleaseTask.h"
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>
using T=ks::VoiceReleaseTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
struct Voice {uint32_t age;uint8_t part,note,extra,flags,pending;};
using Event=std::tuple<unsigned,unsigned,uint32_t>;
int main(){try{std::mt19937 random(0x4b53524c);uint64_t cases=0;
 for(unsigned scenario=0;scenario<65536;++scenario){
  const auto mode=T::Mode(scenario%5);const unsigned count=scenario%17==0?256:scenario%3==0?1:14;const uint8_t part=uint8_t(random()),note=uint8_t(random());
  const uint16_t cut=uint16_t(random());const uint8_t pedal=uint8_t(scenario%3?random():0);const unsigned latency=scenario&1?3:0;
  std::array<Voice,256> actual{},expected{};for(unsigned v=0;v<count;++v){actual[v]={uint32_t(random()),uint8_t(random()%3?part:part+1),uint8_t(random()%3?note:note+1),uint8_t(random()%7==0),uint8_t(random()%3?0:4),uint8_t(random()%3?random():0)};if(random()%7==0)actual[v].age&=0xffff;}
  expected=actual;std::vector<Event> events,wanted;wanted.emplace_back(0,0,0);const bool whole=mode==T::Mode::Part||mode==T::Mode::FastPart||mode==T::Mode::ProvidedPart;
  for(unsigned v=0;v<count;++v){auto& voice=expected[v];bool release=(voice.age>>16)&&voice.part==part;
   if(mode==T::Mode::Note){release=release&&voice.note==note&&!voice.extra;if(release&&!(voice.flags&4)&&pedal){voice.pending=pedal;wanted.emplace_back(1,v,pedal);release=false;}}
   else if(mode==T::Mode::Pedal){release=release&&voice.pending;if(release){voice.pending=0;wanted.emplace_back(1,v,0);}}
   if(release){voice.age=(((voice.age>>16)&0x3fff)|1)<<16|0xffff;wanted.emplace_back(2,v,voice.age);wanted.emplace_back(3,v,0);
    if(whole&&(mode==T::Mode::FastPart||(mode==T::Mode::ProvidedPart&&cut)))wanted.emplace_back(4,v,0x100000);wanted.emplace_back(5,v,0);}
   wanted.emplace_back(0,0,uint8_t(v+1));
  }if(whole)wanted.emplace_back(6,0,0);
  T task(mode,note,cut);uint8_t selector=0;unsigned publications=0,iterations=0;
  auto reader=[&](T::Read what)->uint32_t{using R=T::Read;const auto& voice=actual[task.slot()];switch(what){case R::Part:return part;case R::Count:return uint8_t(count);case R::Age:return voice.age>>16;
   case R::Owner:return voice.part;case R::Note:return voice.note;case R::Extra:return voice.extra;case R::PatchReference:return 0;
   case R::Flags:return voice.flags;case R::Pedal:return pedal;case R::Pending:return voice.pending;case R::Index:return selector;}throw std::runtime_error("Unknown logical read");};
  auto writer=[&](T::Write what,uint32_t value){using W=T::Write;++publications;switch(what){case W::Index:selector=uint8_t(value);events.emplace_back(0,0,value);break;
   case W::Pending:actual[task.slot()].pending=uint8_t(value);events.emplace_back(1,task.slot(),value);break;case W::Age:actual[task.slot()].age=value;events.emplace_back(2,task.slot(),value);break;}};
  auto timing=[&](T::Write){return latency;};
  while(!task.complete()){
   check(++iterations<30000,"Release core stalled");const auto child=task.child();if(child!=T::Child::None){switch(child){
    case T::Child::Voice:check(task.resumeChild(selector),"Voice reply rejected");continue;
    case T::Child::Parameters:check(task.resumeChild(part),"Parameter reply rejected");continue;
    case T::Child::Select:events.emplace_back(3,task.slot(),0);break;case T::Child::Cut:events.emplace_back(4,task.slot(),0x100000);break;
    case T::Child::Gate:events.emplace_back(5,task.slot(),0);break;case T::Child::Clear:events.emplace_back(6,0,0);break;default:break;}
    check(task.resumeChild(),"Completion reply rejected");continue;
   }
   const unsigned cost=task.nextClocks(task.publication()?latency:3u);check(cost>0,"Native release has no next deadline");
   check(!task.advanceBefore(cost-1,reader,writer,timing),"Early release admission");const auto before=publications;
   check(task.advanceBefore(cost,reader,writer,timing)==cost,"Release admission cost differs");check(publications<=before+1,"Multiple publications per admission");
  }
  check(events==wanted,"Release/cut/gate/publication order or value differs");check(task.remaining()==0,"Remaining voices differ");
  for(unsigned v=0;v<count;++v){check(actual[v].age==expected[v].age&&actual[v].pending==expected[v].pending,"Release fields differ");}
  check(!task.resumeChild(),"Stray child reply accepted");++cases;
 }
 std::cout<<"PASS: "<<cases<<" independent native release scenarios; note/part/fast/provided/pedal policies, packed ages, arbitrary pedal/cut/part/note, count1/14/256, gate/cut/publication order and deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
