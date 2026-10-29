#pragma once
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include "kstationControllerDspTransferRuntime.h"
#include "kstationControllerVoiceDspWordTask.h"
#include "kstationVoiceGlobalTask.h"
#include <optional>
namespace kstation {
class ControllerVoiceGlobalLevelsRuntime final {
 using T=VoiceGlobalLevelsTask;using S=T::Stage;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,DspBusy};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(4))return false;for(auto r:ranges_)for(auto a=r.begin;a<r.end;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;for(auto r:ranges_)if(!d.protectImmutableVerified(r.begin,r.end,image+r.begin)||!d.protectImmutableVerified(0x400000+r.begin,0x400000+r.end,image+r.begin))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return bool(task_);}bool start(uint32_t patch){if(!bound_||active())return false;task_.emplace(patch);return true;}void reset(){task_.reset();word_.reset();ownsDsp_=false;}uint32_t result()const{return result_;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerDspTransferRuntime&dsp)const{if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled)||(dsp.active()&&!ownsDsp_))return 0;if(ownsDsp_)return dsp.nextClocks(d,irq);auto c=word_?word_->nextClocks():cost();return c?c+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerDspTransferRuntime&dsp,unsigned budget){if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};if(dsp.active()&&!ownsDsp_)return {0,false,Block::DspBusy};if(ownsDsp_){auto r=dsp.advanceBefore(d,irq,budget);if(r.returned){ownsDsp_=false;word_->resumeDsp();}return {r.clocks,false,Block::None};}auto c=word_?word_->nextClocks():cost(),arb=d.pendingIrqs?1u:0u;if(!c||budget<c+arb)return {};if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;auto read=[&](uint32_t a,unsigned n,unsigned end){uint32_t v=0;unsigned total=n==4?6:3;for(unsigned i=0;i<n;++i){d.cycles=begin+end-total+(n==4&&i<2?3:total);d.lastRead=int(a+i);v=(v<<8)|d.readResource(a+i);}return v;};bool stack=false;if(word_){stack=word_->stackWrites();word_->advanceBefore(c,read);if(word_->awaitingDsp()){if(!dsp.startWrite(word_->address(),word_->word()))return {};ownsDsp_=true;}else if(word_->complete()){word_.reset();task_->resume();}}else{auto stage=task_->stage();stack=stage==S::Send;task_->advance([&](uint32_t a){return read(a,1,c);});if(task_->awaitingChild()){result_=task_->value();word_.emplace(uint16_t(0x3e3+task_->lane()),(result_>>8)&0xffffff);}}d.cycles=begin+c;d.lastRead=-1;if(stack)d.lastWrite=-1;bool done=task_->complete();if(done)task_.reset();return {c+arb,done,Block::None};}
private:
 struct Range{uint32_t begin,end;};inline static constexpr Range ranges_[]={{0x7e7c,0x7edc},{0x5bec,0x5c04}};
 unsigned cost()const{switch(task_->stage()){case S::Read:return 15;case S::Register:return 6;case S::Send:return 10;case S::AwaitChild:case S::Complete:return 0;default:return 3;}}
 ControllerDeviceState*bound_=nullptr;std::optional<T>task_{};std::optional<ControllerVoiceDspWordTask>word_{};bool ownsDsp_=false;uint32_t result_=0;
};
}
