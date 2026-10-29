#pragma once
#include "kstationScriptSettingsTask.h"
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include <optional>
namespace kstation {
class ControllerScriptSettingsRuntime final {
 using T=ScriptSettingsTask;using S=T::Stage;
public:using Kind=T::Kind;enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(2))return false;for(auto a=0x55d8u;a<0x563a;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!d.protectImmutableVerified(0x55d8,0x563a,image+0x55d8)||!d.protectImmutableVerified(0x4055d8,0x40563a,image+0x55d8))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return bool(task_);}bool start(Kind k){if(!bound_||active())return false;task_.emplace(k);return true;}void reset(){task_.reset();}unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{if(bound_!=&d||!irq.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;auto c=cost(d.lastWrite);return c?c+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned budget){if(bound_!=&d||!irq.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};auto c=cost(d.lastWrite),arb=d.pendingIrqs?1u:0u;if(!c||budget<c+arb)return {};if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;task_->advance([&](uint32_t a){d.cycles=begin+c-2;return d.readResource(a);},[&](uint32_t a,uint8_t v){d.cycles=begin+c;d.writeResource(a,v);d.lastWrite=int(a);});d.cycles=begin+c;d.lastRead=-1;bool done=task_->complete();if(done)task_.reset();return {c+arb,done,Block::None};}
private:static unsigned bus(uint32_t a,int p){return (p&1)||a!=uint32_t(p|1)?3:0;}unsigned cost(int p)const{switch(task_->stage()){case S::BindSource:case S::BindTarget:case S::BindSecondSource:case S::BindSecondTarget:case S::BindProgram:return 9;case S::BindCount:case S::BindSecondCount:return 6;case S::Read:case S::ReadSecond:return 5+bus(task_->source(),-1);case S::Write:case S::WriteSecond:return 3+bus(task_->target(),p);case S::WriteFlag:return 12+bus(0xbb1c,p);case S::Return:return 7;case S::Complete:return 0;default:return 3;}}ControllerDeviceState*bound_=nullptr;std::optional<T>task_{};
};
}
