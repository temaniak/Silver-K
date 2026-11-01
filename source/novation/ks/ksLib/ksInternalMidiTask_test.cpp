#include "ksInternalMidiTask.h"
#include <vector>
#include <tuple>
#include <iostream>
#include <stdexcept>
using T=ks::InternalMidiTask;using S=T::Stage;using C=T::Child;using Event=std::tuple<S,uint32_t>;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;auto verify=[&](unsigned input,unsigned remaining,unsigned status,unsigned length,unsigned flags,unsigned channel,bool held,unsigned policy,uint32_t cursor,unsigned latency){
 T task(uint8_t(input),0xffa00003u);std::vector<Event>writes;std::vector<C>children;unsigned reads=0,loops=0;while(!task.complete()&&!task.handoff()){
  check(++loops<100,"Internal MIDI pure stalled");if(task.awaitingChild()){children.push_back(task.child());if(task.child()==C::Descriptor){const auto expected=ks::MidiDescriptorKey::forStatus(uint8_t(input>=128?input:status));check(task.descriptor().family==expected.family&&task.descriptor().index==expected.index,"Internal descriptor domain differs");check(task.resumeDescriptor(),"Descriptor reply rejected");}else check(task.resumeHeld(199,held),"Held reply rejected");continue;}
  auto read=[&](S s)->uint32_t{++reads;switch(s){case S::ReadStatusLength:case S::ReadRunningLength:return length;case S::ReadStatusFlags:case S::ReadRunningFlags:return flags;case S::ReadImmediateHandler:case S::ReadMessageHandler:return 0xdeadbeef;case S::ReadRemaining:return remaining;case S::ReadCursor:return cursor;case S::ReadRunningStatus:return status;case S::ReadChannel:return channel;case S::GuardReadPolicy:return policy;default:throw std::runtime_error("Unexpected internal read");}};auto write=[&](S s,uint32_t d){writes.emplace_back(s,d);};auto timing=[&](S){return latency;};const auto cost=task.nextClocks(task.publication()?latency:3);check(cost>0,"Internal deadline stalled");const auto count=reads+writes.size();check(!task.advanceBefore(cost-1,read,write,timing)&&reads+writes.size()==count,"Internal early access");check(task.advanceBefore(cost,read,write,timing)==cost,"Internal deadline mismatch");}
 std::vector<Event>ew;std::vector<C>ec;bool guard=false,completeMessage=false;
 if(input>=128){ec.push_back(C::Descriptor);if(flags&2)guard=true;else{ew.emplace_back(S::PublishStatusRemaining,length);if(!length)guard=true;else{ew.emplace_back(S::PublishRunningStatus,input);ew.emplace_back(S::PublishChannel,input&15);ew.emplace_back(S::PublishStatusCursor,0x15c28);}}}
 else if(remaining){ew.emplace_back(S::PublishData,input);ew.emplace_back(S::PublishDataRemaining,uint8_t(remaining-1));if(remaining>1)ew.emplace_back(S::PublishDataCursor,cursor+1);else{completeMessage=guard=true;ew.emplace_back(S::PublishDispatchStatus,status);ec.push_back(C::Descriptor);if(flags&1){ew.emplace_back(S::PublishRunningRemaining,length);ew.emplace_back(S::PublishRunningCursor,0x15c28);}ew.emplace_back(S::PublishDispatchChannel,channel);}}
 if(guard)ec.push_back(C::Held);check(writes==ew,"Internal parser publication order/value differs");check(children==ec,"Internal child ordering differs");check(task.handoff()==bool(guard&&held&&(policy&4)),"Internal guarded dispatch differs");check(task.payload()==(completeMessage?0x15c28u:0xffa00003u),"Immediate/payload source differs");++cases;
 };
 for(unsigned latency:{0u,3u}){
  for(unsigned input=128;input<256;++input)for(unsigned length=0;length<256;++length)for(unsigned flags=0;flags<4;++flags)for(unsigned shape=0;shape<4;++shape)verify(input,0,0,length,flags,7,shape&1,shape&2?4:0,0xffffffffu,latency);
  for(unsigned input=0;input<128;++input)for(unsigned remaining=0;remaining<256;++remaining)verify(input,remaining,uint8_t(input*19),2,remaining&3,uint8_t(input+remaining),remaining&1,remaining&2?4:0,remaining&1?0xffffffffu:0xffa00001u,latency);
  for(unsigned status=0;status<256;++status)for(unsigned flags=0;flags<256;++flags)for(unsigned remaining:{0u,1u,2u})verify(64,remaining,status,uint8_t(status+flags),flags,uint8_t(status^flags),flags&4,flags,0xa30001,latency);
 }
 std::cout<<"PASS: "<<cases<<" pure internal MIDI decoders, independent status/running-status/payload/guard oracle, full byte counts/lengths/flags/status and wrapped pointers, semantic descriptor/held replies and early deadlines\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
