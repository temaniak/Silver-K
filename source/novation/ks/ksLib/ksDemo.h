#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ks {
// OS 2.1.02 reads a format-0 SMF at Flash 0x1a000, outside user presets.
// Check the whole track before exposing it to the firmware's unbounded reader.
inline bool validDemoSong(const uint8_t* flash,size_t size){
 constexpr size_t base=0x1a000,end=0x1fc00;
 if(size<end || std::memcmp(flash+base,"MThd\0\0\0\6\0\0\0\1",12) ||
    std::memcmp(flash+base+14,"MTrk",4))return false;
 const auto* p=flash+base;
 const unsigned division=(unsigned(p[12])<<8)|p[13];
 if(!division || division>=0x8000)return false;
 const uint32_t length=(uint32_t(p[18])<<24)|(uint32_t(p[19])<<16)|(uint32_t(p[20])<<8)|p[21];
 if(length<4 || length>end-base-22)return false;
 size_t cursor=base+22;const size_t limit=cursor+length;uint8_t running=0;
 auto vlq=[&](uint32_t& value){value=0;for(unsigned n=0;n<4 && cursor<limit;++n){const auto b=flash[cursor++];value=(value<<7)|(b&127);if(!(b&128))return true;}return false;};
 while(cursor<limit){
  uint32_t delta;if(!vlq(delta) || cursor>=limit)return false;
  auto status=flash[cursor];if(status>=128)++cursor;else status=running;
  if(status>=0x80 && status<0xf0){
   running=status;const unsigned count=(status&0xe0)==0xc0?1:2;
   if(limit-cursor<count)return false;
   for(unsigned n=0;n<count;++n)if(flash[cursor++]>=128)return false;
  }else if(status==0xff){
   if(cursor>=limit)return false;const auto type=flash[cursor++];uint32_t count;
   // The original handler skips unknown meta lengths as one byte.
   if(!vlq(count) || count>=128 || count>limit-cursor)return false;
   if(type==0x2f)return count==0 && cursor==limit;
   if(type==0x51){
    if(count!=3)return false;
    const uint32_t tempo=(uint32_t(flash[cursor])<<16)|(uint32_t(flash[cursor+1])<<8)|flash[cursor+2];
    if(!((tempo>>4)&0xffff))return false; // original tempo division must be nonzero
   }
   cursor+=count;
  }else if(status==0xf0 || status==0xf7){
   uint32_t count;if(!vlq(count) || !count || count>limit-cursor)return false;cursor+=count;running=0;
  }else return false;
 }
 return false;
}
}
