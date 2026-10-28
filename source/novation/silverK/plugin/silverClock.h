#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace silverK {
enum class ClockSource { Host, Internal, Midi };

// Copy the policy once per audio block. UI changes never touch the running
// clock or send MIDI from the message thread.
inline int firmwareClockSource(ClockSource source,bool hostOwnsTransport){
 return source==ClockSource::Internal?1:source==ClockSource::Midi || hostOwnsTransport?2:0;
}
inline bool acceptsExternalTransport(ClockSource source,bool hostOwnsTransport){
 return source!=ClockSource::Internal && !hostOwnsTransport;
}
// Audio-thread-owned MIDI clock. No storage grows with the block size.
class HostClock {
public:
 struct Position {bool available=false,playing=false,hasPpq=false;double bpm=120,ppq=0;};
 struct Event {uint8_t status;int offset;};
 void reset(){initialized=false;authoritative=false;playing=false;phase=0;}
 void begin(const Position& pos,double rate,int samples){
  count=samples;stopPending=startPending=false;
  active=pos.available && std::isfinite(pos.bpm) && pos.bpm>=1 && pos.bpm<=1000 && std::isfinite(rate) && rate>0;
  if(!active){stopPending=authoritative;initialized=authoritative=playing=false;return;}
  const double beatsPerSample=pos.bpm/(60*rate);
  interval=1/(24*beatsPerSample);
  const bool hasPpq=pos.hasPpq && std::isfinite(pos.ppq);
  const bool seek=initialized && playing && pos.playing && hasPpq && hadPpq && std::abs(pos.ppq-expectedPpq)>std::max(1e-5,2*beatsPerSample);
  if(!initialized || (pos.playing && !playing) || seek){
   stopPending=seek;startPending=pos.playing;
   const double ticks=hasPpq?pos.ppq*24:0;
   phase=(std::ceil(ticks-1e-9)-ticks)*interval;
   if(phase<0)phase=0;
  }else{
   if(playing && !pos.playing)stopPending=true;
   // Preserve fractional tick phase across tempo changes, also on Stop.
   phase*=interval/previousInterval;
  }
  previousInterval=interval;
  initialized=authoritative=true;playing=pos.playing;hadPpq=hasPpq;
  expectedPpq=hasPpq?pos.ppq+(playing?samples*beatsPerSample:0):0;
 }
 bool next(Event& event){
  if(stopPending){stopPending=false;event={0xfc,0};return true;}
  if(startPending){startPending=false;event={0xfa,0};return true;}
  if(!active)return false;
  const auto offset=std::ceil(phase-1e-7);
  if(offset>=count){phase-=count;active=false;return false;}
  event={0xf8,int(offset<0?0:offset)};phase+=interval;return true;
 }
 bool ownsTransport()const{return authoritative;}
 static bool transport(uint8_t status){return status==0xf8 || status==0xfa || status==0xfb || status==0xfc || status==0xf2;}
private:
 bool initialized=false,authoritative=false,active=false,playing=false,hadPpq=false;
 bool stopPending=false,startPending=false;
 int count=0;
 double phase=0,interval=1000,previousInterval=1000,expectedPpq=0;
};
}
