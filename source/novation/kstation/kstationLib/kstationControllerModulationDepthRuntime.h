#pragma once
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include "kstationControllerDspTransferRuntime.h"
#include "kstationControllerVoiceDspWordTask.h"
#include "kstationVoiceUpdateTask.h"
#include <optional>
namespace kstation {
class ControllerModulationDepthRuntime final {
 using T=VoiceModulationDepthTask;using S=T::Stage;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,DspBusy};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(4))return false;for(auto r:ranges_)for(auto a=r.begin;a<r.end;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;for(auto r:ranges_)if(!d.protectImmutableVerified(r.begin,r.end,image+r.begin)||!d.protectImmutableVerified(0x400000+r.begin,0x400000+r.end,image+r.begin))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return bool(task_);}bool start(uint32_t patch){if(!bound_||active())return false;task_.emplace(patch);return true;}void reset(){task_.reset();word_.reset();ownsDsp_=false;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerDspTransferRuntime&dsp)const{if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled)||(dsp.active()&&!ownsDsp_))return 0;if(ownsDsp_)return dsp.nextClocks(d,irq);return (word_?word_->nextClocks():task_->nextClocks(credits()))+(d.pendingIrqs?1u:0u);}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerDspTransferRuntime&dsp,unsigned budget){if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};if(dsp.active()&&!ownsDsp_)return {0,false,Block::DspBusy};if(ownsDsp_){auto r=dsp.advanceBefore(d,irq,budget);if(r.returned){ownsDsp_=false;word_->resumeDsp();}return {r.clocks,false,Block::None};}auto c=word_?word_->nextClocks():task_->nextClocks(credits()),arb=d.pendingIrqs?1u:0u;if(!c||budget<c+arb)return {};if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;auto read=[&](uint32_t a,unsigned n,unsigned end){uint32_t v=0;unsigned total=n==4?6:3;for(unsigned i=0;i<n;++i){d.cycles=begin+end-total+(n==4&&i<2?3:total);d.lastRead=int(a+i);v=(v<<8)|d.readResource(a+i);}return v;};bool stack=false;if(word_){stack=word_->stackWrites();word_->advanceBefore(c,read);if(word_->awaitingDsp()){if(!dsp.startWrite(word_->address(),word_->word()))return {0,false,Block::DspBusy};ownsDsp_=true;}else if(word_->complete()){word_.reset();task_->resumeWrite();}}else{auto stage=task_->stage();stack=stage==S::DispatchWord;auto cr=credits();task_->advanceBefore(c,cr,[&](T::Read){auto a=address();return read(a,stage==S::ReadTable?2:1,c);});if(task_->awaitingWrite())word_.emplace(task_->command(),task_->word());}d.cycles=begin+c;d.lastRead=-1;if(stack)d.lastWrite=-1;bool done=task_->complete();if(done)task_.reset();return {c+arb,done,Block::None};}
private:
 struct Range{uint32_t begin,end;};inline static constexpr Range ranges_[]={{0x5c3a,0x5e64},{0x5bec,0x5c04}};
 uint32_t address()const{switch(task_->stage()){case S::ReadBase:return task_->baseAddress();case S::ReadFirst:return task_->paramAddress(0);case S::ReadSecond:return task_->paramAddress(1);case S::ReadThird:return task_->paramAddress(2);case S::ReadFirstControl:return 0xbfc0;case S::ReadSecondControl:return 0xbfc1;case S::ReadThirdControl:return 0xbfc3;case S::ReadTable:return task_->tableAddress();default:return 0;}}
 T::Credits credits()const{return {address()?3u:0u,task_->stage()==S::Return?7u:10u};}
 ControllerDeviceState*bound_=nullptr;std::optional<T>task_{};std::optional<ControllerVoiceDspWordTask>word_{};bool ownsDsp_=false;
};
}
