#include "ksStateForegroundTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateForegroundTask;using S=T::Stage;
void check(bool b){if(!b)throw std::runtime_error("Foreground property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned tick=0;tick<65536;++tick)for(unsigned elapsed:{0u,1u,24u,25u,26u,32767u,65535u}){
  T task(T::Kind::Tick);unsigned scans=0,stored=99999;bool pedal=false;unsigned children=0;
  auto read=[&](T::Read field){check(field==T::Read::LastTick);return uint16_t(tick-elapsed);};auto write=[&](T::Write field,unsigned value){check(field==T::Write::LastTick);stored=value;};
  while(!task.complete()){
   const auto before=task.stage();const auto cost=task.nextClocks();check(cost);check(!task.advanceBefore(cost-1,read,write)&&task.stage()==before);check(task.advanceBefore(cost,read,write)==cost);++intervals;
   if(task.awaitingChild()){++children;if(before==S::ScanButtons||before==S::StartAdc||before==S::ScanEncoder||before==S::ScanPedal)++scans;if(before==S::SendPedal)pedal=true;check(task.resumeChild(before==S::ReadTick?uint16_t(tick):0xffff,true));}
  }
  check(scans==(elapsed>=25?4u:0u)&&stored==(elapsed>=25?tick:99999u)&&!pedal&&children==(elapsed>=25?6u:1u));++cases;
 }
 for(unsigned page=0;page<256;++page)for(unsigned delta=0;delta<256;++delta)for(unsigned mode=0;mode<4;++mode){
  T task(T::Kind::Encoder);bool accepted=!(mode&1);unsigned calls=0;
  auto read=[&](T::Read field){if(field==T::Read::Page)return page;if(field==T::Read::SpecialEdit)return mode&2;return mode&2;};auto write=[](T::Write,unsigned){check(false);};
  while(!task.complete()){const auto before=task.stage();const auto cost=task.nextClocks();check(task.advanceBefore(cost,read,write)==cost);++intervals;if(task.awaitingChild()){++calls;check(task.resumeChild(uint16_t(delta),before==S::CheckEncoderContext?accepted:bool(mode&2)));}}
  check(calls==(!accepted?1u:!delta?2u:page==2||page==3||mode&2?2u:3u));++cases;
 }
 std::cout<<"PASS: "<<cases<<" foreground policies / "<<intervals<<" intervals; all tick wraps, 25-tick threshold, encoder pages/admission and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
