#include "ksParameterSupportTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::ParameterSupportTask;using K=T::Kind;using S=T::Stage;
void check(bool b,const char* w){if(!b)throw std::runtime_error(w);}
int main(){try{uint64_t cases=0;for(K kind:{K::Rate,K::Clock,K::Sync,K::Exit})for(unsigned a=0;a<(kind==K::Exit?1u:256u);++a)for(unsigned b=0;b<(kind==K::Clock||kind==K::Sync?256u:1u);++b){T task(kind);unsigned reads=0,clocks=0,children=0,packets=0;const uint32_t result=(a*0x9e3779b1u)^(b*0x1234567u)^0xfedcba98;auto read=[&](T::Read r)->unsigned{++reads;return r==T::Read::Flags||r==T::Read::SyncFlags?b:a;};
 while(!task.complete()){
  if(task.awaitingChild()){const auto stage=task.stage();if(stage==S::AwaitCoefficient)check(task.scaleInput()==uint16_t(uint8_t(a-(b&7)-94)*170),"Clock input differs");
   else if(kind==K::Rate){const unsigned word=((a<<8)*2)&65535;check(task.dspWord()==uint32_t(word*0x8102u)>>8,"Rate differs");++packets;}
   else if(kind==K::Clock){check(task.dspWord()==result>>8,"Clock packet differs");++packets;}
   else if(kind==K::Sync){check(task.dspWord()==(packets?((b&32)?a:3):a),"Sync differs");++packets;}
   check(task.resumeChild(result),"Child rejected");++children;continue;}
  const auto cost=task.nextClocks(),before=reads;check(cost>0,"Stalled");check(!task.advanceBefore(cost-1,read)&&reads==before,"Early observation");check(task.advanceBefore(cost,read)==cost,"Admission differs");clocks+=cost;
 }
 const unsigned wanted=kind==K::Rate?70:kind==K::Clock?100:kind==K::Sync?119+((b&32)?0:3):57;
 check(clocks==wanted&&reads==(kind==K::Rate?1u:kind==K::Exit?0u:2u)&&children==(kind==K::Rate?1u:2u)&&packets==(kind==K::Rate||kind==K::Clock?1u:kind==K::Sync?2u:0u)&&!task.resumeChild(),"Support schedule differs");++cases;
 }
 std::cout<<"PASS: "<<cases<<" complete rate/clock/sync/exit jobs; unsigned doubled-word8102, all clock/flag and sync/flag pairs, native coefficient inputs/results, retained sync and terminal child/return deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
