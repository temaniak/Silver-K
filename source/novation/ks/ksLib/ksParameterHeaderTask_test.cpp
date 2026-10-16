#include "ksParameterHeaderTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::ParameterHeaderTask;using K=T::Kind;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 for(K kind:{K::Header,K::Part,K::Byte})for(unsigned part=0;part<(kind==K::Byte?1u:256u);++part)for(unsigned base=0;base<(kind==K::Byte?1u:256u);++base)for(unsigned mode=0;mode<(kind==K::Part?1u:256u);++mode){T task(kind);unsigned clocks=0,reads=0,packets=0;
 auto read=[&](unsigned what)->unsigned{++reads;return what==0?part:what==1?base:mode;};const unsigned difference=(part-base)&255,clamped=difference&128?0:difference;
 while(!task.complete()){
  if(task.awaitingWord()){const unsigned want=kind==K::Byte?mode:kind==K::Part?clamped:packets?((mode&64)?3:6):((clamped*3+0x67)&255);check(task.dspWord()==want,"Header packet differs");check(task.resumeWord(),"Completion rejected");++packets;continue;}
  const auto cost=task.nextClocks(),before=reads;check(cost>0,"Stalled");check(!task.advanceBefore(cost-1,read)&&reads==before,"Early read");check(task.advanceBefore(cost,read)==cost,"Admission differs");clocks+=cost;
 }
 const unsigned wanted=kind==K::Header?124+((difference&128)?3:0):kind==K::Part?53+((difference&128)?3:0):38;
 check(clocks==wanted&&reads==(kind==K::Header?3u:kind==K::Part?2u:1u)&&packets==(kind==K::Header?2u:1u),"Header schedule differs");check(!task.resumeWord(),"Extra completion accepted");++cases;
 }
 std::cout<<"PASS: "<<cases<<" complete header/part/raw-mode jobs; all part/base and mode-byte combinations, sign-clamped modular part slots, octavebit6 mode, repeated packet deadlines/exact clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
