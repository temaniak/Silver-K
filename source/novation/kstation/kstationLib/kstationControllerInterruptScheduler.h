#pragma once
#include "kstationControllerInterruptTask.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace kstation {
// Mode-0 priority is fixed by the lowest pending vector. This scheduler owns
// semantic continuations only; vector addresses are consumed at metadata bind.
class ControllerInterruptScheduler final {
public:
 enum class Kind:uint8_t{Unused,Timer1,Timer3,SerialError,SerialReceive,SerialTransmit,Adc,Unsupported};
 enum class Mask:uint8_t{Enabled,Disabled};
 enum class Admit:uint8_t{None,NeedSyscr,Masked,NotBound,UnsupportedMode,Unsupported,Capacity,Entered};
 struct Selection{int vector=-1;Kind kind=Kind::Unsupported;};
 struct Admission{Admit result=Admit::None;int vector=-1;Kind kind=Kind::Unsupported;};
 static constexpr unsigned Capacity=8;
 bool bindVerifiedVectors(const uint8_t* image,size_t size){if(bound_||!image||size<0x142||depth_)return false;if(image[0x140]!=0x56||image[0x141]!=0x70)return false;std::array<Kind,64> next{};for(unsigned i=0;i<64;++i){uint32_t target=(uint32_t(image[4*i])<<24)|(uint32_t(image[4*i+1])<<16)|(uint32_t(image[4*i+2])<<8)|image[4*i+3];target&=0xffffff;const uint32_t expected=i==0?0x142:i>=61?0:i==28?0x968:i==38?0x92c:i==52?0xa50:i==53?0xa64:i==54?0xb50:i==60?0x670:0x140;if(target!=expected)return false;Kind k=Kind::Unsupported;if(target==0x140)k=Kind::Unused;else if(i==28&&target==0x968)k=Kind::Timer1;else if(i==38&&target==0x92c)k=Kind::Timer3;else if(i==52&&target==0xa50)k=Kind::SerialError;else if(i==53&&target==0xa64)k=Kind::SerialReceive;else if(i==54&&target==0xb50)k=Kind::SerialTransmit;else if(i==60&&target==0x670)k=Kind::Adc;next[i]=k;}if(next[28]!=Kind::Timer1||next[38]!=Kind::Timer3||next[52]!=Kind::SerialError||next[53]!=Kind::SerialReceive||next[54]!=Kind::SerialTransmit||next[60]!=Kind::Adc)return false;vectors_=next;bound_=true;return true;}
 static bool liveVectorMatches(Kind kind,uint32_t target){target&=0xffffff;switch(kind){case Kind::Unused:return (target&0xfffffe)==0x140;case Kind::Timer1:return target==0x968;case Kind::Timer3:return target==0x92c;case Kind::SerialError:return target==0xa50;case Kind::SerialReceive:return target==0xa64;case Kind::SerialTransmit:return target==0xb50;case Kind::Adc:return target==0x670;case Kind::Unsupported:return false;}return false;}
 bool bound()const{return bound_;}unsigned depth()const{return depth_;}bool active()const{return depth_!=0;}Mask mask()const{return mask_;}void setMask(Mask m){mask_=m;}void reset(){for(auto&f:jobs_)f.reset();depth_=0;mask_=Mask::Enabled;}
 Selection select(uint64_t pending)const{if(!pending)return {};unsigned v=0;while(!(pending&1)){pending>>=1;++v;}return {int(v),bound_?vectors_[v]:Kind::Unsupported};}
 Admission admit(uint64_t&pending,bool syscrObserved,bool mode2=false,uint64_t high=0){if(mode2||high)return {Admit::UnsupportedMode};if(!pending)return {};if(!syscrObserved)return {Admit::NeedSyscr};if(!bound_)return {Admit::NotBound};if(mask_==Mask::Disabled)return {Admit::Masked};auto s=select(pending);if(s.kind==Kind::Unsupported)return {Admit::Unsupported,s.vector,s.kind};if(depth_==Capacity)return {Admit::Capacity,s.vector,s.kind};auto before=mask_;mask_=Mask::Disabled;pending&=~(uint64_t(1)<<s.vector);jobs_[depth_++].emplace(Job{s.kind,before});return {Admit::Entered,s.vector,s.kind};}
 Kind current()const{return active()?jobs_[depth_-1]->kind:Kind::Unsupported;}bool complete(){if(!active())return false;mask_=jobs_[depth_-1]->previous;jobs_[--depth_].reset();return true;}
private:struct Job{Kind kind;Mask previous;};std::array<Kind,64>vectors_{};std::array<std::optional<Job>,Capacity>jobs_{};unsigned depth_=0;Mask mask_=Mask::Enabled;bool bound_=false;
};
}
