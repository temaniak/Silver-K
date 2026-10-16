#include "ksNoteOnStateTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::NoteOnStateTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{unsigned cases=0;
 auto verify=[&](unsigned note,unsigned velocity,unsigned down,bool empty,unsigned part,unsigned latency){
  T task;std::array<uint8_t,334> memory,expected;memory.fill(0x55);memory[306]=uint8_t(down);expected=memory;
  uint16_t last=0;unsigned activityReplies=0,monoReplies=0,partReplies=0,publications=0;
  const unsigned word=(velocity<<8)|note;
  if(empty){for(unsigned at=0xd0;at<0xe0;++at)expected[at]=0;expected[306]=expected[307]=expected[308]=0;}
  const unsigned retained=empty?0:down;
  if(retained<32){expected[240+2*retained]=uint8_t(word>>8);expected[241+2*retained]=uint8_t(word);expected[307]=expected[306]=uint8_t(retained+1);}
  expected[64+note]=uint8_t(velocity);expected[0xc0+(note>>3)]|=uint8_t(1u<<(note&7));expected[0xd0+(note>>3)]|=uint8_t(1u<<(note&7));
  unsigned stages=0;
  while(!task.complete()){
   if(task.awaitingActivity()){check(task.resumeActivity(empty?0:0x8001),"Activity reply rejected");++activityReplies;continue;}
   if(task.awaitingMonoReset()){for(unsigned at=0xd0;at<0xe0;++at)memory[at]=0;memory[306]=memory[307]=memory[308]=0;check(task.resumeMonoReset(),"Mono reply rejected");++monoReplies;continue;}
   if(task.awaitingPart()){check(task.resumePart(uint8_t(part)),"Part rejected");++partReplies;continue;}
   auto read=[&](T::Input input,unsigned at)->unsigned {switch(input){case T::Input::MidiWord:return word;case T::Input::Note:return note;case T::Input::Velocity:return velocity;default:check(at<334,"Read out of bounds");return memory[at];}};
   auto write=[&](unsigned at,unsigned value,unsigned bytes){++publications;if(!bytes){last=uint16_t(value);return;}check(at+bytes<=334,"Write out of bounds");if(bytes==2){memory[at]=uint8_t(value>>8);memory[at+1]=uint8_t(value);}else memory[at]=uint8_t(value);};
   auto timing=[&](unsigned){return latency;};const auto cost=task.nextClocks(task.bytePublication()?latency:3u);check(cost>0,"Stalled task");
   check(!task.advanceBefore(cost-1,read,write,timing),"Early admission");auto before=publications;
   check(task.advanceBefore(cost,read,write,timing)==cost,"Wrong admission");check(publications<=before+1,"Multiple publications");check(++stages<64,"Task looped");
  }
  check(memory==expected,"Aliased map/velocity/ordered-key state differs");check(last==(retained<32?word:0),"Last event differs");check(publications==unsigned(retained<32?7:3),"Publication count differs");
  check(activityReplies==1&&monoReplies==unsigned(empty)&&partReplies==2&&task.part()==part,"Child requests differ");
  check(!task.resumeActivity(0)&&!task.resumeMonoReset()&&!task.resumePart(0),"Stray reply accepted");++cases;
 };
 for(unsigned note=0;note<256;++note)for(unsigned velocity=0;velocity<256;++velocity)for(unsigned down:{0u,31u,32u,255u})verify(note,velocity,down,false,2,3);
 for(unsigned part=0;part<256;++part)for(bool empty:{false,true})for(unsigned latency:{0u,3u})for(unsigned note:{0u,60u,127u,128u,242u,243u,255u})verify(note,255,31,empty,part,latency);
 std::cout<<"PASS: "<<cases<<" all-byte note/velocity pairs, count/part/map/store-delay edges, alias-preserving publications and child deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
