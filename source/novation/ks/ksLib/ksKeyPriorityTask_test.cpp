#include "ksKeyPriorityTask.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using C=ks::KeyCompactTask;using M=ks::MonoPriorityTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t compacts=0,monos=0;
 for(unsigned count=0;count<256;++count)for(unsigned note=0;note<256;++note)for(bool duplicate:{false,true})for(unsigned latency:{0u,3u}){
  C task;std::array<uint16_t,256> keys{},expected{};for(unsigned i=0;i<256;++i)keys[i]=uint16_t((255-i)<<8)|(duplicate?i%8:i);expected=keys;
  unsigned matched=count;for(unsigned i=0;i<count;++i)if(uint8_t(keys[i])==note){matched=i;break;}
  if(matched<count)for(unsigned i=matched;i+1<count;++i)expected[i]=expected[i+1];
  unsigned ordered=count,physical=0xc7,writes=0,shifted=0,reads=0,clocks=0;
  while(!task.complete()){
   if(task.awaitingPart()){check(task.resumePart(uint8_t(note)),"Part reply rejected");continue;}
   auto read=[&](C::Read field,unsigned i)->unsigned{++reads;if(field==C::Read::Count)return ordered;if(field==C::Read::Note)return note;check(i<count,"Compaction read beyond ordered count");return keys[i];};
   auto write=[&](C::Write field,unsigned i,unsigned data){++writes;if(field==C::Write::Shift){check(i+1<count,"Invalid compact target");keys[i]=uint16_t(data);++shifted;}else if(field==C::Write::Ordered){check(i==307,"Ordered field moved");ordered=data;}else{check(i==306,"Physical field moved");physical=data;}};
   auto timing=[&](unsigned i){check(i==306||i==307,"Unexpected byte store timing");return latency;};
   const auto cost=task.nextClocks(task.publication()?latency:3);check(cost>0,"Compact stalled");const auto beforeReads=reads,beforeWrites=writes;
   check(!task.advanceBefore(cost-1,read,write,timing),"Early compact admission");check(reads==beforeReads&&writes==beforeWrites,"Early compact access");
   check(task.advanceBefore(cost,read,write,timing)==cost,"Compact deadline differs");clocks+=cost;
  }
  check(keys==expected,"Compaction suffix differs");const bool found=matched<count;
  check(ordered==unsigned(found?count-1:count)&&physical==unsigned(found?count-1:0xc7),"Compaction count publication differs");
  check(shifted==unsigned(found?count-matched-1:0)&&writes==shifted+(found?2:0),"Compaction access count differs");
  const unsigned duration=!count?77:!found?92+20*count:134+23*count-3*matched-2*(3-latency);
  check(clocks==duration,"Compaction whole duration differs");check(!task.resumePart(0),"Spurious compact child accepted");++compacts;
 }
 for(unsigned upper=0;upper<256;++upper)for(unsigned lower=0;lower<256;++lower)for(unsigned note:{0u,60u,127u,255u}){
  M task;unsigned calls=0,reads=0,velocityReads=0,clocks=0;
  while(!task.complete()){
   if(task.awaitingPart()){check(task.resumePart(uint8_t(note)),"Mono part reply rejected");continue;}
   if(task.awaitingUpper()){check(task.resumeUpper(),"Mono upper reply rejected");++calls;continue;}
   if(task.awaitingLower()){check(task.resumeLower(),"Mono lower reply rejected");++calls;continue;}
   auto read=[&](M::Read field,unsigned index)->unsigned{++reads;switch(field){case M::Read::Candidate:return calls==1?upper:lower;case M::Read::Note:return note;default:check(index==note,"Velocity indexed by replacement instead of input key");++velocityReads;return (index*19+7)&255;}};
   const auto cost=task.nextClocks();check(cost>0,"Mono stalled");const auto before=reads;
   check(!task.advanceBefore(cost-1,read),"Early mono admission");check(reads==before,"Early mono observation");check(task.advanceBefore(cost,read)==cost,"Mono deadline differs");clocks+=cost;
  }
  unsigned selected=lower;const unsigned hi=uint8_t(upper-note),lo=uint8_t(note-lower);
  if(upper<128&&(lower>=128||hi<=lo))selected=upper;
  check(task.replacement()==selected&&task.upperDistance()==hi&&task.lowerDistance()==lo,"Native mono priority arithmetic differs");
  check(velocityReads==unsigned(selected<128),"Invalid replacement read velocity");if(selected<128)check(task.velocity()==((note*19+7)&255),"Mono velocity differs");
  check(calls==2,"Adjacent search count differs");check(!task.resumePart(0)&&!task.resumeLower()&&!task.resumeUpper(),"Spurious mono child accepted");
  const unsigned duration=281+(upper<128?6+(lower>=128?3:6+(hi<=lo?3:0)):0)+(selected<128?24:0);check(clocks==duration,"Mono whole duration differs");++monos;
 }
 std::cout<<"PASS: "<<compacts<<" compaction count/note/duplicate/store-delay properties and "<<monos<<" full-byte adjacent-candidate/distance/velocity properties; exact access deadlines, suffix/counters, independent arithmetic and compaction whole schedule\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
