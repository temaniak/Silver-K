#pragma once
#include "ksMidiFileEventService.h"
#include "ksMidiFileTimeAbi.h"
#include "ksNativeMidiDecodeAbi.h"
#include "ksControlArithmetic.h"
#include <array>
namespace ks {
// Temporary EXTERNAL compatibility sink, removed with Stage3's outer ABI.
// It is never owned or read by a native event/decoder/arp service. The optional
// void notifications collect completion accounting; only the outer caller may
// apply this image. Disabling this sink cannot change native execution.
class MidiFileEventCompatibility final {
public:
struct Image{std::array<uint32_t,7>values{};uint8_t flags=0;};
MidiFileEventCompatibility()=default;template<class A>explicit MidiFileEventCompatibility(const A&a){image_.values=a.values;image_.flags=a.flags;}
const Image&image()const{return image_;}
void operator()(MidiFileEventService::Notice n,const MidiFileEventService&s){if(n==MidiFileEventService::Notice::DecodeComplete){if(!s.decoderOutcome())throw std::logic_error("Missing completed file decoder accounting");projectNativeMidiDecode(image_,*s.decoderOutcome());}}
void operator()(MidiFileEventCoordinator::Notice n,MidiFileEventTask::Stage stage,const MidiFileEventCoordinator&s,const MidiFileEventTask&t){
if(n==MidiFileEventCoordinator::Notice::TimeComplete){if(!s.timeOutcome())throw std::logic_error("Missing completed file time accounting");projectMidiFileTime(image_,*s.timeOutcome());return;}
using S=MidiFileEventTask::Stage;auto&v=image_.values;const auto low=[&](unsigned x,uint8_t b){v[x]=(v[x]&0xffffff00u)|b;nz(b,8);};const auto high=[&](unsigned x,uint8_t b){v[x]=(v[x]&0xffff00ffu)|(uint32_t(b)<<8);nz(b,8);};const auto word=[&](unsigned x,uint16_t b){v[x]=(v[x]&0xffff0000u)|b;nz(b,16);};const auto full=[&](unsigned x,uint32_t b){v[x]=b;nz(b,32);};
switch(stage){
case S::EventRead:case S::EventReadData:case S::MetaRead:case S::MetaReadSkip:case S::TempoReadLength:case S::TempoReadHigh:case S::TempoReadLow:case S::ContinueRead:low(0,uint8_t(s.result().record));v[4]=t.stream();break;
case S::TempoReadMiddle:high(0,uint8_t(s.result().record>>8));v[4]=t.stream();break;
case S::EventCompareMeta:compare8(uint8_t(v[0]),255);break;case S::EventCompareStart:compare8(uint8_t(v[0]),240);break;case S::EventCompareContinue:compare8(uint8_t(v[0]),247);break;case S::EventObserveStatus:nz(uint8_t(v[0]),8);break;
case S::EventReadStatusCount:case S::EventReadDataCount:low(0,uint8_t(t.remaining()));break;case S::EventRewind:v[4]=t.stream();break;case S::EventRetainCount:eventCount_=uint16_t(v[0]);nz(eventCount_,16);break;case S::EventReleaseCount:word(0,eventCount_);break;case S::EventTickCount:{const auto value=increment8(uint8_t(v[0]),-1);v[0]=(v[0]&0xffffff00u)|value;break;}
case S::MetaBindTable:full(1,0x11a3e);break;case S::MetaCount:low(2,10);break;case S::MetaReadCandidate:high(0,uint8_t(s.result().record>>8));break;case S::MetaCompare:compare8(uint8_t(v[0]>>8),uint8_t(v[0]));break;case S::MetaAdvance:{const auto r=controlArithmetic::add(int32_t(v[1]),int32_t(6),image_.flags);v[1]=uint32_t(r.value);image_.flags=r.flags;break;}case S::MetaTick:{const auto value=increment8(uint8_t(v[2]),-1);v[2]=(v[2]&0xffffff00u)|value;break;}
case S::MetaClearSkip:image_.flags=controlArithmetic::subtract(int32_t(v[0]),int32_t(v[0]),image_.flags).flags;v[0]=0;break;
case S::MetaSkip:case S::SkipAdvance:{const auto delta=stage==S::MetaSkip?v[0]:v[5];const auto r=controlArithmetic::add(int32_t(v[4]),int32_t(delta),image_.flags);v[4]=uint32_t(r.value);image_.flags=r.flags;break;}
case S::MetaReadHandler:full(0,s.result().record);break;case S::TempoNormalize:word(0,uint16_t(v[0]&255));break;case S::TempoPositionHigh:v[0]=(v[0]&65535u)|(v[0]<<16);nz(uint16_t(v[0]>>16),16);break;
case S::EndClear:low(0,0);break;case S::EndPublish:nz(uint8_t(v[0]),8);break;case S::StartStatus:low(0,240);break;
case S::ContinueRetainCount:continueCount_=v[5];nz(continueCount_,32);break;case S::ContinueReleaseCount:full(5,continueCount_);break;case S::ContinueTick:{const auto old=v[5],value=old-1;nz(value,32);image_.flags=uint8_t((image_.flags&~2u)|((old&~value&0x80000000u)?2:0));v[5]=value;break;}
case S::SendRetainStream:sendStream_=v[4];nz(sendStream_,32);break;case S::SendBindByte:low(6,t.byte());break;case S::SendReleaseStream:full(4,sendStream_);break;default:break;}
}
private:
void nz(uint32_t value,unsigned width){const auto sign=uint32_t(1)<<(width-1);const auto mask=width==32?~0u:(uint32_t(1)<<width)-1;value&=mask;image_.flags=uint8_t((image_.flags&0xf1)|(value&sign?8:0)|(!value?4:0));}
void compare8(uint8_t a,uint8_t b){image_.flags=controlArithmetic::subtract(int8_t(a),int8_t(b),image_.flags).flags;}
uint8_t increment8(uint8_t raw,int delta){const auto old=int32_t(int8_t(raw)),value=int32_t(uint32_t(old)+uint32_t(delta));nz(uint32_t(value),32);const bool overflow=(delta>0?(~uint32_t(old)&uint32_t(value)):(uint32_t(old)&~uint32_t(value)))&128;image_.flags=uint8_t((image_.flags&~2u)|(overflow?2:0));return uint8_t(value);}
Image image_{};uint32_t continueCount_=0,sendStream_=0;uint16_t eventCount_=0;
};}
