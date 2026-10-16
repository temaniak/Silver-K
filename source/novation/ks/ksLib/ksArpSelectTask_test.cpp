#include "ksArpSelectTask.h"
#include <array>
#include <iostream>
#include <tuple>
#include <vector>
#include <stdexcept>
using T=ks::ArpSelectTask;using K=T::Kind;using S=T::Stage;using C=T::Child;using Note=std::pair<unsigned,unsigned>;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;auto verify=[&](K kind,unsigned mode,unsigned count,unsigned index,unsigned octave,unsigned range,unsigned from,unsigned direction,unsigned pattern,unsigned candidate,unsigned seed,unsigned sample,unsigned latency){
 T t(kind,17,uint8_t(mode),uint8_t(count),uint8_t(from),uint8_t(candidate));unsigned indexState=index,octaveState=octave,lastState=from,directionState=direction,reads=0,writes=0,loops=0;uint16_t seedState=uint16_t(seed);std::vector<Note> notes;std::array<bool,128> keys{};for(unsigned i=0;i<128;++i)keys[i]=pattern==1?i==60:pattern==2?i==12||i==60||i==84:pattern==3?true:pattern==4?i==0||i==127:false;
 const auto any=[&]{for(bool k:keys)if(k)return true;return false;};const auto neighbor=[&](unsigned note,bool forward)->int{if(forward){for(unsigned n=note+1;n<128;++n)if(keys[n])return int(n);}else if(note<=128){for(int n=int(note)-1;n>=0;--n)if(keys[unsigned(n)])return n;}return -1;};const unsigned randomReply=uint8_t(seed*17+index*3+count);
 const auto listNote=[](unsigned i){return uint8_t(24+uint8_t(i*2)/2*3);};
 const auto ordered=[&]{const unsigned selected=mode&16?randomReply:indexState;const unsigned n=listNote(selected);const unsigned next=uint8_t(indexState+1);if(next>=count){indexState=0;octaveState=octaveState<range?uint8_t(octaveState+1):0;}else indexState=next;notes.emplace_back(n,octaveState);lastState=n;};
 while(!t.complete()&&!t.handoff()){
  check(++loops<10000,"Pure selection stalled");if(t.awaitingChild()){switch(t.child()){
   case C::Part:check(t.resumePart(199),"Part rejected");break;case C::Held:check(t.resumeHeld(199,any()),"Held rejected");break;case C::Clock:check(t.resumeClock(sample),"Clock rejected");break;case C::Random:check(t.resumeRandom(uint16_t(randomReply)),"Random rejected");break;
   case C::Ordered:ordered();check(t.resumeChild(),"Ordered rejected");break;
   case C::Forward:case C::Backward:{const int key=neighbor(t.last(),t.child()==C::Forward);const unsigned note=key<0?t.last():unsigned(key);if(key>=0){notes.emplace_back(note,octaveState);lastState=note;}check(t.resumeKey(uint8_t(note),key>=0),"Key reply rejected");break;}
   case C::Velocity:notes.emplace_back(t.candidate(),octaveState);lastState=t.candidate();check(t.resumeChild(),"Velocity rejected");break;
   case C::Emit:notes.emplace_back(uint8_t(t.key()),octaveState);lastState=uint8_t(t.key());check(t.resumeChild(),"Emit rejected");break;
   case C::SearchForward:case C::SearchBackward:check(t.resumeSearch(uint8_t(candidate)),"Search rejected");break;
   default:throw std::runtime_error("Unexpected selection child");}continue;}
  auto read=[&](S s)->uint32_t{++reads;switch(s){case S::ReadMode:case S::ReadForwardMode:case S::ReadBounceForwardMode:case S::ReadBackwardMode:case S::ReadBounceBackwardMode:return mode;case S::ReadCount:case S::RandomReadCount:return count;case S::OrderedReadIndex:return indexState;case S::OrderedReadKey:return 192<<8|listNote(t.keyOffset()/2);case S::OrderedReadOctave:case S::ReadForwardOctave:case S::ReadBackwardOctave:return octaveState;case S::OrderedReadRange:case S::ReadForwardRange:case S::ReadDescendingRange:case S::ReadBackwardFallbackRange:case S::ReadWrappedBackwardRange:case S::ReadForwardFallbackRange:return range;case S::RandomReadSeed:return seedState;case S::RandomReadIncrement:return 0x1234;case S::ReadLast:return lastState;case S::ReadDirection:return directionState;case S::VelocityRead:check(t.candidate()==candidate,"Velocity lookup candidate differs");return uint8_t(candidate*37);default:throw std::runtime_error("Unexpected pure selection read");}};
  auto write=[&](S s,uint32_t data){++writes;switch(s){case S::PublishQueue:check(data==0,"Queue clear differs");break;case S::PublishOrderedIndex:case S::OrderedPublishIndex:indexState=data;break;case S::OrderedPublishOctave:case S::PublishForwardOctave:case S::PublishWrappedForwardOctave:case S::PublishDescendingRange:case S::PublishBackwardFallbackOctave:case S::PublishBackwardOctave:case S::PublishWrappedBackwardRange:case S::PublishAscendingOctave:case S::PublishForwardFallbackOctave:octaveState=data;break;case S::RandomPublishSeed:seedState=uint16_t(data);break;case S::PublishForwardNote:case S::PublishLastForwardNote:case S::PublishBackwardNote:case S::PublishFirstBackwardNote:lastState=data;break;case S::PublishDescending:case S::PublishAscending:directionState=data;break;default:break;}};auto timing=[&](S){return latency;};const auto cost=t.nextClocks(t.publication()?latency:3);check(cost>0,"Selection deadline stalled");const auto before=reads+writes;check(!t.advanceBefore(cost-1,read,write,timing)&&reads+writes==before,"Early selection access");check(t.advanceBefore(cost,read,write,timing)==cost,"Selection deadline differs");
 }
 std::vector<Note> expected;unsigned expectedIndex=index,expectedOctave=octave,expectedLast=from,expectedDirection=direction;auto output=[&](unsigned note){expected.emplace_back(note,expectedOctave);expectedLast=note;};auto search=[&](unsigned note,bool forward){const int found=neighbor(note,forward);if(found>=0)output(unsigned(found));return found>=0;};auto orderedOracle=[&]{const unsigned chosen=mode&16?randomReply:expectedIndex;const unsigned next=uint8_t(expectedIndex+1);if(next>=count){expectedIndex=0;expectedOctave=expectedOctave<range?uint8_t(expectedOctave+1):0;}else expectedIndex=next;output(listNote(chosen));};
 if(kind==K::Random){const uint16_t expectedSeed=uint16_t(uint64_t(uint16_t(sample))*uint16_t(seed)+0x1234);check(seedState==expectedSeed&&t.randomIndex()==uint16_t(uint32_t(expectedSeed)*count>>16),"LCG/random scaling differs");}
 else if(kind==K::Forward||kind==K::Backward||kind==K::Velocity){const bool accepted=kind==K::Velocity||candidate<128;check(t.handoff()==accepted,"Search sentinel admission differs");if(accepted)check(t.last()==candidate&&t.velocity()==uint8_t(candidate*37),"Velocity handoff differs");}
 else if(kind==K::Ordered){orderedOracle();check(t.complete(),"Ordered task did not return");}
 else if(mode&8){if(count){if(mode&32){expectedIndex=0;for(unsigned i=0;i<count;++i)orderedOracle();}else orderedOracle();}}
 else if(any()){
  if(!direction){if(!search(expectedLast,true)){
   if(expectedOctave<range){++expectedOctave;expectedLast=0;search(expectedLast,true);}else if(!(mode&2)){expectedOctave=0;expectedLast=0;search(expectedLast,true);}else{if(mode&4)expectedLast=127;expectedDirection=1;expectedOctave=range;if(!search(expectedLast,false)){if(range)expectedOctave=range-1;output(expectedLast);}}
  }}else if(!search(expectedLast,false)){
   if(expectedOctave){--expectedOctave;expectedLast=127;search(expectedLast,false);}else if(!(mode&1)){expectedOctave=range;expectedLast=127;search(expectedLast,false);}else{if(mode&4)expectedLast=0;expectedDirection=0;expectedOctave=0;if(!search(expectedLast,true)){if(range)expectedOctave=1;output(expectedLast);}}
  }
 }
 if(kind==K::Run||kind==K::Ordered){check(notes==expected,"Selection note/octave sequence differs");check(indexState==expectedIndex&&octaveState==expectedOctave&&lastState==expectedLast&&directionState==expectedDirection,"Selection final domain state differs");}++cases;
 };
 for(unsigned latency:{0u,3u}){
  for(unsigned count=0;count<256;++count)for(unsigned index=0;index<256;++index)for(unsigned mode:{8u,24u})verify(K::Ordered,mode,count,index,index&3,count&3,60,0,2,0,count*257,0,latency);
  for(unsigned seed=0;seed<65536;++seed)for(unsigned count:{0u,1u,127u,255u})verify(K::Random,0,count,0,0,0,0,0,0,0,seed,uint32_t(seed)*0x12345679u,latency);
  for(unsigned mode=0;mode<8;++mode)for(unsigned note=0;note<128;++note)for(unsigned pattern=0;pattern<5;++pattern)for(unsigned shape=0;shape<4;++shape)for(unsigned direction:{0u,1u})verify(K::Run,mode,3,0,shape&1,shape>>1,note,direction,pattern,0,0,0,latency);
  for(unsigned count=0;count<=32;++count)for(unsigned mode:{8u,24u,40u,56u})for(unsigned index:{0u,1u,31u,255u})verify(K::Run,mode,count,index,1,2,60,0,2,0,17,0,latency);
  for(auto kind:{K::Forward,K::Backward,K::Velocity})for(unsigned candidate=0;candidate<256;++candidate)verify(kind,0,3,0,1,2,60,0,2,candidate,0,0,latency);
 }
 std::cout<<"PASS: "<<cases<<" pure arp selectors, independent ordered/chord and forward/backward bounce/key/octave oracles, full LCG seeds and byte indexes/counts/candidates, typed replies and early deadlines\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
