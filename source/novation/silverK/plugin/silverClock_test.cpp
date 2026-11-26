#include "silverClock.h"
#include "../../controller/command/nativeMusicalClock.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace silverK;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static std::vector<HostClock::Event> block(HostClock& c,HostClock::Position p,double rate,int n){
 c.begin(p,rate,n);std::vector<HostClock::Event> events;HostClock::Event e{};
 while(c.next(e)){check(e.offset>=0 && e.offset<n,"Clock event outside block");if(!events.empty())check(e.offset>=events.back().offset,"Clock ordering");events.push_back(e);}
 return events;
}
static void checkLiveHostClock(){
 using namespace novation::controller::command;
 NativeMusicalClock clock({48000,1});
 const auto drainBefore=[&](NativeFrame limit){
  while(const auto next=clock.nextDeadline()){
   if(!(*next<limit))break;
   check(clock.takeDue(*next).has_value(),"Drain the native clock");
  }
 };
 auto signal=clock.host({0},{true,false,true,120,4.1,true});
 check(signal && signal->running && signal->source==ClockAuthority::Host,"Host tempo at cold Stop");
 signal=clock.takeDue({0});
 check(signal && signal->tick && signal->running && !signal->followsHostGrid && signal->pulse==0,
       "Stopped transport must ignore the stationary song cursor");
 drainBefore({9600});
 check(!clock.host({9600},{true,false,true,150,4.1,true}) && clock.nextDeadline()==NativeFrame{9920},
       "A tempo edit at Stop must preserve tick phase");
 drainBefore({16000});
 signal=clock.host({16000},{true,true,true,150,3.103,true});
 check(signal && signal->reset && signal->running,"Play must re-anchor a running audition");
 signal=clock.takeDue({16423});
 check(signal && signal->running && signal->followsHostGrid && signal->pulse==75 && signal->at==NativeFrame{16423},
       "Play must take its first pulse from DAW PPQ");
 drainBefore({18600});
 const auto epoch=clock.epoch();
 check(!clock.host({18600},{true,false,true,150,3.21,true}) && clock.running() && clock.epoch()==epoch,
       "Stop must keep the live arpeggio running without a transport reset");
 const auto next=clock.nextDeadline();
 check(next.has_value(),"Stopped host clock must keep ticking");
 signal=clock.takeDue(*next);
 check(signal && signal->tick && !signal->followsHostGrid,"Stopped arpeggio returns to its local phase");

 NativeMusicalClock preroll({48000,1});
 check(preroll.host({0},{true,true,true,120,-.375,true}).has_value(),"Negative PPQ start");
 signal=preroll.takeDue({0});
 check(signal && signal->followsHostGrid && signal->pulse%4==3 && signal->pulse%6==3,
       "Negative PPQ must retain its division phase");

 NativeMusicalClock external({48000,1});
 check(external.setPolicy({0},ClockPolicy::ExternalMidi).has_value(),"Select MIDI Clock");
 check(external.midi({0},MidiRealtime{0xfc}).has_value(),"External MIDI Stop");
 signal=external.midi({1000},MidiRealtime{0xf8});
 check(signal && !signal->running,"External MIDI Stop must still suppress arpeggio ticks");
 check(external.midi({1100},MidiRealtime{0xfa}).has_value() && external.running(),"External MIDI Start");
}
int main(){try{
 for(double rate:{44100.,48000.,96000.})for(double bpm:{60.,123.456,180.})for(int size:{1,127,256,511,4096}){
  HostClock c;int64_t total=0,ticks=0;const int64_t length=int64_t(rate*5);
  for(;total<length;){const int n=int(std::min<int64_t>(size,length-total));const auto events=block(c,{true,true,true,bpm,total*bpm/(60*rate)},rate,n);
   for(auto e:events){if(e.status==0xfa)check(total==0,"Unexpected transport restart");if(e.status!=0xf8)continue;const double expected=ticks*rate*60/(bpm*24);check(std::abs(total+e.offset-expected)<=1.00001,"Clock drift or duplicate/missing tick");++ticks;}total+=n;
  }
  check(ticks==int64_t(std::ceil(length*bpm*24/(60*rate)-1e-7)),"24 PPQN count");
 }
 HostClock c;auto events=block(c,{true,true,true,120,0.01},48000,1000);
 check(events.size()==2 && events[0].status==0xfa && events[1].offset==760,"Fractional PPQ start");
 events=block(c,{true,true,true,60,0.01+1000./24000},48000,2000);
 check(events.size()==1 && events[0].status==0xf8 && events[0].offset==1520,"Tempo change must preserve tick phase");
 events=block(c,{true,true,true,60,4},48000,256);
 check(events.size()==3 && events[0].status==0xfc && events[1].status==0xfa && events[2].status==0xf8 && events[2].offset==0,"Loop/seek restart");
 events=block(c,{true,false,true,60,4+256./48000},48000,2048);
 check(events.size()==2 && events[0].status==0xfc && events[1].status==0xf8,"Stopped transport still supplies tempo to effects");
 events=block(c,{},48000,256);check(events.size()==1 && events[0].status==0xfc && !c.ownsTransport(),"Host loss handover");
 check(block(c,{},48000,256).empty(),"Standalone must not synthesize clock");
 check(block(c,{true,true,true,std::numeric_limits<double>::quiet_NaN(),0},48000,256).empty(),"Invalid BPM rejected");
 c.reset();events=block(c,{true,true,true,120,-0.02},48000,1000);
 check(events.size()==2 && events[1].offset==480,"Negative preroll PPQ");
 checkLiveHostClock();
 std::cout<<"PASS: 24 PPQN, sample offsets, no drift at 3 rates/5 block sizes, tempo changes, fractional/negative PPQ, seek, stop and host loss\n";
 std::cout<<"PASS: live Host at Stop, tempo changes, DAW grid on Play, negative PPQ phase and external MIDI transport\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
