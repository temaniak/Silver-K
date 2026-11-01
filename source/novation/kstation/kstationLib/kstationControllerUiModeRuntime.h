#pragma once
#include "kstationUiServicesTask.h"
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include <array>
#include <optional>
namespace kstation {
class ControllerUiModeRuntime final {
 using M=UiModeReadTask;using MS=M::Stage;using G=UiSpecialModeTask;using GS=G::Stage;
public:
 enum class Kind:uint8_t{Mode,Special};enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};struct Range{uint32_t begin,end;};inline static constexpr std::array<Range,2>CodeRanges{{{0x40b0,0x40c0},{0x4534,0x4554}}};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(4))return false;for(auto r:CodeRanges)for(auto a=r.begin;a<r.end;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;for(auto r:CodeRanges)if(!d.protectImmutableVerified(r.begin,r.end,image+r.begin)||!d.protectImmutableVerified(0x400000+r.begin,0x400000+r.end,image+r.begin))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return active_;}uint8_t value()const{return value_;}void reset(){mode_.reset();special_.reset();active_=false;value_=0;}bool start(Kind k){if(!bound_||active_)return false;value_=0;if(k==Kind::Mode)mode_.emplace();else if(k==Kind::Special)special_.emplace();else return false;active_=true;return true;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{if(bound_!=&d||!irq.boundTo(d)||!active_||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;FlashCredits cr;cr.read=3;auto n=mode_?mode_->nextClocks(cr):special_->nextClocks(cr);return n?n+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned budget){if(bound_!=&d||!irq.boundTo(d))return {0,false,Block::NotBound};if(!active_)return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};auto total=nextClocks(d,irq);if(!total||budget<total)return {};auto arb=d.pendingIrqs?1u:0u;if(arb)(void)d.read8(0xfffff2);auto cost=total-arb;auto begin=d.cycles;FlashCredits cr;cr.read=3;if(mode_){if(mode_->advanceBefore(cost,cr,[&]{d.cycles=begin+cost;return d.readResource(0xbb0e);})!=cost)return {};if(mode_->complete()){auto v=mode_->value();mode_.reset();if(special_)special_->resume(v);else{value_=v;active_=false;}}}else{auto stage=special_->stage();if(special_->advanceBefore(cost,cr,[&](bool page){d.cycles=begin+cost;return d.readResource(page?0xcd70:0xcd72);})!=cost)return {};if(stage==GS::Mode)d.lastWrite=-1;if(special_->awaiting())mode_.emplace();if(special_->complete()){value_=special_->result();special_.reset();active_=false;}}d.cycles=begin+cost;d.lastRead=-1;return {total,!active_,Block::None};}
private:ControllerDeviceState*bound_=nullptr;bool active_=false;uint8_t value_=0;std::optional<M>mode_{};std::optional<G>special_{};
};
}
