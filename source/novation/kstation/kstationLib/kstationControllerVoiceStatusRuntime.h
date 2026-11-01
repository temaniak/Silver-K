#pragma once
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include "kstationControllerDspTransferRuntime.h"
#include "kstationControllerVoiceStatusTask.h"
#include <optional>
namespace kstation {
class ControllerVoiceStatusRuntime final {
 using T=ControllerVoiceStatusTask;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,DspBusy};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(2))return false;for(auto a=0x837cu;a<0x8394;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!d.protectImmutableVerified(0x837c,0x8394,image+0x837c)||!d.protectImmutableVerified(0x40837c,0x408394,image+0x837c))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return bool(task_);}bool start(){if(!bound_||active())return false;task_.emplace();return true;}void reset(){task_.reset();ownsDsp_=false;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerDspTransferRuntime&dsp)const{if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled)||(dsp.active()&&!ownsDsp_))return 0;if(ownsDsp_)return dsp.nextClocks(d,irq);auto c=task_->nextClocks();return c?c+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerDspTransferRuntime&dsp,unsigned budget){if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};if(dsp.active()&&!ownsDsp_)return {0,false,Block::DspBusy};if(ownsDsp_){auto r=dsp.advanceBefore(d,irq,budget);if(r.returned){ownsDsp_=false;task_->resumeDsp();}return {r.clocks,false,Block::None};}auto c=task_->nextClocks(),arb=d.pendingIrqs?1u:0u;if(!c||budget<c+arb)return {};if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;auto stack=task_->stackWrites();task_->advanceBefore(c,[&](uint32_t a,unsigned,unsigned end){d.cycles=begin+end;d.lastRead=int(a);return d.readResource(a);},[&](uint32_t a,uint32_t v,unsigned,unsigned end){d.cycles=begin+end;d.lastWrite=int(a);d.writeResource(a,uint8_t(v));});if(task_->awaitingDsp()){if(!dsp.startWrite(task_->address(),task_->word()))return {};ownsDsp_=true;}d.cycles=begin+c;d.lastRead=-1;if(stack)d.lastWrite=-1;bool done=task_->complete();if(done)task_.reset();return {c+arb,done,Block::None};}
private:ControllerDeviceState*bound_=nullptr;std::optional<T>task_{};bool ownsDsp_=false;
};
}
