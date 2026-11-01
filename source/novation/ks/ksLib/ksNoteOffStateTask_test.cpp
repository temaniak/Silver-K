#include "ksNoteOffStateTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::NoteOffStateTask;using S=T::Stage;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 auto verify=[&](unsigned note,unsigned pedal,unsigned mode,bool held,unsigned byte,unsigned latency){
  T task;unsigned reads=0,writes=0,heldWrites=0,physicalWrites=0,partReplies=0,compactReplies=0,heldReplies=0,releaseReplies=0,resetReplies=0,clocks=0;std::vector<S> dispatch;
  while(!task.complete()){
   if(task.awaitingPart()){check(task.resumePart(uint8_t(note)),"Part reply rejected");++partReplies;continue;}
   if(task.awaitingCompact()){check(task.resumeCompact(uint8_t(note^0xa5)),"Compact reply rejected");++compactReplies;continue;}
   if(task.awaitingHeld()){check(task.resumeHeld(held,uint8_t(note^0x5a)),"Held reply rejected");++heldReplies;continue;}
   if(task.awaitingRelease()){check(task.resumeRelease(),"Release reply rejected");++releaseReplies;continue;}
   if(task.awaitingMonoReset()){check(task.resumeMonoReset(),"Mono reset reply rejected");++resetReplies;continue;}
   if(task.awaitingPhysicalPart()){check(task.resumePhysicalPart(uint8_t(255-note)),"Final part reply rejected");++partReplies;continue;}
   const auto stage=task.stage();
   auto read=[&](T::Read field,unsigned offset)->unsigned{++reads;switch(field){
    case T::Read::Note:return stage==S::CaptureNote?note:stage==S::ClearHeld?uint8_t(note+5):uint8_t(note+3);
    case T::Read::Pedal:check(task.part()==note&&offset==327,"Pedal part/address differs");return pedal;
    case T::Read::Mode:check(task.part()==(note^0xa5)&&offset==324,"Mode not observed from returned compact part");return mode;
    case T::Read::HeldByte:check(task.part()==note&&offset==208+note/8,"Held map rebound/indexed incorrectly");return byte;
    case T::Read::PhysicalByte:check(task.part()==255-note&&offset==192+note/8,"Physical map not independently rebound");return byte;
   }return 0;};
   auto write=[&](T::Write field,unsigned offset,unsigned data){++writes;if(field==T::Write::HeldByte){++heldWrites;check(stage==S::ClearHeld&&offset==208+note/8,"Held publication differs");check(data==(byte&~(1u<<((note+5)&7))),"Held bit uses stale note");}else{++physicalWrites;check(stage==S::ClearPhysical&&offset==192+note/8,"Physical publication differs");check(data==(byte&~(1u<<((note+3)&7))),"Physical bit uses stale note");}};
   auto timing=[&](unsigned offset){check(offset==task.publicationOffset(),"Store timing field differs");return latency;};
   const auto cost=task.nextClocks(task.publication()?latency:3);check(cost>0,"Task stalled");const auto beforeReads=reads,beforeWrites=writes;
   check(!task.advanceBefore(cost-1,read,write,timing),"Early Note Off admission");check(reads==beforeReads&&writes==beforeWrites,"Early Note Off access");
   check(task.advanceBefore(cost,read,write,timing)==cost,"Note Off deadline differs");clocks+=cost;
   if(stage==S::DispatchPart||stage==S::DispatchCompact||stage==S::DispatchHeld||stage==S::DispatchRelease||stage==S::DispatchMonoReset||stage==S::DispatchPhysicalPart)dispatch.push_back(stage);
  }
  const bool sustain=pedal&16,scan=!sustain&&(mode&64),release=scan&&!held;
  check(partReplies==2&&compactReplies==unsigned(!sustain)&&heldReplies==unsigned(scan)&&releaseReplies==unsigned(release)&&resetReplies==unsigned(release),"Native child decisions differ");
  check(physicalWrites==1&&heldWrites==unsigned(!sustain),"Native bitmap publications differ");
  std::vector<S> expected{S::DispatchPart};if(!sustain)expected.push_back(S::DispatchCompact);if(scan)expected.push_back(S::DispatchHeld);if(release){expected.push_back(S::DispatchRelease);expected.push_back(S::DispatchMonoReset);}expected.push_back(S::DispatchPhysicalPart);check(dispatch==expected,"Native child order differs");
  const unsigned duration=256+(!sustain?60:0)+(scan?23:0)+(release?222:0)-(3-latency)*(sustain?1:2);check(clocks==duration,"Whole Note Off own schedule differs");
  check(!task.resumePart(0)&&!task.resumeCompact(0)&&!task.resumeHeld(false,0)&&!task.resumeRelease()&&!task.resumeMonoReset()&&!task.resumePhysicalPart(0),"Spurious child reply accepted");++cases;
 };
 for(unsigned note=0;note<256;++note)for(unsigned pedal=0;pedal<256;++pedal)for(unsigned mode:{0u,64u})for(bool held:{false,true})for(unsigned latency:{0u,3u})verify(note,pedal,mode,held,uint8_t(note*17+pedal*7),latency);
 for(unsigned mode=0;mode<256;++mode)for(unsigned note:{0u,1u,60u,127u,128u,255u})for(unsigned byte=0;byte<256;++byte)for(bool held:{false,true})verify(note,0,mode,held,byte,3);
 std::cout<<"PASS: "<<cases<<" full-byte Note Off note/pedal/mode/map properties; exact sustain/mono child order, native part rebinding, original key index with live bit, bitmap arithmetic, complete own schedules and access deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
