#include "ksMidiDecodeTask.h"
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>
using T=ks::MidiDecodeTask;using S=T::Stage;using R=T::Read;using W=T::Write;using D=ks::MidiDescriptorTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t descriptors=0,cases=0;
 for(unsigned byte=0;byte<256;++byte){D task{uint8_t(byte)};unsigned clocks=0;while(!task.complete()){const auto cost=task.nextClocks();check(!task.advanceBefore(cost-1),"Early descriptor admission");check(task.advanceBefore(cost)==cost,"Descriptor deadline differs");clocks+=cost;}
  const bool system=byte>=240;const unsigned index=system?byte&15:(byte&0x70)>>4;check(task.index()==index&&task.offset()==index*6,"Native descriptor arithmetic differs");check((task.key().family==ks::MidiDescriptorKey::Family::System)==system,"Descriptor category differs");check(clocks==unsigned(system?56:62),"Descriptor own schedule differs");++descriptors;
 }
 auto verify=[&](unsigned byte,unsigned running,unsigned remaining,unsigned length,unsigned flags,unsigned channel,uint32_t cursor,unsigned latency){
  T task;std::vector<std::pair<W,uint32_t>> writes;unsigned reads=0,handlerReads=0,replies=0,messages=0,clocks=0;
  while(!task.complete()&&!task.handoff()){
   if(task.awaitingDescriptor()){
    const unsigned wanted=byte>=128?byte:running;const auto key=task.descriptor();check(key.index==(wanted>=240?(wanted&15):((wanted&0x70)>>4)),"Decoder requested wrong descriptor index");check((key.family==ks::MidiDescriptorKey::Family::System)==(wanted>=240),"Decoder requested wrong descriptor family");
    check(task.resumeDescriptor(),"Decoder descriptor reply rejected");++replies;continue;
   }
   if(task.awaitingMessage()){check(task.resumeMessage(),"Decoder message reply rejected");++messages;continue;}
   auto read=[&](R field)->uint32_t{++reads;switch(field){case R::Input:return byte;case R::Length:return length;case R::Flags:return flags;case R::Remaining:return remaining;case R::Cursor:return cursor;case R::RunningStatus:return running;case R::Channel:return channel;case R::Handler:++handlerReads;return 0xffa12340u;}return 0;};
   auto write=[&](W field,uint32_t value){writes.emplace_back(field,value);};auto timing=[&](W){return latency;};
   const auto cost=task.nextClocks(task.publication()?latency:3);check(cost>0,"Decoder stalled");const auto before=reads;const auto beforeWrites=writes.size();
   check(!task.advanceBefore(cost-1,read,write,timing)&&reads==before&&writes.size()==beforeWrites,"Early decoder access");check(task.advanceBefore(cost,read,write,timing)==cost,"Decoder admission differs");clocks+=cost;
  }
  std::vector<std::pair<W,uint32_t>> expected{{W::Preview,0}};unsigned duration=0,pubs=0;bool immediate=false,full=false;
  if(byte>=128){
   expected.insert(expected.end(),{{W::ReceivedStatus,byte},{W::Activity,0},{W::Expected,length}});immediate=(flags&2)||!length;
   if(!(flags&2))expected.emplace_back(W::Remaining,length);
   if(!immediate)expected.insert(expected.end(),{{W::RunningStatus,byte},{W::Channel,byte&15},{W::Cursor,0x15c1f}});
   duration=(flags&2)?140:!length?155:202;pubs=(flags&2)?4:!length?5:7;
   check(replies==1&&messages==0&&handlerReads==unsigned(immediate),"Status parser child/read decisions differ");check(task.handoff()==immediate,"Status parser completion differs");
  }else if(!remaining){duration=53;pubs=1;check(!replies&&!messages&&!handlerReads&&task.complete(),"Empty parser state dispatched");}
  else{
   expected.emplace_back(W::Data,byte);expected.emplace_back(W::Remaining,uint8_t(remaining-1));
   if(remaining>1){expected.emplace_back(W::Cursor,uint32_t(cursor+1));duration=116;pubs=3;check(!replies&&!messages&&!handlerReads&&task.complete(),"Partial message dispatched");}
   else{
    full=true;expected.emplace_back(W::DispatchStatus,running);
    if(flags&1)expected.insert(expected.end(),{{W::Remaining,length},{W::Cursor,0x15c1f}});
    expected.emplace_back(W::DispatchChannel,channel);duration=(flags&1)?274:220;pubs=(flags&1)?6:5;
    check(replies==1&&messages==1&&handlerReads==1&&task.stage()==S::HandoffLoop,"Completed message child/loop behavior differs");
   }
  }
  check(writes==expected,"MIDI parser publication order/value differs");check(clocks==duration-pubs*(3-latency),"Whole decoder own schedule differs");
  check(!task.resumeDescriptor()&&!task.resumeMessage(),"Spurious decoder child accepted");++cases;
 };
 for(unsigned byte=0;byte<256;++byte)for(unsigned flags=0;flags<256;++flags)for(unsigned length:{0u,1u,2u,255u})for(unsigned latency:{0u,3u})verify(byte,0x90,byte%3,length,flags,byte,0x15c1f,latency);
 for(unsigned remaining=0;remaining<256;++remaining)for(unsigned running=0;running<256;++running)for(unsigned channel:{0u,127u,255u})verify(60,running,remaining,2,running,channel,0xffffffffu,3);
 std::cout<<"PASS: "<<descriptors<<" complete descriptor byte properties and "<<cases<<" incremental MIDI status/data/system/immediate/running-status properties; exact semantic descriptor requests, raw lengths/flags/channel, cursor wrap, publication/child order, independent complete schedules/deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
