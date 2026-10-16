#include "ksVoiceSelectionTask.h"
#include <random>
#include <array>
#include <iostream>
#include <stdexcept>
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct Case {unsigned mode,held,shape,voices,selected,allocation,clocks,steps,own;};
const Case cases[]={
{0,0,0,14,0,1,3042,352,2098},
{0,0,1,14,0,4,508,54,331},
{0,0,2,14,13,2,3267,384,2323},
{0,0,3,14,13,1,3252,380,2308},
{0,0,4,14,13,1,4259,452,2127},
{0,0,5,14,0,4,508,54,331},
{0,0,6,14,13,3,2902,320,1958},
{0,0,7,14,0,4,508,54,331},
{0,0,8,256,255,2,52635,6192,37413},
{0,1,0,14,0,1,335,36,217},
{0,1,1,14,0,1,3704,396,1572},
{0,1,2,14,0,1,3704,396,1572},
{0,1,3,14,0,1,335,36,217},
{0,1,4,14,13,1,3899,422,1767},
{0,1,5,14,0,1,3689,394,1557},
{0,1,6,14,0,1,3704,396,1572},
{0,1,7,14,0,1,3704,396,1572},
{0,1,8,256,0,1,40004,4510,23594},
{1,0,0,14,0,1,3042,352,2098},
{1,0,1,14,0,4,508,54,331},
{1,0,2,14,13,2,3267,384,2323},
{1,0,3,14,13,1,3252,380,2308},
{1,0,4,14,13,1,4259,452,2127},
{1,0,5,14,0,4,508,54,331},
{1,0,6,14,13,3,2902,320,1958},
{1,0,7,14,0,4,508,54,331},
{1,0,8,256,255,2,52635,6192,37413},
{1,1,0,14,0,1,335,36,217},
{1,1,1,14,0,1,3704,396,1572},
{1,1,2,14,0,1,3704,396,1572},
{1,1,3,14,0,1,335,36,217},
{1,1,4,14,13,1,3899,422,1767},
{1,1,5,14,0,1,3689,394,1557},
{1,1,6,14,0,1,3704,396,1572},
{1,1,7,14,0,1,3704,396,1572},
{1,1,8,256,0,1,40004,4510,23594},
{2,0,0,14,0,0,327,35,209},
{2,0,1,14,0,0,3696,395,1564},
{2,0,2,14,0,0,3696,395,1564},
{2,0,3,14,0,0,327,35,209},
{2,0,4,14,13,0,3891,421,1759},
{2,0,5,14,0,0,3681,393,1549},
{2,0,6,14,0,0,3696,395,1564},
{2,0,7,14,0,0,3696,395,1564},
{2,0,8,256,0,0,39996,4509,23586},
{2,1,0,14,0,0,327,35,209},
{2,1,1,14,0,0,3696,395,1564},
{2,1,2,14,0,0,3696,395,1564},
{2,1,3,14,0,0,327,35,209},
{2,1,4,14,13,0,3891,421,1759},
{2,1,5,14,0,0,3681,393,1549},
{2,1,6,14,0,0,3696,395,1564},
{2,1,7,14,0,0,3696,395,1564},
{2,1,8,256,0,0,39996,4509,23586},
{3,0,0,14,0,0,2291,269,1406},
{3,0,1,14,0,0,4434,495,2302},
{3,0,2,14,0,0,4434,495,2302},
{3,0,3,14,0,0,2291,269,1406},
{3,0,4,14,13,0,4335,479,2203},
{3,0,5,14,0,0,4419,493,2287},
{3,0,6,14,0,0,4161,456,2029},
{3,0,7,14,13,4,3050,360,2165},
{3,0,8,256,0,0,53318,6303,36908},
{3,1,0,14,0,0,2291,269,1406},
{3,1,1,14,0,0,4434,495,2302},
{3,1,2,14,0,0,4434,495,2302},
{3,1,3,14,0,0,2291,269,1406},
{3,1,4,14,13,0,4335,479,2203},
{3,1,5,14,0,0,4419,493,2287},
{3,1,6,14,0,0,4161,456,2029},
{3,1,7,14,13,4,3050,360,2165},
{3,1,8,256,0,0,53318,6303,36908}};
int main(){try{
 using T=ks::VoiceSelectionTask;using R=T::Read;using W=T::Write;using C=T::Child;
 unsigned count=0;
 for(const auto& example:cases){
  T task;unsigned current=0,allocation=0,own=0,reads=0,writes=0,steps=0,voiceLookups=0,selects=0,uploads=0,gates=0;
  struct Slot {uint32_t age;unsigned owner=2,unison=3,note=61;};std::array<Slot,256> slots{};
  for(unsigned voice=0;voice<example.voices;++voice){const auto shape=example.shape;auto& slot=slots[voice];slot.age=shape==0?0:shape==1?0x80000000u+voice:shape==2?0x10000u+voice:shape==3?0x100u+voice:shape==4?0x1000000u+example.voices-voice:shape==5?0xffffffffu:shape==6?0x1000000u:shape==7?0xc0000000u+voice:0x10000u+voice;
   slot.owner=shape==4?3:shape==6?(voice==example.voices-1?2:3):2;slot.note=shape==7&&voice==example.voices-1?60:61;}
  auto read=[&](R what)->uint32_t{++reads;const auto& slot=slots[task.slot()];switch(what){
   case R::PatchMode:return example.mode<<3;case R::Held:return example.held;case R::Unison:return 3;case R::Part:return 2;case R::Count:return uint8_t(example.voices);
   case R::Age:return slot.age;case R::OwnerPart:return slot.owner;case R::OwnerUnison:return slot.unison;case R::SlotNote:return slot.note;case R::VoiceIndex:return current;case R::IncomingNote:return 60;}throw std::runtime_error("Unexpected field");};
  auto write=[&](W what,uint8_t value){++writes;if(what==W::VoiceIndex)current=value;else allocation=value;};
  while(!task.complete()){
   if(task.child()!=C::None){const auto child=task.child();if(child==C::Voice)++voiceLookups;else if(child==C::SelectVoice)++selects;else if(child==C::WriteOne||child==C::WriteTwo)++uploads;else if(child==C::Gate)++gates;
    check(task.resumeChild(uint8_t(child==C::Part?2:current)),"Native selection child not acknowledged");continue;}
   const auto cost=task.nextClocks();check(cost,"Native selection stalled");const auto beforeReads=reads,beforeWrites=writes;
   auto p=task.advanceBefore(cost-1,read,write,[](W){return 3u;});check(!p.clocks&&!p.boundaries&&reads==beforeReads&&writes==beforeWrites,"Selection crossed deadline");
   p=task.advanceBefore(cost,read,write,[](W){return 3u;});check(p.clocks==cost&&p.boundaries==1,"Selection boundary mismatch");own+=p.clocks;check(++steps<12000,"Selection loop did not terminate");
  }
  if(current!=example.selected||allocation!=example.allocation||own!=example.own){std::cerr<<"mode="<<example.mode<<" held="<<example.held<<" shape="<<example.shape<<" selected="<<current<<"/"<<example.selected<<" mode="<<allocation<<"/"<<example.allocation<<" own="<<own<<"/"<<example.own<<'\n';throw std::runtime_error("Native selection policy/time differs from direct original");}
  check(voiceLookups&&selects<=1&&uploads==selects*2&&gates==selects,"Release child sequence differs");++count;
 }

 // Independent high-level policy oracle for random slot arrays. It performs no
 // timed task steps or compatibility projection and treats ages as unsigned.
 std::mt19937 rng(0x4b535650);unsigned randomized=0;
 for(unsigned trial=0;trial<8192;++trial){
  struct Slot {uint32_t age;uint8_t part,unison,note;};std::array<Slot,256> slots{};
  const unsigned mode=rng()%4,held=trial%5?uint8_t(rng()|1):0,voices=trial%127?1+rng()%14:256,ownerPart=rng()%4,ownerUnison=rng()%8,incoming=rng()%128;
  for(unsigned i=0;i<voices;++i){auto& slot=slots[i];slot.age=trial%7==0?0:trial%7==1?0xffffffffu:trial%7==2?(rng()&0xffffu):rng();slot.part=uint8_t(rng()%4);slot.unison=uint8_t(rng()%8);slot.note=uint8_t(rng()%128);}
  unsigned expected=0,expectedMode=0;uint32_t best=0xffffffffu;bool release=false;
  auto matches=[&](unsigned i){return slots[i].part==ownerPart&&slots[i].unison==ownerUnison;};
  auto minimum=[&](unsigned i){if(slots[i].age<best){best=slots[i].age;expected=i;}};
  if(mode<2&&!held){
   unsigned firstMatch=voices;for(unsigned i=0;i<voices;++i){if(matches(i)){firstMatch=i;break;}minimum(i);}
   if(firstMatch==voices){expectedMode=1;release=(best>>16)!=0;}
   else {best=0;bool immediate=false;for(unsigned i=firstMatch;i<voices;++i)if(matches(i)){
     if(slots[i].age&0xc0000000u){expected=i;expectedMode=4;immediate=true;break;}
     if(slots[i].age>best){best=slots[i].age;expected=i;}
    }if(!immediate)expectedMode=!(best>>16)?1:(best>>16)<256?2:3;}
  }else if(mode==2||(mode<2&&held)){
   expectedMode=mode==2?0:1;bool found=false;for(unsigned i=0;i<voices;++i){if(!(slots[i].age>>16)){expected=i;found=true;break;}minimum(i);}release=!found;
  }else {bool found=false;for(unsigned i=0;i<voices;++i){if((slots[i].age>>16)&&matches(i)&&slots[i].note==incoming){expected=i;expectedMode=4;found=true;break;}minimum(i);}if(!found)release=(best>>16)!=0;}
  T task;unsigned current=0,actualMode=0,selects=0,uploads=0,gates=0,steps=0;
  auto read=[&](R what)->uint32_t{const auto& slot=slots[task.slot()];switch(what){
   case R::PatchMode:return mode<<3;case R::Held:return held;case R::Unison:return ownerUnison;case R::Part:return ownerPart;case R::Count:return uint8_t(voices);case R::Age:return slot.age;
   case R::OwnerPart:return slot.part;case R::OwnerUnison:return slot.unison;case R::SlotNote:return slot.note;case R::VoiceIndex:return current;case R::IncomingNote:return incoming;}return 0;};
  auto write=[&](W what,uint8_t value){if(what==W::VoiceIndex)current=value;else actualMode=value;};
  while(!task.complete()){
   if(task.child()!=C::None){const auto child=task.child();if(child==C::SelectVoice)++selects;else if(child==C::WriteOne||child==C::WriteTwo)++uploads;else if(child==C::Gate)++gates;check(task.resumeChild(uint8_t(child==C::Part?ownerPart:current)),"Random selection child stalled");}
   else {const auto p=task.advanceBefore(task.nextClocks(),read,write,[](W){return 3u;});check(p.boundaries==1,"Random selection did not advance");}check(++steps<16000,"Random selection loop stalled");
  }
  if(current!=expected||actualMode!=expectedMode||selects!=unsigned(release)||uploads!=unsigned(release)*2||gates!=unsigned(release)){std::cerr<<"trial="<<trial<<" mode="<<mode<<" held="<<held<<" selected="<<current<<"/"<<expected<<" allocation="<<actualMode<<"/"<<expectedMode<<'\n';throw std::runtime_error("Random policy differs from independent selection oracle");}++randomized;
 }
 ks::VoiceGateOffTask gate;unsigned gateClocks=0;while(gate.nextClocks()){const auto cost=gate.nextClocks();check(!gate.advanceBefore(cost-1),"Gate crossed deadline");check(gate.advanceBefore(cost),"Gate failed admission");gateClocks+=cost;}
 check(gateClocks==29&&gate.stage()==ks::VoiceGateOffTask::Stage::Await&&gate.resumeChild()&&gate.stage()==ks::VoiceGateOffTask::Stage::Complete,"Gate child completion/duration differs");
 std::cout<<"PASS: "<<randomized<<" independent random slot policies, plus gate-off typed command/deadlines\n";
 std::cout<<"PASS: "<<count<<" native selection policies/own-clock schedules match independent original, all four modes, ownership/age/retrigger/ties,14/256 slots; no CPU or guest state in task\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
