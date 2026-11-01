#pragma once
#include "kstationStartupServicesTask.h"
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include "kstationControllerDspTransferRuntime.h"
#include <optional>
#include <array>
namespace kstation {
class ControllerStartupVolumeRuntime final {
 using T=StartupVolumeTask;using S=T::Stage;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,DspBusy};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};struct Range{uint32_t begin,end;};inline static constexpr std::array<Range,1>CodeRanges{{{0x403c,0x4054}}};
 bool bindBootResources(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(1))return false;for(uint32_t a=0x403c;a<0x4054;++a)if(d.readResource(a)!=image[a])return false;if(!d.protectImmutableVerified(0x403c,0x4054,image+0x403c))return false;bound_=&d;return true;}
 bool bindMirroredFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_!=&d||mirrored_||active()||!image||size!=49728||!d.hasImmutableCapacity(1))return false;for(uint32_t a=0x403c;a<0x4054;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!d.protectImmutableVerified(0x40403c,0x404054,image+0x403c))return false;mirrored_=true;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return task_.has_value();}void reset(){task_.reset();ownsDsp_=false;}bool start(uint8_t value){if(!bound_||active())return false;task_.emplace(value);return true;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerDspTransferRuntime&dsp)const{if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d)||!task_||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled)||(dsp.active()&&!ownsDsp_))return 0;if(ownsDsp_)return dsp.nextClocks(d,irq);FlashCredits c;auto n=task_->nextClocks(c);return n?n+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerDspTransferRuntime&dsp,unsigned budget){if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d))return {0,false,Block::NotBound};if(!task_)return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};if(dsp.active()&&!ownsDsp_)return {0,false,Block::DspBusy};if(ownsDsp_){auto r=dsp.advanceBefore(d,irq,budget);if(r.returned){ownsDsp_=false;task_->resume();}return {r.clocks,false,r.blocked==ControllerDspTransferRuntime::Block::NeedsInterrupt?Block::NeedsInterrupt:Block::None};}FlashCredits c;auto total=nextClocks(d,irq,dsp);if(!total||budget<total)return {};auto arb=d.pendingIrqs?1u:0u;if(arb)(void)d.read8(0xfffff2);auto clocks=total-arb;auto stage=task_->stage();if(task_->advanceBefore(clocks,c)!=clocks)return {};d.cycles+=clocks;d.lastRead=-1;if(stage==S::Retain||stage==S::Send)d.lastWrite=-1;if(task_->awaiting()){if(!dsp.startWrite(0x13,task_->word()))return {0,false,Block::DspBusy};ownsDsp_=true;}bool done=task_->complete();if(done)task_.reset();return {total,done,Block::None};}
private:ControllerDeviceState*bound_=nullptr;bool mirrored_=false,ownsDsp_=false;std::optional<T>task_{};
};
}
