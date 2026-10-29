#pragma once
#include "kstationMainLoopTask.h"
#include "kstationHdiWait.h"
#include <array>
#include <cstdint>

namespace kstation {
// Temporary architectural projection for an individual deferred slot. It
// never runs the tick or callback: the owner must service and resume them.
class NativeDeferredSlot final {
 using T=DeferredSlotTask; using P=T::Phase;
public:
 explicit NativeDeferredSlot(unsigned slot):task_(slot){}
 const T& task()const{return task_;}
 static uint32_t entry(unsigned slot){return slot<5?0x38b0+0x5c*slot:0;}
 uint32_t boundary()const { const auto b=entry(task_.slot());switch(task_.phase()){
 case P::SaveDelay:return b;case P::ReadDelay:return b+2;case P::SelectEnabled:return b+8;
 case P::SaveTick:return b+10;case P::RequestTick:return b+14;case P::ReadStart:return b+18;
 case P::Elapsed:return b+24;case P::Compare:return b+26;case P::SelectDue:return b+28;
 case P::ClearValue:return b+30;case P::ClearDelay:return b+32;case P::ReadCallback:return b+38;
 case P::RequestCallback:return b+46;case P::RestoreTick:return b+48;case P::RestoreDelay:return b+52;
 case P::Return:return b+54;default:return 0;}}
 bool resumeChild(Stage1Cpu& c) {
  if(!task_.awaitingChild())return false;
  if(task_.child()==T::Child::Tick){if(uint32_t(c.getPC())!=entry(task_.slot())+18)return false;return task_.resumeTick(uint16_t(c.reg16(6)));}
  if(uint32_t(c.getPC())!=entry(task_.slot())+48)return false;return task_.resumeCallback();
 }
 bool step(Stage1Cpu&c){
  if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80))||task_.awaitingChild()||task_.complete()||uint32_t(c.getPC())!=boundary())return false;
  if(c.pending_irqs)c.read8(0xfffff2);
  const auto at=boundary();const auto p=task_.phase();const auto len=length(p);c.fetchClocks(len);c.pc=c.makepc(int(at+1));
  const uint32_t delay=0xcd44+8*task_.slot();
  switch(p){
   case P::SaveDelay:c.setSP(c.getSP()-2);c.write16(c.reg16(5),int(c.getSP()));c.clockI(2);val(c,c.reg16(5));break;
   case P::SaveTick:c.setSP(c.getSP()-4);c.write32(c.reg32(6),int(c.getSP()));c.clockI(2);val(c,c.reg32(6));break;
   case P::RequestTick:call(c,0x9aa,at+len,true);break;
   case P::Elapsed:c.reg16(6)=int16_t(sub(c,uint16_t(c.reg16(6)),uint16_t(c.reg16(14))));break;
   case P::Compare:sub(c,uint16_t(c.reg16(6)),uint16_t(c.reg16(5)));break;
   case P::ClearValue:c.reg16(14)=int16_t(sub(c,uint16_t(c.reg16(14)),uint16_t(c.reg16(14))));break;
   case P::RequestCallback:call(c,uint32_t(c.reg32(6))&0xffffff,at+len,false);break;
   case P::RestoreTick:c.reg32(6)=int32_t(c.read32(int(c.getSP())));c.setSP(c.getSP()+4);c.clockI(2);val(c,c.reg32(6));break;
   case P::RestoreDelay:c.reg16(5)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);val(c,c.reg16(5));break;
   case P::Return:c.irqDepth(c.irqDepth()-1);c.clockI(2);{const auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));}break;
   default:break;
  }
  task_.advance([&](T::Read field)->uint32_t{switch(field){
   case T::Read::Delay:c.reg16(5)=int16_t(c.read16(int(delay)));val(c,c.reg16(5));return uint16_t(c.reg16(5));
   case T::Read::Start:c.reg16(14)=int16_t(c.read16(int(delay-2)));val(c,c.reg16(14));return uint16_t(c.reg16(14));
   case T::Read::Callback:c.reg32(6)=int32_t(c.read32(int(delay+2)));val(c,c.reg32(6));return uint32_t(c.reg32(6));}return 0;},[&](T::Write,uint32_t v){c.write16(int16_t(v),int(delay));val(c,c.reg16(14));});
  if(!task_.awaitingChild()&&!task_.complete())c.pc=c.makepc(int(boundary()));
  return true;
 }
private:
 static unsigned length(P p){switch(p){case P::ReadDelay:case P::ReadStart:case P::ClearDelay:return 6;case P::ReadCallback:return 8;case P::SaveTick:case P::RequestTick:case P::RestoreTick:return 4;default:return 2;}}
 static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static uint16_t sub(Stage1Cpu&c,uint16_t a,uint16_t b){const uint32_t r=uint16_t(a-b),carry=(~a&b)|(~a&r)|(b&r),over=(a&~b&~r)|(~a&b&r);c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&0x8000)?8:0)|((carry&0x8000)?1:0)|((carry&0x800)?32:0)|((over&0x8000)?2:0));return uint16_t(r);}
 static void call(Stage1Cpu&c,uint32_t target,uint32_t returned,bool wide){auto sp=c.getSP()-4;c.write32(int32_t(returned),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);if(wide)c.clockI(2);c.pc=c.makepc(int(target));}
 T task_;
};

// Projection of the recurring call sequence; child completion is explicit and
// the caller alone decides whether each requested child is native or H8.
class NativeMainLoop final {
public:
 const MainLoopTask& task()const{return task_;}
 uint32_t childAddress()const{return task_.index()<targets_.size()?targets_[task_.index()]:0;}
 uint32_t boundary()const{return task_.atBackEdge()?0x3b1e:sites_[task_.index()];}
 bool resumeChild(const Stage1Cpu&c){if(!task_.awaitingChild()||uint32_t(c.getPC())!=sites_[task_.index()]+length(task_.index()))return false;return task_.resumeChild(task_.request().child);}
 bool step(Stage1Cpu&c){
  if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80))||task_.awaitingChild()||uint32_t(c.getPC())!=boundary())return false;
  if(c.pending_irqs)c.read8(0xfffff2);
  auto at=boundary();if(task_.atBackEdge()){c.fetchClocks(2);c.pc=c.makepc(0x3ae8);return task_.nextIteration();}
  const auto i=task_.index(),len=length(i);c.fetchClocks(len);c.pc=c.makepc(int(at+1));const auto sp=c.getSP()-4;c.write32(int32_t(at+len),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);if(len==4)c.clockI(2);c.pc=c.makepc(int(targets_[i]));return task_.beginChild();
 }
private:
 static unsigned length(unsigned i){return i==9?2:4;}
 inline static constexpr std::array<uint32_t,14> sites_{{0x3ae8,0x3aec,0x3af0,0x3af4,0x3af8,0x3afc,0x3b00,0x3b04,0x3b08,0x3b0c,0x3b0e,0x3b12,0x3b16,0x3b1a}};
 inline static constexpr std::array<uint32_t,14> targets_{{0x38b0,0x390c,0x3968,0x39c4,0x3a20,0x3be4,0x3c10,0x401c,0x252c,0x3b20,0x819a,0x3446,0x581e,0x834a}};
 MainLoopTask task_;
};
}
