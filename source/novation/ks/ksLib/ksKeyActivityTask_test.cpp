#include "ksKeyActivityTask.h"
#include <vector>
#include <iostream>
#include <stdexcept>
using T=ks::KeyActivityTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{unsigned cases=0;
 for(auto kind:{T::Kind::Held,T::Kind::Physical})for(unsigned part=0;part<256;++part)for(unsigned found=0;found<=8;++found)for(unsigned word:{1u,0x80u,0x100u,0x8000u,0xffffu}){
 T task(kind);std::vector<unsigned> reads;unsigned clocks=0;
 while(!task.complete()){
  if(task.awaitingPart()){check(task.resumePart(uint8_t(part)),"Part rejected");continue;}
  auto reader=[&](unsigned offset){check(offset==(kind==T::Kind::Held?0xd0u:0xc0u)+2*reads.size(),"Read order differs");reads.push_back(offset);return uint16_t(reads.size()-1==found?word:0);};
  auto cost=task.nextClocks();check(cost>0,"Stalled task");check(!task.advanceBefore(cost-1,reader),"Early admission");auto before=reads.size();
  check(task.advanceBefore(cost,reader)==cost,"Admission differs");check(reads.size()<=before+1,"Multiple reads at boundary");clocks+=cost;
 }
 check(task.part()==part,"Part handle differs");check(reads.size()==(found==8?8:found+1),"Scan length differs");check(task.result()==(found==8?0:word),"Result differs");
 check(task.remaining()==(found==8?0:8-found),"Remaining count differs");check(clocks==(found==8?191:66+17*found),"Own clocks differ");check(!task.resumePart(0),"Stray reply accepted");++cases;
 }
 std::cout<<"PASS: "<<cases<<" keyboard map/part/result/first-word scenarios, ordered reads, bounded admission, exact own clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
