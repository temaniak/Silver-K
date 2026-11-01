#pragma once
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include "kstationVoiceInitTask.h"
#include <optional>
namespace kstation {
class ControllerVoiceInitRuntime final {
 using T=VoiceInitTask;using S=T::Stage;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(2))return false;for(auto a=0x6f50u;a<0x6fde;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!d.protectImmutableVerified(0x6f50,0x6fde,image+0x6f50)||!d.protectImmutableVerified(0x406f50,0x406fde,image+0x6f50))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return bool(task_);}bool start(){if(!bound_||active())return false;task_.emplace();return true;}void reset(){task_.reset();}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{if(bound_!=&d||!irq.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;auto c=task_->nextClocks(credits(d.lastWrite));return c?c+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned budget){if(bound_!=&d||!irq.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};auto cr=credits(d.lastWrite);auto c=task_->nextClocks(cr),arb=d.pendingIrqs?1u:0u;if(!c||budget<c+arb)return {};if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;auto a=address();unsigned n=task_->stage()==S::WriteAge?4:1;task_->advanceBefore(c,cr,[&](T::Read){d.cycles=begin+c;d.lastRead=int(a);return d.readResource(a);},[&](T::Write,uint32_t v){for(unsigned i=0;i<n;++i){d.cycles=begin+c-(n==4?6:3)+(n==4&&i<2?3:(n==4?6:3));d.lastWrite=int(a+i);d.writeResource(a+i,uint8_t(v>>(8*(n-i-1))));}});d.cycles=begin+c;d.lastRead=-1;bool done=task_->complete();if(done)task_.reset();return {c+arb,done,Block::None};}
private:
 static unsigned bus(uint32_t a,unsigned n,int p){unsigned c=0;for(unsigned i=0;i<n;++i){auto x=a+i;if((p&1)||x!=uint32_t(p|1))c+=3;p=int(x);}return c;}
 uint32_t address()const{auto v=task_->voice();switch(task_->stage()){case S::ReadOffset:return task_->offsetAddress();case S::WriteNote:return v+5;case S::WriteKey:return v+13;case S::WriteVelocity:return v+7;case S::WriteAge:return v+8;case S::WriteFlags:return v+17;case S::WriteOffset:return v+14;case S::WriteUpper:return v+15;case S::WriteLower:return v+16;default:return 0;}}
 T::Credits credits(int last)const{auto s=task_->stage();bool rd=s==S::ReadOffset,wr=s==S::WriteNote||s==S::WriteKey||s==S::WriteVelocity||s==S::WriteAge||s==S::WriteFlags||s==S::WriteOffset||s==S::WriteUpper||s==S::WriteLower;return {rd?bus(address(),1,-1):0,wr?bus(address(),s==S::WriteAge?4:1,last):0,7};}
 ControllerDeviceState*bound_=nullptr;std::optional<T>task_{};
};
}
