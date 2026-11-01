#pragma once
#include "kstationScriptTempoTask.h"
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include <optional>
namespace kstation {
class ControllerScriptTempoRuntime final {
 using T=ScriptTempoTask;using S=T::Stage;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(2))return false;for(auto a=0x5a74u;a<0x5aa0;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!d.protectImmutableVerified(0x5a74,0x5aa0,image+0x5a74)||!d.protectImmutableVerified(0x405a74,0x405aa0,image+0x5a74))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return bool(task_);}bool start(uint32_t input){if(!bound_||active())return false;task_.emplace(input);return true;}void reset(){task_.reset();}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{if(bound_!=&d||!irq.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;auto c=cost(d.lastWrite);return c?c+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned budget){if(bound_!=&d||!irq.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};auto c=cost(d.lastWrite),arb=d.pendingIrqs?1u:0u;if(!c||budget<c+arb)return {};if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;task_->advance([&]{d.cycles=begin+c;return uint16_t((d.readResource(0xd01a)<<8)|d.readResource(0xd01b));},[&](uint32_t v){for(unsigned i=0;i<4;++i){d.cycles=begin+c-(i<2?1:0);d.writeResource(0xfffe5e +i,uint8_t(v>>(8*(3-i))));d.lastWrite=0xfffe5e +i;}});d.cycles=begin+c;d.lastRead=-1;bool done=task_->complete();if(done)task_.reset();return {c+arb,done,Block::None};}
private:unsigned cost(int p)const{switch(task_->stage()){case S::ReadDivision:return 12;case S::BindScale:case S::BindFactor:case S::BindClock:return 6;case S::MultiplyScale:case S::MultiplyClock:case S::Divide:return 23;case S::Write:return 12+(((p&1)||0xfffe5e!=uint32_t(p|1))?2:1);case S::Return:return 7;case S::Complete:return 0;default:return 3;}}ControllerDeviceState*bound_=nullptr;std::optional<T>task_{};
};
}
