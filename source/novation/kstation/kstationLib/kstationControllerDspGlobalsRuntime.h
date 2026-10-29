#pragma once
#include "kstationControllerDspTransferRuntime.h"
#include "kstationControllerVoiceInitRuntime.h"
#include "kstationControllerVoiceStatusRuntime.h"
#include "kstationDspGlobalsTask.h"
#include <optional>
#include <array>
#include <utility>
namespace kstation {
class ControllerDspGlobalsRuntime final {
 using G=DspGlobalsTask;using H=DspHarmonicsTask;using C=DspCursorResetTask;
 enum class Child:uint8_t{None,Dsp,Voices,Status};
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,DspBusy};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(8))return false;for(auto r:ranges_)for(auto a=r.first;a<r.second;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!voices_.bindVerifiedFirmware(d,image,size)||!status_.bindVerifiedFirmware(d,image,size))return false;for(auto r:ranges_)if(!d.protectImmutableVerified(r.first,r.second,image+r.first)||!d.protectImmutableVerified(0x400000+r.first,0x400000+r.second,image+r.first))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return globals_||harmonics_||cursor_||child_!=Child::None;}
 void reset(){globals_.reset();harmonics_.reset();cursor_.reset();voices_.reset();status_.reset();child_=Child::None;}
 bool start(){if(!bound_||active())return false;globals_.emplace();return true;}bool startHarmonics(){if(!bound_||active())return false;harmonics_.emplace();return true;}bool startCursor(){if(!bound_||active())return false;cursor_.emplace();return true;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerDspTransferRuntime&dsp)const{
  if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;
  if(child_==Child::Dsp)return dsp.nextClocks(d,irq);if(child_==Child::Voices)return voices_.nextClocks(d,irq);if(child_==Child::Status)return status_.nextClocks(d,irq,dsp);if(dsp.active())return 0;
  auto c=credits(d);auto n=harmonics_?harmonics_->nextClocks(c):cursor_?cursor_->nextClocks(c):globals_->nextClocks(c);return n?n+(d.pendingIrqs?1u:0u):0;
 }
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerDspTransferRuntime&dsp,unsigned available){
  if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};
  if(child_==Child::Dsp){auto r=dsp.advanceBefore(d,irq,available);if(r.returned){child_=Child::None;harmonics_->resume();}return {r.clocks,false,Block::None};}
  if(child_==Child::Voices){auto r=voices_.advanceBefore(d,irq,available);if(r.returned){child_=Child::None;globals_->resume();}return {r.clocks,false,Block::None};}
  if(child_==Child::Status){auto r=status_.advanceBefore(d,irq,dsp,available);if(r.returned){child_=Child::None;globals_->resume();}return {r.clocks,false,Block::None};}
  if(dsp.active())return {0,false,Block::DspBusy};auto full=nextClocks(d,irq,dsp),arb=d.pendingIrqs?1u:0u;if(!full||available<full)return {};if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;auto cost=full-arb;bool stack=false;
  auto publish=[&](uint32_t at,uint8_t v){d.cycles=begin+cost;d.lastWrite=int(at);d.writeResource(at,v);};auto c=credits(d);
  if(harmonics_){auto stage=harmonics_->stage();auto word=harmonics_->word();stack=stage==H::Stage::Invalidate||stage==H::Stage::FirstWord||stage==H::Stage::NextWord;if(harmonics_->advanceBefore(cost,c)!=cost)throw std::logic_error("K harmonic interval");
   if(harmonics_->awaiting()){bool ok=stage==H::Stage::Invalidate?dsp.startInvalidate():stage==H::Stage::FirstWord?dsp.startWrite(0x8400,word):dsp.startNext(word);if(!ok)throw std::logic_error("K harmonic DSP admission");child_=Child::Dsp;}else if(harmonics_->complete()){harmonics_.reset();if(globals_)globals_->resume();}
  }else if(cursor_){if(cursor_->advanceBefore(cost,c,[&](unsigned v){publish(0xd0ec,uint8_t(v));})!=cost)throw std::logic_error("K DSP cursor interval");if(cursor_->complete()){cursor_.reset();if(globals_)globals_->resume();}
  }else{auto stage=globals_->stage();stack=stage==G::Stage::Harmonics||stage==G::Stage::InitializeVoices||stage==G::Stage::ResetCursor||stage==G::Stage::MirrorSlot;
   if(globals_->advanceBefore(cost,c,[&](G::Write field,unsigned v){publish(address(field),uint8_t(v));})!=cost)throw std::logic_error("K DSP globals interval");
   if(globals_->awaiting()){if(stage==G::Stage::Harmonics)harmonics_.emplace();else if(stage==G::Stage::ResetCursor)cursor_.emplace();else if(stage==G::Stage::InitializeVoices){if(!voices_.start())throw std::logic_error("K global voice initialization");child_=Child::Voices;}else if(stage==G::Stage::MirrorSlot){if(!status_.start())throw std::logic_error("K global voice status");child_=Child::Status;}}
   else if(globals_->complete())globals_.reset();
  }
  d.cycles=begin+cost;d.lastRead=-1;if(stack)d.lastWrite=-1;return {full,!active(),Block::None};
 }
private:
 static uint32_t address(G::Write w){switch(w){case G::Write::Count:return 0xd022;case G::Write::Part:return 0xd028;case G::Write::Latch:return 0xd0ee;case G::Write::Slot:return 0xd029;case G::Write::Flags:return 0xd023;}return 0;}
 FlashCredits credits(const ControllerDeviceState&d)const{FlashCredits c;c.call=10;c.shortCall=5;c.ret=7;uint32_t a=0;if(cursor_&&cursor_->stage()==C::Stage::Write)a=0xd0ec;else if(globals_){switch(globals_->stage()){case G::Stage::WriteCount:a=0xd022;break;case G::Stage::WritePart:a=0xd028;break;case G::Stage::WriteLatch:a=0xd0ee;break;case G::Stage::WriteSlot:a=0xd029;break;case G::Stage::WriteFlags:a=0xd023;break;default:break;}}if(a)c.write=(d.lastWrite&1)||a!=uint32_t(d.lastWrite|1)?3:0;return c;}
 inline static constexpr std::array<std::pair<uint32_t,uint32_t>,2>ranges_{{{0x5b38,0x5ba0},{0x8190,0x819a}}};
 ControllerDeviceState*bound_=nullptr;std::optional<G>globals_{};std::optional<H>harmonics_{};std::optional<C>cursor_{};ControllerVoiceInitRuntime voices_;ControllerVoiceStatusRuntime status_;Child child_=Child::None;
};
}
