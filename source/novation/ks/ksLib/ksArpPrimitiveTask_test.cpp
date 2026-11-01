#include "ksArpPrimitiveTask.h"
#include <iostream>
#include <vector>
#include <tuple>
#include <stdexcept>
using T=ks::ArpPrimitiveTask;using S=T::Stage;using K=T::Kind;using R=T::Read;using W=T::Write;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{uint64_t cases=0;for(unsigned latency:{0u,3u})for(unsigned first=0;first<256;++first)for(unsigned second=0;second<256;++second)for(unsigned kind=0;kind<4;++kind){
 T t(K(kind),uint8_t(first+37));std::vector<std::tuple<W,uint32_t>> writes;std::vector<R> reads;std::vector<unsigned> children;unsigned clocks=0;const uint32_t pattern=0xffa00000u+second,rate=0x80000000u+first*999;const uint16_t step=uint16_t(first*32767+second);const unsigned wordBus=second&1?6:3;
 while(!t.complete()){
  if(t.awaitingPart()){children.push_back(1);check(t.resumePart(uint8_t(first+37)),"Part reply rejected");continue;}
  if(t.awaitingChild()){children.push_back(t.stage()==S::AwaitReset?2:3);check(t.resumeChild(),"Child reply rejected");continue;}
  auto read=[&](R r)->uint32_t{reads.push_back(r);switch(r){case R::Tempo:return second;case R::DesiredTempo:return first;case R::Rate:check(t.rateIndex()==uint8_t(first*4),"Byte rate index arithmetic differs");return rate;case R::Mode:return second;case R::Cursor:return first;case R::Pattern:return t.stage()==S::ReloadPattern?pattern+93:pattern;case R::Step:check(t.nextReference()==pattern+uint8_t(first*2),"Pattern byte index differs");return step;case R::Length:return second;}return 0;};
  auto write=[&](W w,uint32_t v){writes.emplace_back(w,v);};auto timing=[&](W w){return t.stage()==S::ReadStep?wordBus:latency;};const auto cost=t.nextClocks(t.publication()?latency:3,t.stage()==S::ReadStep?wordBus:3);check(cost>0,"Primitive stalled");const auto count=reads.size()+writes.size();check(!t.advanceBefore(cost-1,read,write,timing)&&count==reads.size()+writes.size(),"Primitive accessed before deadline");check(t.advanceBefore(cost,read,write,timing)==cost,"Primitive deadline mismatch");clocks+=cost;
 }
 std::vector<std::tuple<W,uint32_t>> ew;std::vector<R> er;std::vector<unsigned> ec;unsigned expected=0;
 if(kind==0)expected=20;
 else if(kind==1){ec.push_back(1);er={R::Tempo,R::DesiredTempo};expected=58;if(first!=second){ew={{W::Tempo,first},{W::Rate,rate}};er.push_back(R::Rate);ec.push_back(2);expected=144-(3-latency);}}
 else if(kind==2){ew={{W::Accumulator,0},{W::CursorZero,0},{W::PhaseZero,0},{W::GateZero,0}};er={R::Mode};expected=107-3*(3-latency);if(second&64){ec.push_back(3);expected+=20;}}
 else {const auto next=uint8_t(first+1);const bool wrap=next>=second;ew={{W::Cursor,wrap?0u:next}};er={R::Cursor,R::Pattern,R::Step,R::Length};if(wrap)er.push_back(R::Pattern);expected=134+(wordBus-3)+(wrap?24:0)-(3-latency);check(t.step()==step&&t.nextReference()==(wrap?pattern+93:pattern+uint8_t(first*2)+2),"Pattern result differs");}
 check(reads==er,"Primitive read order differs");check(writes==ew,"Primitive write order/value differs");check(children==ec,"Primitive child requests differ");if(clocks!=expected){std::cerr<<kind<<' '<<clocks<<' '<<expected<<'\n';throw std::runtime_error("Primitive clocks differ");}++cases;
 }
 std::cout<<"PASS: "<<cases<<" pure primitives, exhaustive unsigned tempo/mode/cursor/length arithmetic, child requests, reads/publications, early deadlines and exact own clocks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
