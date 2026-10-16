#include "ksMidiPartInitTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
using P=ks::MidiPartInitTask;using C=ks::MidiControlInitTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{unsigned cases=0;
 for(unsigned seed=0;seed<256;++seed)for(unsigned latency:{0u,3u}){
  P task;std::array<std::array<uint8_t,334>,256> memory;for(auto& p:memory)p.fill(0x5a);std::array<unsigned,5> bindings{};std::vector<unsigned> selected;unsigned reads=0,writes=0,clocks=0;
  const uint32_t reference=seed*0x01010101u^0xffa30000u;
  while(!task.complete()){
   if(task.awaitingPart()){const auto part=uint8_t(task.index()*59+seed);bindings[task.index()]=part;check(task.resumePart(part),"Initialization part reply rejected");continue;}
   auto read=[&]{++reads;return reference;};
   auto write=[&](P::Write field,unsigned offset,uint32_t data){++writes;if(field==P::Write::SelectedPart){selected.push_back(data);return;}auto& p=memory[task.part()];if(field==P::Write::Reference)for(unsigned i=0;i<4;++i)p[offset+i]=uint8_t(data>>(24-8*i));else p[offset]=uint8_t(data);};
   auto timing=[&](P::Write,unsigned){return latency;};const auto cost=task.nextClocks(task.publication()?latency:3);const auto beforeReads=reads,beforeWrites=writes;
   check(cost>0&&!task.advanceBefore(cost-1,read,write,timing)&&reads==beforeReads&&writes==beforeWrites,"Early initialization access");check(task.advanceBefore(cost,read,write,timing)==cost,"Part initialization deadline differs");clocks+=cost;
  }
  check(selected==std::vector<unsigned>{0,1,2,3,4}&&reads==5&&writes==1310,"Part initialization ordering/count differs");
  for(unsigned part=0;part<256;++part){bool bound=false;for(auto p:bindings)bound|=p==part;for(unsigned at=0;at<334;++at){unsigned wanted=0x5a;if(bound){if(at<4)wanted=uint8_t(reference>>(24-8*at));else if(at>=60&&at<=312)wanted=0;else for(unsigned field:{314u,331u,332u,318u,328u,327u,324u})if(at==field)wanted=255;}check(memory[part][at]==wanted,"Part initialization changed unrelated byte");}}
  check(task.index()==5&&!task.remainingParts()&&!task.remainingData()&&task.dataOffset()==313,"Part initialization counters differ");check(clocks==20073-1305*(3-latency),"Part initialization whole schedule differs");check(!task.resumePart(0),"Spurious init part reply accepted");++cases;
 }
 for(unsigned seed=0;seed<256;++seed)for(unsigned latency:{0u,3u}){
  C task;std::array<std::array<uint8_t,20>,5> records{};unsigned reads=0,writes=0,patches=0,clocks=0;
  while(!task.complete()){
   auto read=[&](unsigned offset){++reads;check(offset<20,"Template index exceeds record");return uint8_t(seed*31+offset*17);};
   auto write=[&](C::Write field,unsigned part,unsigned offset,uint32_t data){++writes;check(part<5,"Control init exceeded part count");if(field==C::Write::Patch){++patches;check(data==0x168fc+part*256,"Patch binding differs");for(unsigned i=0;i<4;++i)records[part][i]=uint8_t(data>>(24-8*i));}else records[part][offset]=uint8_t(data);};auto timing=[&](unsigned,unsigned){return latency;};
   const auto cost=task.nextClocks(task.publication()?latency:3);const auto beforeReads=reads,beforeWrites=writes;
   check(cost>0&&!task.advanceBefore(cost-1,read,write,timing)&&reads==beforeReads&&writes==beforeWrites,"Early control initialization access");check(task.advanceBefore(cost,read,write,timing)==cost,"Control initialization deadline differs");clocks+=cost;
  }
  check(reads==100&&writes==105&&patches==5,"Control initialization access count differs");for(unsigned part=0;part<5;++part)for(unsigned at=0;at<20;++at){const unsigned wanted=at<4?uint8_t((0x168fc+part*256)>>(24-8*at)):uint8_t(seed*31+at*17);check(records[part][at]==wanted,"Control template/reference byte differs");}
  check(task.part()==5&&!task.remainingParts()&&!task.remainingBytes()&&task.offset()==20&&task.patch()==0x16dfc,"Control initialization counters differ");check(clocks==2608-100*(3-latency),"Control initialization whole schedule differs");++cases;
 }
 std::cout<<"PASS: "<<cases<<" native part/control initialization binding/reference/template/store-delay properties; exact seven sentinel fields,253-byte clear/unrelated fields, five templates and patch bindings, publication order, independent complete schedules/deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
