#pragma once
#include "kstationDspStartupTask.h"
#include "kstationHdiWait.h"
#include "kstationNativeDspTransfer.h"
#include <optional>
namespace kstation {
// Transitional observation adapter for 155E's complete stream, with explicit
// fault-child ownership by NativeDspTransfer (14E6 ->20F6).
class NativeDspBootStream final {
 using T=DspBootStreamTask;using P=DspPollTask;
public:
 bool active()const{return bool(task_);}void reset(){task_.reset();source_=0;}bool awaitingFault()const{return task_&&task_->awaitingFault();}
 static bool candidate(uint32_t p){return p>=0x155e&&p<0x15a6;}
 bool step(Stage1Cpu&c){if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&128)))return false;auto at=uint32_t(c.getPC());if(!task_){if(at!=0x155e)return false;task_.emplace(uint16_t(c.reg16(11)));source_=uint32_t(c.reg32(4));}auto&t=*task_;if(t.awaitingFault()){if(at!=0x1580)return false;t.resumeFault();}if(at!=boundary(t))return false;if(c.pending_irqs)c.read8(0xfffff2);unsigned len=bytes(t);T::Credits cr;cr.protocol.call=8+bus(c.getSP()-4,4,c.writeHistory());cr.protocol.ret=5+bus(c.getSP(),4,int(at+len-1));cr.readByte=5+bus(source_+t.sourceIndex(),1,int(at+len-1));auto cost=t.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(at+1);
  if(t.stage()==T::Stage::PollReady){switch(t.poll().stage()){case P::Stage::FirstMask:case P::Stage::Mask:c.reg8(8)&=2;value(c,c.reg8(8));break;case P::Stage::ResetRetries:c.reg16(8)=0;c.ccr=uint8_t((c.ccr&~0x2f)|4);break;case P::Stage::DecrementRetries:dec(c,8);break;case P::Stage::DispatchFault:c.write32(0x1580,int(c.getSP()-4));c.setSP(c.getSP()-4);c.irqDepth(c.irqDepth()+1);c.clockI(2);c.pc=c.makepc(0x14e6);break;default:break;}}
  else if(t.stage()==T::Stage::Decrement)dec(c,11);else if(t.stage()==T::Stage::Return){c.clockI(2);auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(c.read32(int(sp))&0xffffff);c.irqDepth(c.irqDepth()-1);}
  t.advanceBefore(cost,cr,[&](bool status,unsigned index)->uint32_t{c.reg8(8)=c.read8(int(status?0x600004:source_+index));if(!status){c.reg32(4)=int32_t(uint32_t(c.reg32(4))+1);c.clockI(2);}value(c,c.reg8(8));return uint8_t(c.reg8(8));},[&](unsigned offset,uint8_t v){c.write8(int8_t(v),int(0x600000+offset));value(c,c.reg8(8));});
  if(c.cycles-begin!=cost)throw std::logic_error("Native K boot stream clocks differ");if(t.complete())task_.reset();else if(!t.awaitingFault())c.pc=c.makepc(boundary(t));return true;
 }
private:
 static unsigned bus(uint32_t at,unsigned n,int last){unsigned cost=0;for(unsigned i=0;i<n;++i){auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((last&1)||a!=uint32_t(last|1))cost+=a<0x400000?3:1;last=int(a);}else{cost+=a>=0xffffa0?1:3;last=-1;}}return cost;}
 static void value(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}static void dec(Stage1Cpu&c,unsigned r){int old=c.reg16(r),v=old-1;value(c,v);if((old&~v)&0x8000)c.ccr|=2;c.reg16(r)=int16_t(v);}
 static unsigned boundary(const T&t){switch(t.stage()){case T::Stage::PollReady:{static constexpr unsigned a[]={0x155e,0x1566,0x1568,0x156a,0x156c,0x1574,0x1576,0x1578,0x157a,0x157c,0,0x1580,0};return a[unsigned(t.poll().stage())];}case T::Stage::ReadHigh:return 0x1582;case T::Stage::WriteHigh:return 0x1584;case T::Stage::ReadMiddle:return 0x158c;case T::Stage::WriteMiddle:return 0x158e;case T::Stage::ReadLow:return 0x1596;case T::Stage::WriteLow:return 0x1598;case T::Stage::Decrement:return 0x15a0;case T::Stage::Repeat:return 0x15a2;case T::Stage::Return:return 0x15a4;case T::Stage::Complete:return 0;}return 0;}
 static unsigned bytes(const T&t){if(t.stage()==T::Stage::PollReady){auto s=t.poll().stage();return s==P::Stage::FirstRead||s==P::Stage::Read?8:s==P::Stage::DispatchFault?4:2;}auto s=t.stage();return s==T::Stage::WriteHigh||s==T::Stage::WriteMiddle||s==T::Stage::WriteLow?8:2;}
 std::optional<T>task_;uint32_t source_=0;
};
}

namespace kstation {
// Complete 14EE DSP startup owner, including live reset edges, exact delay
// phases, both streams, and native address/cache/fault children.
class NativeDspStartup final {
 using B=DspBootTask;using R=DspResetTask;using P=DspPollTask;
public:
 struct CodeRange{uint32_t begin,end;};
 inline static constexpr std::array<CodeRange,2>CodeRanges{{{0x2ae,0x2ee},{0x14ee,0x15a6}}};
 bool active()const{return boot_||reset_||stream_.active()||transfer_.active();}
 void reset(){boot_.reset();reset_.reset();stream_.reset();transfer_.reset();steps_=0;}
 bool awaitingFault()const{return stream_.awaitingFault()||(boot_&&boot_->awaitingFault());}
 static bool candidate(uint32_t pc){return (pc>=0x14ee&&pc<0x15a6)||(pc>=0x2ae&&pc<0x2ee)||NativeDspTransfer::candidate(pc);}
 bool step(Stage1Cpu&c){const auto at=uint32_t(c.getPC());if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&128)))return false;
  if(NativeDspTransfer::candidate(at)){if(!transfer_.step(c))return false;++steps_;return true;}
  if(at>=0x155e&&at<0x15a6){if(!stream_.step(c))return false;++steps_;return true;}
  if(at==0x2ae&&!reset_)reset_.emplace();
  if(reset_){if(at!=resetBoundary(*reset_))return false;resetStep(c);++steps_;return true;}
  if(at==0x14ee&&!boot_)boot_.emplace();if(!boot_)return false;auto&t=*boot_;
  if(t.awaiting()){if(at!=bootBoundary(t))return false;t.resumeChild();}if(t.awaitingFault()){if(at!=0x1544)return false;t.resumeFault();}if(at!=bootBoundary(t))return false;
  if(c.pending_irqs)c.read8(0xfffff2);
  const auto stage=t.stage();const auto len=bootBytes(t);auto cr=bootCredits(c,len);auto cost=t.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(at+1);
  switch(stage){case B::Stage::ResetDsp:call(c,0x2ae,0x14f2,true);break;case B::Stage::BindPort:c.reg32(5)=0x600000;value(c,c.reg32(5));break;case B::Stage::BindProgram:c.reg32(4)=0x84f2;value(c,c.reg32(4));break;case B::Stage::BindCount:c.reg16(11)=c.reg16(0);value(c,c.reg16(11));break;case B::Stage::IncludeHeader:inc(c,11,2);break;case B::Stage::SendProgram:call(c,0x155e,0x150c,false);break;
  case B::Stage::RetainDelay:case B::Stage::RetainInner:push(c,0);break;case B::Stage::CountSettling:c.reg16(0)=10;value(c,10);break;case B::Stage::CountDelay:c.reg16(0)=2663;value(c,2663);break;case B::Stage::TickDelay:case B::Stage::TickSettling:dec(c,0);break;case B::Stage::ReleaseInner:case B::Stage::ReleaseDelay:pop(c,0);break;
  case B::Stage::PollFinished:projectPoll(t.poll(),c);break;
  case B::Stage::SelectAddress:c.reg16(4)=int16_t(0x8200);value(c,c.reg16(4));break;case B::Stage::SendAddress:call(c,0x15a6,0x154c,false);break;case B::Stage::BindTables:c.reg32(4)=0xa211;value(c,c.reg32(4));break;case B::Stage::CountTables:c.reg16(11)=512;value(c,512);break;case B::Stage::SendTables:call(c,0x155e,0x1558,false);break;case B::Stage::InvalidateAddress:call(c,0x1640,0x155c,true);break;case B::Stage::Return:ret(c);break;default:break;}
  t.advanceBefore(cost,cr,[&](unsigned offset)->uint32_t{if(stage==B::Stage::PollFinished){c.reg8(8)=c.read8(0x600004);value(c,c.reg8(8));return uint8_t(c.reg8(8));}auto v=uint8_t(c.read8(0x84f2+offset));auto r=offset==1?0:8;c.reg8(r)=int8_t(v);value(c,c.reg8(r));return v;});
  if(c.cycles-begin!=cost)throw std::logic_error("Native K DSP startup clocks differ");if(t.complete())boot_.reset();else if(!t.awaiting()&&!t.awaitingFault())c.pc=c.makepc(bootBoundary(t));++steps_;return true;
 }
 uint64_t steps()const{return steps_;}
private:
 static unsigned bus(uint32_t at,unsigned n,int last){unsigned cost=0;for(unsigned i=0;i<n;++i){auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((last&1)||a!=uint32_t(last|1))cost+=a<0x400000?3:1;last=int(a);}else{cost+=a>=0xffffa0?1:3;last=-1;}}return cost;}
 static B::Credits bootCredits(const Stage1Cpu&c,unsigned len){B::Credits cr;cr.protocol.call=8+bus(c.getSP()-4,4,c.writeHistory());cr.shortCall=3+bus(c.getSP()-4,4,c.writeHistory());cr.protocol.ret=5+bus(c.getSP(),4,int(c.getPC()+len-1));cr.protocol.retainWord=5+bus(c.getSP()-2,2,c.writeHistory());cr.protocol.restoreWord=5+bus(c.getSP(),2,int(c.getPC()+len-1));return cr;}
 static void value(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static void inc(Stage1Cpu&c,unsigned r,int n){int old=c.reg16(r),v=old+n;value(c,v);if((~old&v)&0x8000)c.ccr|=2;c.reg16(r)=int16_t(v);}
 static void dec(Stage1Cpu&c,unsigned r){int old=c.reg16(r),v=old-1;value(c,v);if((old&~v)&0x8000)c.ccr|=2;c.reg16(r)=int16_t(v);}
 static void push(Stage1Cpu&c,unsigned r){c.setSP(c.getSP()-2);c.write16(c.reg16(r),int(c.getSP()));c.clockI(2);value(c,c.reg16(r));}
 static void pop(Stage1Cpu&c,unsigned r){c.reg16(r)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);value(c,c.reg16(r));}
 static void call(Stage1Cpu&c,unsigned target,unsigned returned,bool wide){c.write32(returned,int(c.getSP()-4));c.setSP(c.getSP()-4);c.irqDepth(c.irqDepth()+1);if(wide)c.clockI(2);c.pc=c.makepc(target);}
 static void ret(Stage1Cpu&c){c.clockI(2);auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(c.read32(int(sp))&0xffffff);c.irqDepth(c.irqDepth()-1);}
 static void projectPoll(const P&p,Stage1Cpu&c){switch(p.stage()){case P::Stage::FirstMask:case P::Stage::Mask:c.reg8(8)&=16;value(c,c.reg8(8));break;case P::Stage::ResetRetries:c.reg16(8)=0;c.ccr=uint8_t((c.ccr&~0x2f)|4);break;case P::Stage::DecrementRetries:dec(c,8);break;case P::Stage::DispatchFault:call(c,0x14e6,0x1544,false);break;default:break;}}
 static unsigned bootBoundary(const B&t){switch(t.stage()){case B::Stage::ResetDsp:return 0x14ee;case B::Stage::BindPort:return 0x14f2;case B::Stage::BindProgram:return 0x14f8;case B::Stage::ReadCountHigh:return 0x14fe;case B::Stage::ReadCountLow:return 0x1502;case B::Stage::BindCount:return 0x1506;case B::Stage::IncludeHeader:return 0x1508;case B::Stage::SendProgram:return 0x150a;case B::Stage::RetainDelay:return 0x150c;case B::Stage::CountSettling:return 0x150e;case B::Stage::RetainInner:return 0x1512;case B::Stage::CountDelay:return 0x1514;case B::Stage::TickDelay:return 0x1518;case B::Stage::RepeatDelay:return 0x151a;case B::Stage::ReleaseInner:return 0x151c;case B::Stage::TickSettling:return 0x151e;case B::Stage::RepeatSettling:return 0x1520;case B::Stage::ReleaseDelay:return 0x1522;case B::Stage::PollFinished:{static constexpr unsigned a[]={0x1524,0x152c,0x152e,0x1530,0x1532,0x153a,0x153c,0x153e,0x1540,0x1542,0,0x1544,0};return a[unsigned(t.poll().stage())];}case B::Stage::SelectAddress:return 0x1546;case B::Stage::SendAddress:return 0x154a;case B::Stage::BindTables:return 0x154c;case B::Stage::CountTables:return 0x1552;case B::Stage::SendTables:return 0x1556;case B::Stage::InvalidateAddress:return 0x1558;case B::Stage::Return:return 0x155c;case B::Stage::Complete:return 0;}return 0;}
 static unsigned bootBytes(const B&t){switch(t.stage()){case B::Stage::BindPort:case B::Stage::BindProgram:case B::Stage::BindTables:return 6;case B::Stage::ResetDsp:case B::Stage::ReadCountHigh:case B::Stage::ReadCountLow:case B::Stage::CountSettling:case B::Stage::CountDelay:case B::Stage::SelectAddress:case B::Stage::CountTables:case B::Stage::InvalidateAddress:return 4;case B::Stage::PollFinished:return t.poll().stage()==P::Stage::FirstRead||t.poll().stage()==P::Stage::Read?8:2;default:return 2;}}
 static unsigned resetBoundary(const R&t){switch(t.stage()){case R::Stage::RetainPins:return 0x2ae;case R::Stage::ReadPins:return 0x2b0;case R::Stage::Assert:return 0x2b2;case R::Stage::PublishAssert:return 0x2b4;case R::Stage::RetainDelay:return t.secondDelay()?0x2d2:0x2b6;case R::Stage::CountSettling:return t.secondDelay()?0x2d4:0x2b8;case R::Stage::RetainInner:return t.secondDelay()?0x2d8:0x2bc;case R::Stage::CountDelay:return t.secondDelay()?0x2da:0x2be;case R::Stage::TickDelay:return t.secondDelay()?0x2de:0x2c2;case R::Stage::RepeatDelay:return t.secondDelay()?0x2e0:0x2c4;case R::Stage::ReleaseInner:return t.secondDelay()?0x2e2:0x2c6;case R::Stage::TickSettling:return t.secondDelay()?0x2e4:0x2c8;case R::Stage::RepeatSettling:return t.secondDelay()?0x2e6:0x2ca;case R::Stage::ReleaseDelay:return t.secondDelay()?0x2e8:0x2cc;case R::Stage::ReleaseReset:return 0x2ce;case R::Stage::PublishRelease:return 0x2d0;case R::Stage::RestorePins:return 0x2ea;case R::Stage::Return:return 0x2ec;case R::Stage::Complete:return 0;}return 0;}
 void resetStep(Stage1Cpu&c){auto&t=*reset_;auto s=t.stage();unsigned len=s==R::Stage::CountSettling||s==R::Stage::CountDelay?4:2;if(c.pending_irqs)c.read8(0xfffff2);auto bc=bootCredits(c,len);R::Credits cr{bc.protocol.retainWord,bc.protocol.restoreWord,bc.protocol.ret};auto cost=t.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(c.getPC()+1);
  switch(s){case R::Stage::RetainPins:push(c,6);break;case R::Stage::Assert:c.reg8(6)&=int8_t(~2u);break;case R::Stage::ReleaseReset:c.reg8(6)|=2;break;case R::Stage::RetainDelay:case R::Stage::RetainInner:push(c,0);break;case R::Stage::CountSettling:c.reg16(0)=10;value(c,10);break;case R::Stage::CountDelay:c.reg16(0)=2663;value(c,2663);break;case R::Stage::TickDelay:case R::Stage::TickSettling:dec(c,0);break;case R::Stage::ReleaseInner:case R::Stage::ReleaseDelay:pop(c,0);break;case R::Stage::RestorePins:pop(c,6);break;case R::Stage::Return:ret(c);break;default:break;}
  t.advanceBefore(cost,cr,[&]{c.reg8(6)=c.read8(0xffffd2);value(c,c.reg8(6));return uint8_t(c.reg8(6));},[&](uint8_t v){value(c,c.reg8(6));c.write8(int8_t(v),0xffffd2);});if(c.cycles-begin!=cost)throw std::logic_error("Native K DSP reset clocks differ");if(t.complete())reset_.reset();else c.pc=c.makepc(resetBoundary(t));
 }
 std::optional<B>boot_;std::optional<R>reset_;NativeDspBootStream stream_;NativeDspTransfer transfer_;uint64_t steps_=0;
};
}
