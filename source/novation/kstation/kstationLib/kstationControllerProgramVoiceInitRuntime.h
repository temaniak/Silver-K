#pragma once
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include "kstationProgramVoicesTask.h"
#include "kstationVoiceAllocationTask.h"
#include <optional>
namespace kstation {
class ControllerProgramVoiceInitRuntime final {
 using T=ProgramVoiceInitTask;using S=T::Stage;using P=VoicePartAddressTask;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(4))return false;for(auto r:ranges_)for(auto a=r.begin;a<r.end;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;for(auto r:ranges_)if(!d.protectImmutableVerified(r.begin,r.end,image+r.begin)||!d.protectImmutableVerified(0x400000+r.begin,0x400000+r.end,image+r.begin))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return bool(task_);}bool start(uint8_t part){if(!bound_||active())return false;partInput_=part;task_.emplace();return true;}void reset(){task_.reset();part_.reset();}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{if(bound_!=&d||!irq.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;auto c=part_?part_->nextClocks({3,7}):task_->nextClocks(credits(d.lastWrite));return c?c+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned budget){if(bound_!=&d||!irq.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};auto cr=credits(d.lastWrite);auto c=part_?part_->nextClocks({3,7}):task_->nextClocks(cr),arb=d.pendingIrqs?1u:0u;if(!c||budget<c+arb)return {};if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;bool stack=false;if(part_){part_->advanceBefore(c,{3,7},[](){return 0u;});if(part_->complete()){auto a=part_->address();part_.reset();task_->resumeChild(a);}}else{stack=task_->stage()==S::Address;task_->advanceBefore(c,cr,[&](){auto a=task_->part()+15;d.cycles=begin+c;d.lastRead=int(a);return d.readResource(a);},[&](bool generation,unsigned v){auto a=task_->part()+(generation?15:14);d.cycles=begin+c;d.lastWrite=int(a);d.writeResource(a,uint8_t(v));});if(task_->awaiting())part_.emplace(partInput_);}d.cycles=begin+c;d.lastRead=-1;if(stack)d.lastWrite=-1;bool done=task_->complete();if(done)task_.reset();return {c+arb,done,Block::None};}
private:struct Range{uint32_t begin,end;};inline static constexpr Range ranges_[]={{0x816c,0x8190},{0x72d4,0x72e2}};static unsigned bus(uint32_t a,int p){return ((p&1)||a!=uint32_t(p|1))?3u:0u;}FlashCredits credits(int last)const{FlashCredits c{};if(task_){auto s=task_->stage();if(s==S::ReadGeneration)c.read=3;else if(s==S::PublishEnable||s==S::PublishGeneration)c.write=bus(task_->part()+(s==S::PublishEnable?14:15),last);}return c;}ControllerDeviceState*bound_=nullptr;std::optional<T>task_{};std::optional<P>part_{};uint8_t partInput_=0;
};
}
