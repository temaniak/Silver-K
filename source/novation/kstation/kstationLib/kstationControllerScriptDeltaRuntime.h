#pragma once
#include "kstationScriptDeltaTask.h"
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include <optional>
namespace kstation {
class ControllerScriptDeltaRuntime final {
 using T=ScriptDeltaTask;using S=T::Stage;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(2))return false;for(auto a=0x59aeu;a<0x59e6;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!d.protectImmutableVerified(0x59ae,0x59e6,image+0x59ae)||!d.protectImmutableVerified(0x4059ae,0x4059e6,image+0x59ae))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return bool(task_);}bool start(uint32_t source){if(!bound_||active()||source>0xffffff)return false;task_.emplace(source);return true;}uint32_t cursor()const{return cursor_;}uint32_t value()const{return value_;}void reset(){task_.reset();}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{if(bound_!=&d||!irq.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;auto c=cost();return c?c+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned budget){if(bound_!=&d||!irq.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};auto c=cost(),arb=d.pendingIrqs?1u:0u;if(!c||budget<c+arb)return {};if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;task_->advance([&](uint32_t a){d.cycles=begin+c-2;return d.readResource(a);});d.cycles=begin+c;d.lastRead=-1;bool done=task_->complete();if(done){cursor_=task_->cursor();value_=task_->result();task_.reset();}return {c+arb,done,Block::None};}
private:unsigned cost()const{switch(task_->stage()){case S::ReadFirst:case S::ReadSecond:case S::ReadThird:case S::ReadFourth:{auto a=task_->cursor();return 5+((a>=0xfffd10&&a<0xffffa0)?1:a>=0xffffa0?1:3);}case S::BindFactor:return 6;case S::Multiply:return 23;case S::Return:return 7;case S::Complete:return 0;default:return 3;}}ControllerDeviceState*bound_=nullptr;std::optional<T>task_{};uint32_t cursor_=0,value_=0;
};
}
