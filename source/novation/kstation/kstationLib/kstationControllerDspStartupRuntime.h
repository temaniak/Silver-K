#pragma once
#include "kstationControllerDspTransferRuntime.h"
#include "kstationDspStartupTask.h"
#include <optional>
#include <array>
#include <utility>
namespace kstation {
// Reset pin timing and live DSP firmware/table streams. Context intervals are
// bounded timing credits; neither processor frames nor opcodes are represented.
class ControllerDspStartupRuntime final {
 using B=DspBootTask;using R=DspResetTask;using S=DspBootStreamTask;
 enum class Transfer:uint8_t{None,Fault,Address,Invalidate};
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,DspBusy};
 struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(4))return false;for(auto r:ranges_)for(auto a=r.first;a<r.second;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;for(auto r:ranges_)if(!d.protectImmutableVerified(r.first,r.second,image+r.first)||!d.protectImmutableVerified(0x400000+r.first,0x400000+r.second,image+r.first))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return boot_||reset_||stream_||transfer_!=Transfer::None;}
 void reset(){boot_.reset();reset_.reset();stream_.reset();transfer_=Transfer::None;source_=0;}
 bool start(){if(!bound_||active())return false;boot_.emplace();return true;}
 bool startReset(){if(!bound_||active())return false;reset_.emplace();return true;}
 bool startStream(uint32_t source,uint16_t words){if(!bound_||active())return false;source_=source;stream_.emplace(words);return true;}
 uint32_t streamByteReference()const{return source_+(stream_?stream_->sourceIndex():0);}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerDspTransferRuntime&dsp)const{
  if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled)||(dsp.active()&&transfer_==Transfer::None))return 0;
  if(transfer_!=Transfer::None)return dsp.nextClocks(d,irq);
  unsigned cost=reset_?reset_->nextClocks(R::Credits{}):stream_?stream_->nextClocks(streamCredits()):boot_->nextClocks(B::Credits{});
  return cost?cost+(d.pendingIrqs?1u:0u):0;
 }
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerDspTransferRuntime&dsp,unsigned available){
  if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};if(dsp.active()&&transfer_==Transfer::None)return {0,false,Block::DspBusy};
  if(transfer_!=Transfer::None){auto result=dsp.advanceBefore(d,irq,available);if(result.returned){auto which=transfer_;transfer_=Transfer::None;if(which==Transfer::Fault){if(stream_)stream_->resumeFault();else if(boot_)boot_->resumeFault();}else if(boot_)boot_->resumeChild();}return {result.clocks,!active(),Block::None};}
  auto full=nextClocks(d,irq,dsp),arb=d.pendingIrqs?1u:0u;if(!full||available<full)return {};if(arb)(void)d.read8(0xfffff2);auto clocks=full-arb;auto begin=d.cycles;bool stackWrite=false;
  auto read=[&](uint32_t address,unsigned at){d.cycles=begin+at;d.lastRead=history(address&0xffffff);return d.readResource(address);};
  auto write=[&](uint32_t address,uint8_t value,unsigned at){d.cycles=begin+at;d.lastWrite=history(address&0xffffff);d.writeResource(address,value);};
  if(reset_){auto stage=reset_->stage();stackWrite=stage==R::Stage::RetainPins||stage==R::Stage::RetainDelay||stage==R::Stage::RetainInner;
   if(reset_->advanceBefore(clocks,R::Credits{},[&]{return read(0xffffd2,clocks);},[&](uint8_t v){write(0xffffd2,v,clocks);})!=clocks)throw std::logic_error("K DSP reset interval");
   if(reset_->complete()){reset_.reset();if(boot_)boot_->resumeChild();}
  }else if(stream_){auto stage=stream_->stage();bool fault=stage==S::Stage::PollReady&&stream_->poll().stage()==DspPollTask::Stage::DispatchFault;stackWrite=fault;
   if(stream_->advanceBefore(clocks,streamCredits(),[&](bool status,unsigned index){return read(status?0x600004:source_+index,status?clocks:clocks-2);},[&](unsigned port,uint8_t v){write(0x600000+port,v,clocks);})!=clocks)throw std::logic_error("K DSP stream interval");
   if(fault){if(!dsp.startFault())throw std::logic_error("K DSP stream fault admission");transfer_=Transfer::Fault;}
   else if(stream_->complete()){stream_.reset();if(boot_)boot_->resumeChild();}
  }else{
   auto stage=boot_->stage();bool fault=stage==B::Stage::PollFinished&&boot_->poll().stage()==DspPollTask::Stage::DispatchFault;
   stackWrite=fault||stage==B::Stage::ResetDsp||stage==B::Stage::SendProgram||stage==B::Stage::SendAddress||stage==B::Stage::SendTables||stage==B::Stage::InvalidateAddress||stage==B::Stage::RetainDelay||stage==B::Stage::RetainInner;
   if(boot_->advanceBefore(clocks,B::Credits{},[&](unsigned field){return read(stage==B::Stage::PollFinished?0x600004:0x84f2+field,clocks);})!=clocks)throw std::logic_error("K DSP bootstrap interval");
   if(fault){if(!dsp.startFault())throw std::logic_error("K DSP bootstrap fault admission");transfer_=Transfer::Fault;}
   else if(boot_->awaiting()){switch(boot_->child()){
    case B::Child::Reset:reset_.emplace();break;
    case B::Child::ProgramStream:source_=0x84f2;stream_.emplace(boot_->programWords());break;
    case B::Child::Address:if(!dsp.startAddress(0x8200))throw std::logic_error("K DSP bootstrap address admission");transfer_=Transfer::Address;break;
    case B::Child::TableStream:source_=0xa211;stream_.emplace(512);break;
    case B::Child::Invalidate:if(!dsp.startInvalidate())throw std::logic_error("K DSP bootstrap invalidation admission");transfer_=Transfer::Invalidate;break;
    default:break;
   }}else if(boot_->complete())boot_.reset();
  }
  d.cycles=begin+clocks;d.lastRead=-1;if(stackWrite)d.lastWrite=-1;return {full,!active(),Block::None};
 }
private:
 static int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
 S::Credits streamCredits()const{S::Credits c;auto a=streamByteReference()&0xffffff;c.readByte=5+(a<0x400000?3u:a>=0xfffd10&&a<0xffffa0?1u:a>=0xffffa0?1u:3u);return c;}
 inline static constexpr std::array<std::pair<uint32_t,uint32_t>,2>ranges_{{{0x2ae,0x2ee},{0x14ee,0x15a6}}};
 ControllerDeviceState*bound_=nullptr;std::optional<B>boot_{};std::optional<R>reset_{};std::optional<S>stream_{};Transfer transfer_=Transfer::None;uint32_t source_=0;
};
}
