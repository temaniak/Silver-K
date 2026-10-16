#include "ksArpScheduleTask.h"
#include <iostream>
#include <vector>
#include <tuple>
#include <stdexcept>
using T=ks::ArpScheduleTask;using S=T::Stage;using R=T::Read;using W=T::Write;using Event=std::tuple<unsigned,unsigned>;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{uint64_t cases=0;for(unsigned latency:{0u,3u})for(unsigned mode:{0u,1u,2u,255u})for(unsigned initialMask=0;initialMask<256;++initialMask)for(unsigned currentMode=0;currentMode<256;++currentMode)for(unsigned workPattern:{0u,1u,2u}){
 T t;std::vector<Event> events;unsigned clocks=0,mask=initialMask,index=0,reads=0;while(!t.complete()){
  if(t.awaitingChild()){events.emplace_back(t.stage()==S::AwaitPart?3:4,index);check(t.resumeChild(),"Schedule child reply rejected");continue;}
  auto read=[&](R r)->unsigned{++reads;switch(r){case R::Mask:return initialMask;case R::Mode:return mode;case R::Work:return workPattern==0?0:workPattern==1?255:index&1;case R::CurrentMode:return uint8_t(currentMode+index);case R::CurrentMask:return mask;}return 0;};auto write=[&](W w,unsigned v){events.emplace_back(unsigned(w),v);if(w==W::Mask)mask=v;if(w==W::Part)index=v;};auto timing=[&](W){return latency;};const auto cost=t.nextClocks(t.publication()?latency:3);check(cost>0,"Schedule stalled");const auto before=events.size()+reads;check(!t.advanceBefore(cost-1,read,write,timing)&&events.size()+reads==before,"Early schedule access");check(t.advanceBefore(cost,read,write,timing)==cost,"Schedule deadline mismatch");clocks+=cost;
 }
 unsigned result=0;std::vector<Event> expected;unsigned imask=initialMask,markCount=0,parts=mode?4:1,first=mode?1:0;expected.emplace_back(unsigned(W::Mask),initialMask);
 for(unsigned p=first;p<first+parts;++p){expected.emplace_back(unsigned(W::Part),p);expected.emplace_back(unsigned(W::Work),0);expected.emplace_back(3,p);const bool marked=workPattern==1||workPattern==2&&(p&1);if(marked){result|=1u<<(uint8_t(p-uint8_t(currentMode+p))&7);++markCount;}imask>>=1;expected.emplace_back(unsigned(W::Mask),imask);}
 expected.emplace_back(4,first+parts-1);check(t.result()==result&&t.remaining()==0&&t.index()==first+parts,"Schedule arithmetic differs");check(events==expected,"Schedule children/publications differ");const unsigned expectedClocks=48+(mode?6:0)+134*parts+21*markCount+37-(1+3*parts)*(3-latency);if(clocks!=expectedClocks){std::cerr<<clocks<<' '<<expectedClocks<<'\n';throw std::runtime_error("Schedule own clocks differ");}++cases;
 }
 std::cout<<"PASS: "<<cases<<" pure schedules, independent part/mask/result oracle, dynamic mode bytes, work patterns, child ordering, deadlines and own clocks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
