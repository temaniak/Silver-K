#include "ksNoteProgramTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::NoteProgramTask;using S=T::Stage;using R=T::Read;using W=T::Write;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 auto verify=[&](bool select,unsigned part,unsigned note,unsigned bank,unsigned sample,unsigned latency){
  T task(select?T::Kind::Select:T::Kind::Play);unsigned clocks=0,reads=0;std::vector<unsigned> children;std::vector<std::pair<W,uint32_t>> writes;
  const uint32_t original=0xffa30000u+part*256;
  while(!task.complete()){
   if(task.awaitingControls()){check(task.resumeControls(31),"Program controls reply rejected");children.push_back(0xa66a);continue;}
   if(task.awaitingPart()){check(task.resumePart(17),"Program part reply rejected");children.push_back(0x4000);continue;}
   if(task.awaitingChild()){children.push_back(task.stage()==S::AwaitPlay?0x36bc:task.stage()==S::AwaitUnison?0x8fcc:0xa00e);check(task.resumeChild(),"Program child reply rejected");continue;}
   auto read=[&](R field)->uint32_t{++reads;switch(field){case R::Part:return part;case R::Program:check(task.tableOffset()==uint8_t(part*2),"Program index failed byte wrap");return (bank<<8)|sample;case R::OriginalReference:check(task.controlPart()==31,"Original patch binding differs");return original;default:return task.stage()==S::PublishLast?uint8_t(note^0x7f):note;}};
   auto write=[&](W field,uint32_t data){writes.emplace_back(field,data);if(field==W::LastNote)check(task.part()==17,"Last-note part binding lost");else check(task.controlPart()==31,"Temporary patch part binding lost");};auto timing=[&](unsigned offset){check(offset==314,"Wrong program publication timing");return latency;};
   const auto cost=task.nextClocks(task.publication()?latency:3);check(cost>0,"Program task stalled");const auto before=reads;const auto beforeWrites=writes.size();
   check(!task.advanceBefore(cost-1,read,write,timing)&&reads==before&&writes.size()==beforeWrites,"Early program access");check(task.advanceBefore(cost,read,write,timing)==cost,"Program deadline differs");clocks+=cost;
  }
  const bool valid=bank<=4||(note>=36&&note<=84),temporary=select&&bank>4&&note>=36&&note<=84&&uint8_t(note-36)!=sample;
  std::vector<unsigned> expected;std::vector<std::pair<W,uint32_t>> published;
  unsigned duration=0;
  if(!select){expected={0x4000,0x8fcc,0xa00e};published={{W::LastNote,uint8_t(note^0x7f)}};duration=92-(3-latency);}
  else if(!valid)duration=note<36?62:68;
  else if(!temporary){expected={0x4000,0x8fcc,0xa00e};published={{W::LastNote,uint8_t(note^0x7f)}};duration=(bank<=4?131:155)-(3-latency);}
  else{
   const uint32_t patch=0x839000+uint8_t(bank-5)*0x3200+uint8_t(note-36)*0x100;
   expected={0xa66a,0x36bc};published={{W::TemporaryReference,patch},{W::OriginalReference,original}};duration=343;
   check(task.patchReference()==patch&&task.originalReference()==original,"Native patch address arithmetic differs");
  }
  check(children==expected&&writes==published,"Program child/publication order differs");check(clocks==duration,"Whole program own schedule differs");
  check(!task.resumeControls(0)&&!task.resumePart(0)&&!task.resumeChild(),"Spurious program reply accepted");++cases;
 };
 for(bool select:{false,true})for(unsigned note=0;note<256;++note)for(unsigned bank=0;bank<256;++bank)for(unsigned sample:{0u,24u,48u,255u})for(unsigned latency:{0u,3u})verify(select,note,note,bank,sample,latency);
 for(unsigned part=0;part<256;++part)for(unsigned sample=0;sample<256;++sample)verify(true,part,60,6,sample,3);
 std::cout<<"PASS: "<<cases<<" native program full-byte part/note/bank/sample properties; byte-wrapped table index, independent generated address, inclusive boundaries, exact temporary restore/child order/own schedules/deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
