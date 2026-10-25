#pragma once
#include "ksMidiPacketService.h"
namespace ks {
// Final external caller projection; never used to select a packet phase.
template<class A>void projectMidiPacket(A&a,const MidiPacketService&s){
 if(!s.complete()||!s.summary().bytes)throw std::logic_error("Incomplete MIDI packet projection");
 const auto&v=s.summary();const auto&last=v.output[v.bytes-1];
 if(v.kind==MidiPacketService::Kind::Panic)a.values[0]=v.controlAddress;
 a.values[6]=(a.values[6]&0xffff0000u)|v.event;
 const uint16_t next=last.serial?uint16_t((last.producer+1)&255):last.producer;
 const int32_t restored=int32_t(a.values[0]);
 a.flags=uint8_t((a.flags&0xd0)|(restored<0?8:0)|(!restored?4:0)|(last.consumer<next?1:0)|((last.consumer&0xfff)<(next&0xfff)?32:0));
}
}
