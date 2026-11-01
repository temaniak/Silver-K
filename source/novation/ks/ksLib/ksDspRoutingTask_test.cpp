#include "ksDspRoutingTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
 using T=ks::DspRoutingTask;using K=T::Kind;using R=T::Read;using W=T::Write;using Q=T::Request;
 uint64_t cases=0;
 for(K kind:{K::PrimeRelative,K::WriteRelative,K::ReadAlternate,K::VoiceB,K::VoiceA,K::PrimeAbsolute,K::WriteAbsolute,K::WriteA,K::WriteB}){
  const unsigned limit=kind==K::VoiceB?256:65536;
  for(unsigned input=0;input<limit;++input){
   T task(kind,uint16_t(input));unsigned reads=0,writes=0,children=0,clocks=0;uint16_t primary=uint16_t(input*37+101),alternate=uint16_t(input*23+22),cache=uint16_t(input^0xa55a);const uint8_t voice=uint8_t(input),base=uint8_t(input>>8);
   auto read=[&](R what)->unsigned{++reads;switch(what){case R::PrimaryOffset:return primary;case R::AlternateOffset:return alternate;case R::VoiceB:case R::VoiceA:return voice;case R::VoiceBase:return base;case R::CachedAddress:return cache;}return 0;};
   auto write=[&](W what,uint16_t value){++writes;if(what==W::PrimaryOffset)primary=value;else if(what==W::AlternateOffset)alternate=value;else cache=value;};
   unsigned boundaries=0;
   while(!task.complete()){
    if(task.request()!=Q::None){++children;check(task.resumeChild(),"Routing child did not resume");continue;}
    const auto cost=task.nextClocks();const auto oldReads=reads,oldWrites=writes;
    const auto shortStep=task.advanceBefore(cost-1,read,write);check(!shortStep.clocks&&!shortStep.boundaries&&reads==oldReads&&writes==oldWrites,"Routing crossed deadline");
    const auto p=task.advanceBefore(cost,read,write);check(p.clocks==cost&&p.boundaries==1,"Routing step duration differs");clocks+=p.clocks;check(++boundaries<24,"Routing stalled");
   }
   if(kind==K::VoiceB){check(primary==voice*101u&&alternate==voice*22u&&reads==1&&writes==2&&children==1&&clocks==117,"B voice window differs");}
   else if(kind==K::VoiceA){const unsigned wrapped=uint8_t(voice-base),index=wrapped&128?0:wrapped;check(primary==index*53&&alternate==index*5&&reads==2&&writes==2&&children==1&&clocks==(wrapped&128?138u:135u),"A voice window differs");}
   else if(kind==K::PrimeRelative||kind==K::WriteRelative||kind==K::ReadAlternate){const bool useAlternate=kind==K::ReadAlternate||(input&0x8000);check(task.address()==uint16_t(input+(useAlternate?alternate:primary))&&writes==0&&children==1,"Relative address resolution differs");}
   else if(kind==K::PrimeAbsolute){check(cache==input&&reads==0&&writes==1&&children==1&&clocks==32,"Address mirror publication differs");}
   else {check(task.cached()==cache&&reads==1&&writes==0&&children==0&&task.request()==Q::AddressThenWord&&clocks==(kind==K::WriteB?47u:kind==K::WriteA?44u:35u),"Write preparation or unconditional request differs");}
   ++cases;
  }
 }
 std::cout<<"PASS: "<<cases<<" native routing values/deadlines/requests; all 16-bit addresses, every A voice/base byte pair, all B voices, both offset signs; no CPU state\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
