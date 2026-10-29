#pragma once
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include "kstationControllerDspTransferRuntime.h"
#include "kstationVoiceUpdateTask.h"
#include <optional>
namespace kstation {
class ControllerExpressionValuesRuntime final {
 using T=VoiceExpressionValuesTask;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,DspBusy};
 struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(2))return false;for(uint32_t a=0x5e64;a<0x5ea8;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!d.protectImmutableVerified(0x5e64,0x5ea8,image+0x5e64)||!d.protectImmutableVerified(0x405e64,0x405ea8,image+0x5e64))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}
 bool start(){if(!bound_||active())return false;task_.emplace();return true;}
 bool active()const{return bool(task_);}void reset(){task_.reset();ownsDsp_=false;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerDspTransferRuntime&dsp)const{if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled)||(dsp.active()&&!ownsDsp_))return 0;if(ownsDsp_)return dsp.nextClocks(d,irq);return task_->nextClocks({3,task_->stage()==T::S::Return?7u:10u})+(d.pendingIrqs?1u:0u);}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerDspTransferRuntime&dsp,unsigned budget){if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};if(dsp.active()&&!ownsDsp_)return {0,false,Block::DspBusy};if(ownsDsp_){auto r=dsp.advanceBefore(d,irq,budget);if(r.returned){ownsDsp_=false;task_->resumeDsp();}return {r.clocks,false,Block::None};}auto cost=task_->nextClocks({3,task_->stage()==T::S::Return?7u:10u}),arb=d.pendingIrqs?1u:0u;if(!cost||budget<cost+arb)return {};if(arb)(void)d.read8(0xfffff2);auto stage=task_->stage();auto begin=d.cycles;task_->advanceBefore(cost,{3,stage==T::S::Return?7u:10u},[&](unsigned off){d.cycles=begin+cost;d.lastRead=int(0xbfc0+off);return d.readResource(0xbfc0+off);});if(stage==T::S::DispatchFirst||stage==T::S::DispatchSecond||stage==T::S::DispatchThird){auto address=stage==T::S::DispatchFirst?0x14:stage==T::S::DispatchSecond?0x15:0x16;if(!dsp.startWrite(address,task_->word()))return {0,false,Block::DspBusy};ownsDsp_=true;}d.cycles=begin+cost;d.lastRead=-1;if(stage==T::S::DispatchFirst||stage==T::S::DispatchSecond||stage==T::S::DispatchThird)d.lastWrite=-1;bool done=task_->complete();if(done)task_.reset();return {cost+arb,done,Block::None};}
private:ControllerDeviceState*bound_=nullptr;std::optional<T>task_{};bool ownsDsp_=false;
};
}
