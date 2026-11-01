#pragma once
#include "ksNativeMonoArpCoordinator.h"
#include "ksMidiFileTimeAbi.h"
#include "ksNativeMonoArpBootstrapAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
// External Stage3 bookkeeping. No native service owns or reads this state.
class NativeMonoArpCompatibility final {
public:
 struct Image{std::array<uint32_t,7>values{};uint8_t flags=0;};
 NativeMonoArpCompatibility()=default;template<class A>explicit NativeMonoArpCompatibility(const A&a){replaceImage(a);}template<class A>void replaceImage(const A&a){image_.values=a.values;image_.flags=a.flags;}const Image&image()const{return image_;}
 void operator()(NativeMonoArpCoordinator::Notice notice,MonoArpTask::Stage stage,const NativeMonoArpCoordinator&s,const MonoArpTask&t){using N=NativeMonoArpCoordinator::Notice;using S=MonoArpTask::Stage;auto&v=image_.values;if(notice==N::Lookup){v[0]=0x160d2u+334*s.lookup()->index();image_.flags&=0xd0;return;}if(notice==N::Time){projectMidiFileTime(image_,*s.time());return;}if(notice==N::Output){projectNativeMonoArpBootstrap(image_,*s.output());return;}
 const auto low=[&](unsigned r,uint8_t x){v[r]=(v[r]&0xffffff00u)|x;nz(x,8);};const auto half=[&](unsigned r,uint16_t x){v[r]=(v[r]&0xffff0000u)|x;nz(x,16);};const auto top=[&](unsigned r,uint16_t x){v[r]=(v[r]&0xffffu)|(uint32_t(x)<<16);nz(x,16);};const auto full=[&](unsigned r,uint32_t x){v[r]=x;nz(x,32);};
 switch(stage){case S::ServiceReadStream:full(4,t.stream());break;case S::ServiceReadDeadline:full(1,s.result().deadline);break;case S::ServiceReadPhase:full(2,s.result().phase);break;case S::ServiceCapturePhase:top(3,uint16_t(v[2]>>16));break;case S::ServiceCompareSigns:top(3,uint16_t((v[3]^v[1])>>16));break;case S::ServiceThreshold:top(3,0xc000);break;case S::ServiceCompareDeadline:image_.flags=controlArithmetic::subtract(int16_t(v[1]>>16),int16_t(v[3]>>16),image_.flags).flags;break;case S::ServiceComparePhase:image_.flags=controlArithmetic::subtract(int16_t(v[2]>>16),int16_t(v[3]>>16),image_.flags).flags;break;case S::ServiceCompareTime:image_.flags=controlArithmetic::subtract(int32_t(v[2]),int32_t(v[1]),image_.flags).flags;break;
 case S::ServiceRetainPart:serviceRecord_=v[0];nz(v[0],32);break;case S::ServiceRestorePart:full(0,serviceRecord_);break;case S::ServicePublishCursor:nz(v[4],32);break;
 case S::EventRead:case S::EventReadData:case S::MetaRead:low(1,t.byte());v[4]=t.stream();break;case S::EventCompareMeta:compare8(uint8_t(v[1]),255);break;case S::EventCompareStart:compare8(uint8_t(v[1]),240);break;case S::EventCompareContinue:compare8(uint8_t(v[1]),247);break;case S::EventObserveStatus:nz(uint8_t(v[1]),8);break;case S::EventReadStatusCount:case S::EventReadDataCount:low(1,s.result().remaining);break;case S::EventRewind:v[4]=t.stream();break;case S::EventRetainCount:eventCount_=uint16_t(v[1]);nz(eventCount_,16);break;case S::EventReleaseCount:half(1,eventCount_);break;case S::EventTickCount:{const auto old=int32_t(int8_t(v[1]));const auto value=uint32_t(old)-1;nz(value,32);image_.flags=uint8_t((image_.flags&~2u)|((uint32_t(old)&~value&128u)?2:0));v[1]=(v[1]&0xffffff00u)|uint8_t(value);break;}
 case S::SendRetainPart:sendRecord_=v[0];nz(v[0],32);break;case S::SendRetainStream:sendStream_=v[4];nz(v[4],32);break;case S::SendBindByte:low(6,t.byte());break;case S::SendRestoreStream:full(4,sendStream_);break;case S::SendRestorePart:full(0,sendRecord_);break;
 case S::MetaCompareEnd:compare8(uint8_t(v[1]),47);break;case S::MetaClearSkip:image_.flags=controlArithmetic::subtract(int32_t(v[1]),int32_t(v[1]),image_.flags).flags;v[1]=0;break;case S::MetaReadSkip:low(1,s.result().skip);v[4]=t.stream();break;case S::MetaSkip:{const auto r=controlArithmetic::add(int32_t(v[4]),int32_t(v[1]),image_.flags);v[4]=uint32_t(r.value);image_.flags=r.flags;break;}case S::MetaReadStream:full(4,t.stream());break;default:break;}
 }
private:
 void compare8(uint8_t a,uint8_t b){image_.flags=controlArithmetic::subtract(int8_t(a),int8_t(b),image_.flags).flags;}
 void nz(uint32_t v,unsigned bits){const auto mask=bits==32?~0u:(uint32_t(1)<<bits)-1;v&=mask;image_.flags=uint8_t((image_.flags&0xf1)|(v&(uint32_t(1)<<(bits-1))?8:0)|(!v?4:0));}
 Image image_{};uint32_t serviceRecord_=0,sendRecord_=0,sendStream_=0;uint16_t eventCount_=0;
};
}
