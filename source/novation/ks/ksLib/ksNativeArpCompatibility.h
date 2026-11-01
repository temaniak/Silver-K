#pragma once
#include "ksNativeArpService.h"
#include "ksNativeArpMainCompatibility.h"
#include "ksPanelMasksAbi.h"
namespace ks {
// Temporary outer compatibility accounting, outside NativeArpService and its
// children. Its image is applied once, after the complete owned transaction.
class NativeArpCompatibility final {
public:
 using Image=NativeArpMainCompatibility::Image;
 template<class A>NativeArpCompatibility(NativeArpService::Kind k,const A&a):kind_(k){replaceImage(a);if(k==NativeArpService::Kind::Main)main_.emplace(image_);}
 const Image&image()const{return image_;}template<class A>void replaceImage(const A&a){image_.values=a.values;image_.flags=a.flags;if(main_)main_->replaceImage(a);}
 void operator()(NativeArpMainService::Notice n,ArpMainTask::Stage stage,const NativeArpMainService&s){if(!main_)throw std::logic_error("Missing external arp part image");(*main_)(n,stage,s);image_=main_->image();}
 void operator()(NativeMonoArpCoordinator::Notice n,MonoArpTask::Stage stage,const NativeMonoArpCoordinator&s,const MonoArpTask&t){if(!main_)throw std::logic_error("Missing external arp mono image");(*main_)(n,stage,s,t);image_=main_->image();}
 void operator()(NativeArpService::Notice notice,ArpScheduleTask::Stage stage,const NativeArpService&s){using N=NativeArpService::Notice;using S=ArpScheduleTask::Stage;auto&v=image_.values;const auto&t=s.schedule();if(notice==N::PartBegin){main_.emplace(image_);return;}if(notice==N::PartComplete){if(main_)image_=main_->image();return;}if(notice==N::DrainComplete){projectPanelMasks(image_,*s.masks());return;}if(notice!=N::Policy)return;
 const auto low=[&](unsigned r,uint8_t x){v[r]=(v[r]&0xffffff00u)|x;nz(x,8);};const auto high=[&](unsigned r,uint8_t x){v[r]=(v[r]&0xffff00ffu)|(uint32_t(x)<<8);nz(x,8);};const auto half=[&](unsigned r,uint16_t x){v[r]=(v[r]&0xffff0000u)|x;nz(x,16);};
 switch(stage){case S::ReadMask:low(0,t.mask());break;case S::PublishMask:nz(t.mask(),8);break;case S::ClearResult:image_.flags=controlArithmetic::subtract(int16_t(v[2]),int16_t(v[2]),image_.flags).flags;v[2]&=0xffff0000u;break;case S::ClearIndex:low(0,0);break;case S::SinglePart:high(0,1);break;case S::ReadMode:low(1,t.mode());break;case S::FirstMultiPart:low(0,1);break;case S::MultiParts:high(0,4);break;case S::PublishPart:nz(t.index(),8);break;case S::PublishWork:nz(0,8);break;case S::RetainParts:retainedParts_=uint16_t(v[0]);nz(retainedParts_,16);break;case S::RetainResult:retainedResult_=uint16_t(v[2]);nz(retainedResult_,16);break;case S::ReleaseResult:half(2,retainedResult_);break;case S::ReleaseParts:half(0,retainedParts_);break;case S::ReadWork:low(1,t.work());break;case S::CaptureIndex:low(1,t.index());break;case S::ReadCurrentMode:high(1,t.currentMode());break;case S::LocateResult:{auto r=controlArithmetic::subtract(int8_t(v[1]),int8_t(v[1]>>8),image_.flags);v[1]=(v[1]&0xffffff00u)|uint8_t(r.value);image_.flags=r.flags;break;}case S::MarkResult:v[2]=(v[2]&0xffff00ffu)|(uint32_t(t.result())<<8);break;case S::ReadCurrentMask:low(1,t.mask());break;case S::ShiftMask:{const auto x=uint8_t(v[1]);image_.flags=uint8_t((image_.flags&~1u)|(x&1));low(1,uint8_t(x>>1));break;}case S::PublishShiftedMask:nz(t.mask(),8);break;case S::TickPart:{const auto x=inc(uint8_t(v[0]),1);v[0]=(v[0]&0xffffff00u)|x;break;}case S::TickParts:{const auto x=inc(uint8_t(v[0]>>8),-1);v[0]=(v[0]&0xffff00ffu)|(uint32_t(x)<<8);break;}default:break;}
 }
private:
 uint8_t inc(uint8_t raw,int delta){const int32_t old=int8_t(raw);const uint32_t value=uint32_t(old)+uint32_t(delta);nz(value,32);const bool overflow=(delta>0?(~uint32_t(old)&value):(uint32_t(old)&~value))&128u;image_.flags=uint8_t((image_.flags&~2u)|(overflow?2:0));return uint8_t(value);}
 void nz(uint32_t v,unsigned bits){const auto mask=bits==32?~0u:(uint32_t(1)<<bits)-1;v&=mask;image_.flags=uint8_t((image_.flags&0xf1)|(v&(uint32_t(1)<<(bits-1))?8:0)|(!v?4:0));}
 NativeArpService::Kind kind_;Image image_{};std::optional<NativeArpMainCompatibility>main_;uint16_t retainedParts_=0,retainedResult_=0;
};
}
