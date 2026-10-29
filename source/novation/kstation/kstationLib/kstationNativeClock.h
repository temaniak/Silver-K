#pragma once
#include "kstationClockPulseTask.h"
#include "kstationClockTasks.h"
#include "kstationHdiWait.h"
#include <array>
#include <variant>
#include <stdexcept>
namespace kstation {
// Fixed K clock graph projection. Musical continuations live in the pure
// tasks; code locations and architectural observations belong only here.
class NativeClock final {
 using T=ClockTickTask;using P=ClockPulseTask;using R=ClockReadTask;using W=ClockSendTask;
 struct Job {std::variant<T,P,R,W> task{};};
public:
 struct CodeRange{uint32_t begin,end;};
 inline static constexpr std::array<CodeRange,3>CodeRanges{{{0x946,0x968},{0xaf4,0xb0c},{0x1970,0x1b56}}};
 struct Metrics{uint64_t steps=0,internalCalls=0,externalCalls=0,trackerCalls=0,timerReads=0,clockSends=0,adaptiveUpdates=0;};
 const Metrics&metrics()const{return metrics_;}void reset(){depth_=0;metrics_={};}bool active()const{return depth_!=0;}
 static bool candidate(uint32_t pc){return (pc>=0x1970&&pc<0x1b56)||(pc>=0x946&&pc<0x968)||(pc>=0xaf4&&pc<0xb0c);}
 bool step(Stage1Cpu&c){
  if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80)))return false;
  const auto at=uint32_t(c.getPC());if(!candidate(at))return false;
  if(!depth_||boundary(jobs_[depth_-1])!=at){
   if(depth_==jobs_.size())throw std::logic_error("Native K clock capacity");
   Job j;
   if(at==0x1970){j.task.emplace<T>();++metrics_.internalCalls;}
   else if(at==0x19f4){j.task.emplace<P>(P::Kind::Latched,0,0);++metrics_.externalCalls;}
   else if(at==0x1a08){j.task.emplace<P>(P::Kind::Edge,uint8_t(c.reg8(9)),uint8_t(c.reg8(10)));++metrics_.trackerCalls;}
   else if(at==0x946){j.task.emplace<R>();++metrics_.timerReads;}
   else if(at==0xaf4){j.task.emplace<W>();++metrics_.clockSends;}
   else throw std::logic_error("Unowned native K clock boundary");
   jobs_[depth_++]=j;
  }
  if(c.pending_irqs)c.read8(0xfffff2);
  auto&j=jobs_[depth_-1];const auto length=bytes(at);const auto cr=credits(c,at,length);const auto cost=nextClocks(j,cr);const auto begin=c.cycles;
  if(!cost)throw std::logic_error("Blocked native K clock continuation");
  c.fetchClocks(length);c.pc=c.makepc(int(at+1));
  project(j,c);
  advance(j,c,cost,cr);
  if(c.cycles-begin!=cost)throw std::logic_error("Native K clock phase cost differs");
  if(complete(j)){
   const auto result=std::holds_alternative<R>(j.task)?std::get<R>(j.task).result():0;
   --depth_;if(depth_)resume(jobs_[depth_-1],uint32_t(c.getPC()),uint16_t(result));
  }else if(!awaiting(j))c.pc=c.makepc(int(boundary(j)));
  ++metrics_.steps;return true;
 }
private:
 static unsigned busCost(uint32_t a,unsigned count,int previous){unsigned cost=0;for(unsigned n=0;n<count;++n){const auto at=(a+n)&0xffffff;if(at<0x400000||(at>=0xfffd10&&at<0xffffa0)){if((previous&1)||at!=uint32_t(previous|1))cost+=at<0x400000?3:1;previous=int(at);}else{cost+=at>=0xffffa0?1:3;previous=-1;}}return cost;}
 static ClockCredits credits(const Stage1Cpu&c,uint32_t at,unsigned length){ClockCredits x;const int read=int(at+length-1);x.retainLong=8+busCost(c.getSP()-4,4,c.writeHistory());x.restoreLong=8+busCost(c.getSP(),4,read);x.retainWord=5+busCost(c.getSP()-2,2,c.writeHistory());x.restoreWord=5+busCost(c.getSP(),2,read);x.call=8+busCost(c.getSP()-4,4,c.writeHistory());x.shortCall=3+busCost(c.getSP()-4,4,c.writeHistory());x.ret=5+busCost(c.getSP(),4,read);x.flagWrite=busCost(0xfffe6f,1,c.writeHistory());x.latchWrite=busCost(0xfffe73,1,c.writeHistory());return x;}
 static P::Credits pulseCredits(const ClockCredits&c){return {c.call,c.shortCall,c.ret,c.retainWord,c.restoreWord,c.flagWrite};}
 static unsigned nextClocks(const Job&j,const ClockCredits&c){switch(j.task.index()){case 0:return std::get<T>(j.task).nextClocks(c);case 1:return std::get<P>(j.task).nextClocks(pulseCredits(c));case 2:return std::get<R>(j.task).nextClocks(c);default:return std::get<W>(j.task).nextClocks(c);}}
 static bool complete(const Job&j){return std::visit([](const auto&t){return t.complete();},j.task);}
 static bool awaiting(const Job&j){if(const auto*t=std::get_if<T>(&j.task))return t->awaitingChild();if(const auto*p=std::get_if<P>(&j.task))return p->awaitingChild();return false;}
 static void resume(Job&j,uint32_t returned,uint16_t tick){if(auto*t=std::get_if<T>(&j.task)){if(t->awaitingChild()&&returned==0x19a2)t->resumeChild();}else if(auto*p=std::get_if<P>(&j.task)){if(p->awaitingChild()){const auto s=p->resumeStage();if((s==P::Stage::ClearLatched&&returned==0x19fe)||(s==P::Stage::WriteResetTick&&returned==0x1a22)||(s==P::Stage::RetainTick&&returned==0x1a8e))p->resumeChild(tick);}}}
 static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static void bit(Stage1Cpu&c,unsigned mask,bool value){c.ccr=uint8_t((c.ccr&~mask)|(value?mask:0));}
 static uint32_t arithmetic(Stage1Cpu&c,uint32_t a,uint32_t b,unsigned bits,bool subtract){const uint32_t mask=bits==32?0xffffffffu:((1u<<bits)-1),sign=1u<<(bits-1),half=1u<<(bits-5);a&=mask;b&=mask;const uint32_t r=(subtract?a-b:a+b)&mask;const auto carry=subtract?((~a&b)|(~a&r)|(b&r)):((a&b)|(a&~r)|(b&~r));const auto over=subtract?((a&~b&~r)|(~a&b&r)):((a&b&~r)|(~a&~b&r));c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&sign)?8:0)|((carry&sign)?1:0)|((carry&half)?32:0)|((over&sign)?2:0));return r;}
 static void incWord(Stage1Cpu&c,int reg){const int old=c.reg16(reg),v=old+1;val(c,v);bit(c,2,(~old&v)&0x8000);c.reg16(reg)=int16_t(v);}
 static void incByte(Stage1Cpu&c,int reg){const int old=c.reg8(reg),v=old+1;val(c,v);bit(c,2,(~old&v)&0x80);c.reg8(reg)=int8_t(v);}
 static void left(Stage1Cpu&c,int r){const uint32_t a=uint32_t(c.reg32(r));bit(c,1,a&0x80000000);c.reg32(r)=int32_t(a<<1);val(c,c.reg32(r));}
 static void right(Stage1Cpu&c,int r,bool sign=false){const uint32_t a=uint32_t(c.reg32(r));bit(c,1,a&1);c.reg32(r)=int32_t((a>>1)|(sign?(a&0x80000000):0));val(c,c.reg32(r));}
 static void saveLong(Stage1Cpu&c,int r){c.setSP(c.getSP()-4);c.write32(c.reg32(r),int(c.getSP()));val(c,c.reg32(r));c.clockI(2);}
 static void restoreLong(Stage1Cpu&c,int r){c.reg32(r)=int32_t(c.read32(int(c.getSP())));c.setSP(c.getSP()+4);val(c,c.reg32(r));c.clockI(2);}
 static void saveWord(Stage1Cpu&c,int r){c.setSP(c.getSP()-2);c.write16(c.reg16(r),int(c.getSP()));c.clockI(2);val(c,c.reg16(r));}
 static void restoreWord(Stage1Cpu&c,int r){c.reg16(r)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);val(c,c.reg16(r));}
 static void call(Stage1Cpu&c,uint32_t target,uint32_t returned,bool wide=true){const auto sp=c.getSP()-4;c.write32(int32_t(returned),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);if(wide)c.clockI(2);c.pc=c.makepc(int(target));}
 static void ret(Stage1Cpu&c){c.irqDepth(c.irqDepth()-1);c.clockI(2);const auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));}
 static void mulSigned(Stage1Cpu&c,int src,int dst){const int32_t v=int32_t(c.reg16(src))*int32_t(int16_t(c.reg32(dst)));c.reg32(dst)=v;bit(c,8,v<0);bit(c,4,!v);c.clockI(20);}
 static void mulUnsigned(Stage1Cpu&c,int src,int dst){c.reg32(dst)=int32_t(uint32_t(uint16_t(c.reg16(src)))*uint16_t(c.reg32(dst)));c.clockI(20);}
 static void divUnsigned(Stage1Cpu&c,int src,int dst){const auto d=uint16_t(c.reg16(src));const auto n=uint32_t(c.reg32(dst));const uint32_t q=d?n/d:0;bit(c,8,d&0x8000);bit(c,4,!d);c.reg16(dst)=int16_t(q);c.reg16(dst+8)=int16_t(n-q*d);c.clockI(20);}
 static void divSigned(Stage1Cpu&c,int src,int dst){const int64_t d=c.reg16(src),n=c.reg32(dst);c.clockI(20);bit(c,4,!d);if(!d&&n)return;const auto q=d?n/d:1;const auto rem=n-q*d;c.reg16(dst)=int16_t(q);c.reg16(dst+8)=int16_t(rem);bit(c,8,((d&~n)|(~d&n))&0x8000);}
 static uint32_t readByte(Stage1Cpu&c,uint32_t a,int r){c.reg8(r)=c.read8(int(a));val(c,c.reg8(r));return uint8_t(c.reg8(r));}
 static uint32_t readWord(Stage1Cpu&c,uint32_t a,int r){c.reg16(r)=int16_t(c.read16(int(a)));val(c,c.reg16(r));return uint16_t(c.reg16(r));}
 static uint32_t readLong(Stage1Cpu&c,uint32_t a,int r){c.reg32(r)=int32_t(c.read32(int(a)));val(c,c.reg32(r));return uint32_t(c.reg32(r));}
 static void writeByte(Stage1Cpu&c,uint32_t a,uint32_t v,int r){val(c,c.reg8(r));c.write8(int8_t(v),int(a));}
 static void writeWord(Stage1Cpu&c,uint32_t a,uint32_t v,int r){c.write16(int16_t(v),int(a));val(c,c.reg16(r));}
 static void writeLong(Stage1Cpu&c,uint32_t a,uint32_t v,int r){c.write32(int32_t(v),int(a));val(c,c.reg32(r));}
 static void project(Job&j,Stage1Cpu&c){
  if(auto*t=std::get_if<T>(&j.task)){using S=T::Stage;switch(t->stage()){
   case S::RetainIncrement:saveLong(c,1);break;case S::RetainCount:saveLong(c,2);break;
   case S::AdvancePhase:c.reg32(0)=int32_t(arithmetic(c,uint32_t(c.reg32(0)),uint32_t(c.reg32(1)),32,false));break;
   case S::IncrementPulseCount:incWord(c,10);break;
   case S::TestOutputFlag:bit(c,4,!(c.reg8(9)&2));break;
   case S::SendClock:call(c,0xaf4,0x19a2);break;
   case S::AdvanceSecondary:c.reg32(0)=int32_t(arithmetic(c,uint32_t(c.reg32(0)),uint32_t(c.reg32(1)),32,false));break;
   case S::AdvanceTimeout:incByte(c,8);break;
   case S::ClearTimeoutFlag:c.reg8(0)=int8_t(uint8_t(c.reg8(0))&0xfe);break;
   case S::RestoreCount:restoreLong(c,2);break;case S::RestoreIncrement:restoreLong(c,1);break;case S::Return:ret(c);break;default:break;
  }}else if(auto*p=std::get_if<P>(&j.task)){using S=P::Stage;switch(p->stage()){
   case S::BindLatchedIndex:c.reg8(9)=0;val(c,0);break;
   case S::ConsumeLatched:call(c,0x1a08,0x19fe,false);break;
   case S::ClearLatched:c.reg8(8)=0;val(c,0);break;
   case S::ReturnLatched:case S::ReturnPulse:ret(c);break;
   case S::SelectFlags:c.reg32(0)=0xfffe6f;val(c,c.reg32(0));break;
   case S::CompareResetSource:(void)arithmetic(c,uint8_t(c.reg8(10)),255,8,true);break;
   case S::ReadResetTick:call(c,0x946,0x1a22);break;
   case S::ClearResetCount:c.reg32(0)=int32_t(arithmetic(c,uint32_t(c.reg32(0)),uint32_t(c.reg32(0)),32,true));break;
   case S::IncrementPulseCount:incWord(c,10);break;
   case S::ClearPulseFraction:c.reg16(2)=int16_t(arithmetic(c,uint16_t(c.reg16(2)),uint16_t(c.reg16(2)),16,true));break;
   case S::DifferenceTimer:c.reg32(0)=int32_t(arithmetic(c,uint32_t(c.reg32(0)),uint32_t(c.reg32(2)),32,true));break;
   case S::CaptureTimerSign:c.reg16(2)=c.reg16(8);val(c,c.reg16(2));break;
   case S::ExtendTimerSign:c.reg16(2)=int8_t(c.reg16(2));val(c,c.reg16(2));break;
   case S::BindTimerSign:c.reg16(8)=c.reg16(2);val(c,c.reg16(8));break;
   case S::AccumulateTimer:c.reg32(2)=int32_t(arithmetic(c,uint32_t(c.reg32(2)),uint32_t(c.reg32(0)),32,false));break;
   case S::IncrementSamples:c.reg16(9)=int16_t(arithmetic(c,uint16_t(c.reg16(9)),512,16,false));break;
   case S::ReadTick:call(c,0x946,0x1a8e);break;
   case S::RetainTick:c.reg16(8)=c.reg16(0);val(c,c.reg16(8));break;
   case S::ElapsedTick:c.reg16(0)=int16_t(arithmetic(c,uint16_t(c.reg16(0)),uint16_t(c.reg16(1)),16,true));break;
   case S::CompareElapsed:(void)arithmetic(c,uint16_t(c.reg16(0)),20000,16,true);break;
   case S::SelectElapsed:case S::FinishReset:c.clockI(2);break;
   case S::ClearSampleCount:c.reg16(8)=int16_t(arithmetic(c,uint16_t(c.reg16(8)),uint16_t(c.reg16(8)),16,true));break;
   case S::DivideAverage:divSigned(c,9,2);break;
   case S::RetainAverage:saveWord(c,2);break;
   case S::BindRateFactor:c.reg16(1)=int16_t(64000);val(c,c.reg16(1));break;
   case S::MultiplyRate:mulUnsigned(c,9,1);break;case S::DivideRate:divUnsigned(c,0,1);break;
   case S::DifferenceRate:c.reg16(1)=int16_t(arithmetic(c,uint16_t(c.reg16(1)),uint16_t(c.reg16(8)),16,true));break;
   case S::RetainError:c.reg16(2)=c.reg16(1);val(c,c.reg16(2));break;
   case S::MultiplyCorrection:mulSigned(c,9,1);break;case S::DoubleCorrection:left(c,1);break;
   case S::AddEstimate:c.reg32(0)=int32_t(arithmetic(c,uint32_t(c.reg32(0)),uint32_t(c.reg32(1)),32,false));break;
   case S::AddSecondCorrection:c.reg32(1)=int32_t(arithmetic(c,uint32_t(c.reg32(1)),uint32_t(c.reg32(0)),32,false));break;
   case S::QuarterEstimate:case S::QuarterEstimateAgain:right(c,1);break;
   case S::RestoreAverage:restoreWord(c,8);break;
   case S::ClearAverageLow:c.reg16(0)=int16_t(arithmetic(c,uint16_t(c.reg16(0)),uint16_t(c.reg16(0)),16,true));break;
   case S::QuarterAverage:case S::QuarterAverageAgain:right(c,0,true);break;
   case S::DifferencePeriod:c.reg32(1)=int32_t(arithmetic(c,uint32_t(c.reg32(1)),uint32_t(c.reg32(0)),32,true));break;
   case S::AddIntegralError:c.reg16(8)=int16_t(arithmetic(c,uint16_t(c.reg16(8)),uint16_t(c.reg16(2)),16,false));break;
   case S::CompareSaturationGain:(void)arithmetic(c,uint16_t(c.reg16(0)),0x4000,16,true);break;
   case S::ClearIntegral:c.reg16(8)=int16_t(arithmetic(c,uint16_t(c.reg16(8)),uint16_t(c.reg16(8)),16,true));break;
   case S::BindIntegralFactor:c.reg16(0)=0x7e00;val(c,c.reg16(0));break;
   case S::MultiplyIntegral:mulSigned(c,8,0);break;case S::DoubleIntegral:left(c,0);break;
   case S::NegateIntegralMagnitude:c.reg16(8)=int16_t(arithmetic(c,0,uint16_t(c.reg16(8)),16,true));break;
   case S::CompareIntegralThreshold:(void)arithmetic(c,uint16_t(c.reg16(8)),0x1800,16,true);break;
   case S::SetMaximumGain:c.reg16(0)=0x4000;val(c,c.reg16(0));break;
   case S::DecayGain:c.reg16(0)=int16_t(arithmetic(c,uint16_t(c.reg16(0)),0x800,16,true));break;
   case S::CompareGainFloor:(void)arithmetic(c,uint16_t(c.reg16(0)),0x80,16,true);break;
   case S::SetMinimumGain:c.reg16(0)=0x80;val(c,c.reg16(0));break;
   default:break;
  }}else if(auto*r=std::get_if<R>(&j.task)){using S=R::Stage;switch(r->stage()){
   case S::CompareCounter:(void)arithmetic(c,uint16_t(c.reg16(0)),1000,16,true);break;case S::IncrementOverflow:incWord(c,8);break;
   case S::ScaleFirst:case S::ScaleSecond:case S::ScaleThird:case S::ScaleFourth:right(c,0);break;case S::Return:ret(c);break;default:break;
  }}else {using S=W::Stage;switch(std::get<W>(j.task).stage()){
   case S::RetainValue:saveWord(c,0);break;case S::PrepareByte:c.reg8(8)=int8_t(0xf8);val(c,c.reg8(8));break;
   case S::RetainPriority:c.reg8(0)=int8_t(c.ccr);break;case S::MaskPriority:c.ccr|=0xc0;break;
   case S::PrepareEnable:c.reg8(8)=int8_t(0xf0);val(c,c.reg8(8));break;case S::RestorePriority:c.ccr=uint8_t(c.reg8(0));break;
   case S::RestoreValue:restoreWord(c,0);break;case S::Return:ret(c);break;default:break;
  }}
 }
 void advance(Job&j,Stage1Cpu&c,unsigned cost,const ClockCredits&cr){
  unsigned used=0;
  if(auto*t=std::get_if<T>(&j.task))used=t->advanceBefore(cost,cr,[&](T::Read field)->uint32_t{switch(field){case T::Read::Increment:return readLong(c,0xfffe56,1);case T::Read::Phase:return readLong(c,0xfffe52,0);case T::Read::PulseCount:return readWord(c,0xfffe50,10);case T::Read::OutputFlags:return readByte(c,0xfffe6f,9);case T::Read::Secondary:return readLong(c,0xfffe5a,0);case T::Read::SecondaryIncrement:return readLong(c,0xfffe5e,1);case T::Read::RunFlags:return readByte(c,0xfffe6f,0);case T::Read::Timeout:return readByte(c,0xfffe72,8);}return 0;},[&](T::Write field,uint32_t v){switch(field){case T::Write::Phase:writeLong(c,0xfffe52,v,0);break;case T::Write::PulseCount:writeWord(c,0xfffe50,v,10);break;case T::Write::Secondary:writeLong(c,0xfffe5a,v,0);break;case T::Write::Timeout:writeByte(c,0xfffe72,v,8);break;case T::Write::RunFlags:writeByte(c,0xfffe6f,v,0);break;}});
  else if(auto*p=std::get_if<P>(&j.task)){
   const auto stage=p->stage();if(stage==P::Stage::DivideAverage)++metrics_.adaptiveUpdates;
   used=p->advanceBefore(cost,pulseCredits(cr),[&](P::Read field)->uint32_t{switch(field){
    case P::Read::Latched:return readByte(c,0xfffe72,10);case P::Read::Flags:return uint8_t(c.read8(0xfffe6f));case P::Read::Active:return readByte(c,0xfffe6e,8);
    case P::Read::Pulses:return readWord(c,0xfffe50,0);case P::Read::PulseCount:return readWord(c,0xfffe80,10);case P::Read::Timer:return readLong(c,0xfffe50,0);case P::Read::Sum:return readLong(c,0xfffe82,2);case P::Read::Samples:return readWord(c,0xfffe74,9);case P::Read::PreviousTick:return readWord(c,0xfffe76,1);case P::Read::Estimate:return readLong(c,0xfffe78,0);case P::Read::Gain:return readWord(c,0xfffe7c,stage==P::Stage::ReadGain?9:0);case P::Read::Integral:return readWord(c,0xfffe7e,8);
   }return 0;},[&](P::Write field,uint32_t v){switch(field){
    case P::Write::Latched:writeByte(c,0xfffe72,v,8);break;case P::Write::Flags:c.write8(int8_t(v),0xfffe6f);break;
    case P::Write::PreviousTick:writeWord(c,0xfffe76,v,stage==P::Stage::WriteResetTick?0:8);break;
    case P::Write::Samples:writeWord(c,0xfffe74,v,stage==P::Stage::WriteResetCount?0:stage==P::Stage::WriteSamples?9:8);break;
    case P::Write::Sum:writeLong(c,0xfffe82,v,stage==P::Stage::WriteResetSum?0:2);break;case P::Write::PulseCount:writeWord(c,0xfffe80,v,stage==P::Stage::WriteResetPulses?0:10);break;
    case P::Write::SumHigh:writeWord(c,0xfffe82,v,8);break;case P::Write::SumLow:writeWord(c,0xfffe84,v,8);break;case P::Write::Estimate:writeLong(c,0xfffe78,v,1);break;case P::Write::Period:writeLong(c,0xfffe56,v,1);break;case P::Write::Integral:writeWord(c,0xfffe7e,v,8);break;case P::Write::Gain:writeWord(c,0xfffe7c,v,0);break;
   }});
  }else if(auto*r=std::get_if<R>(&j.task))used=r->advanceBefore(cost,cr,[&](R::Read field)->uint32_t{if(field==R::Read::Counter)return readWord(c,0xffff86,0);if(field==R::Read::Overflow)return readWord(c,0xfffd28,8);const uint8_t v=uint8_t(c.read8(0xffff85));bit(c,4,!(v&4));return v;});
  else used=std::get<W>(j.task).advanceBefore(cost,cr,[&](W::Write field,uint32_t v){writeByte(c,field==W::Write::Latch?0xfffe73:0xffffb2,v,8);});
  if(used!=cost)throw std::logic_error("Native K clock admission differs");
 }
 static uint32_t boundary(const Job&j){switch(j.task.index()){
 case 0:{const auto stage=std::get<T>(j.task).stage();switch(stage){
 case T::Stage::RetainIncrement:return 0x1970;
 case T::Stage::RetainCount:return 0x1974;
 case T::Stage::ReadIncrement:return 0x1978;
 case T::Stage::ReadPhase:return 0x1980;
 case T::Stage::ReadPulseCount:return 0x1988;
 case T::Stage::AdvancePhase:return 0x198e;
 case T::Stage::SelectCarry:return 0x1990;
 case T::Stage::IncrementPulseCount:return 0x1992;
 case T::Stage::ReadOutputFlags:return 0x1994;
 case T::Stage::TestOutputFlag:return 0x199a;
 case T::Stage::SelectOutput:return 0x199c;
 case T::Stage::SendClock:return 0x199e;
 case T::Stage::WritePhase:return 0x19a2;
 case T::Stage::WritePulseCount:return 0x19aa;
 case T::Stage::ReadSecondary:return 0x19b0;
 case T::Stage::ReadSecondaryIncrement:return 0x19b8;
 case T::Stage::AdvanceSecondary:return 0x19c0;
 case T::Stage::WriteSecondary:return 0x19c2;
 case T::Stage::ReadRunFlags:return 0x19ca;
 case T::Stage::ReadTimeout:return 0x19d0;
 case T::Stage::AdvanceTimeout:return 0x19d6;
 case T::Stage::SelectTimeout:return 0x19d8;
 case T::Stage::WriteTimeout:return 0x19da;
 case T::Stage::SkipTimeoutClear:return 0x19e0;
 case T::Stage::ClearTimeoutFlag:return 0x19e2;
 case T::Stage::WriteRunFlags:return 0x19e4;
 case T::Stage::RestoreCount:return 0x19ea;
 case T::Stage::RestoreIncrement:return 0x19ee;
 case T::Stage::Return:return 0x19f2;
 default:return 0;}}
 case 1:{const auto stage=std::get<P>(j.task).stage();switch(stage){
 case P::Stage::BindLatchedIndex:return 0x19f4;
 case P::Stage::ReadLatchedSource:return 0x19f6;
 case P::Stage::ConsumeLatched:return 0x19fc;
 case P::Stage::ClearLatched:return 0x19fe;
 case P::Stage::WriteLatched:return 0x1a00;
 case P::Stage::ReturnLatched:return 0x1a06;
 case P::Stage::SelectFlags:return 0x1a08;
 case P::Stage::WriteSourceFlag:return 0x1a0e;
 case P::Stage::ReadActive:return 0x1a12;
 case P::Stage::SelectActive:return 0x1a18;
 case P::Stage::CompareResetSource:return 0x1a1a;
 case P::Stage::SelectResetSource:return 0x1a1c;
 case P::Stage::ReadResetTick:return 0x1a1e;
 case P::Stage::WriteResetTick:return 0x1a22;
 case P::Stage::ClearResetCount:return 0x1a28;
 case P::Stage::WriteResetCount:return 0x1a2a;
 case P::Stage::WriteResetSum:return 0x1a30;
 case P::Stage::ReadResetPulses:return 0x1a38;
 case P::Stage::WriteResetPulses:return 0x1a3e;
 case P::Stage::FinishReset:return 0x1a44;
 case P::Stage::ReadPulseCount:return 0x1a48;
 case P::Stage::IncrementPulseCount:return 0x1a4e;
 case P::Stage::WritePulseCount:return 0x1a50;
 case P::Stage::ClearPulseFraction:return 0x1a56;
 case P::Stage::ReadTimer:return 0x1a58;
 case P::Stage::DifferenceTimer:return 0x1a60;
 case P::Stage::CaptureTimerSign:return 0x1a62;
 case P::Stage::ExtendTimerSign:return 0x1a64;
 case P::Stage::BindTimerSign:return 0x1a66;
 case P::Stage::ReadSum:return 0x1a68;
 case P::Stage::AccumulateTimer:return 0x1a70;
 case P::Stage::WriteSum:return 0x1a72;
 case P::Stage::ReadSamples:return 0x1a7a;
 case P::Stage::IncrementSamples:return 0x1a80;
 case P::Stage::WriteSamples:return 0x1a84;
 case P::Stage::ReadTick:return 0x1a8a;
 case P::Stage::RetainTick:return 0x1a8e;
 case P::Stage::ReadPreviousTick:return 0x1a90;
 case P::Stage::ElapsedTick:return 0x1a96;
 case P::Stage::CompareElapsed:return 0x1a98;
 case P::Stage::SelectElapsed:return 0x1a9c;
 case P::Stage::WritePreviousTick:return 0x1aa0;
 case P::Stage::ClearSampleCount:return 0x1aa6;
 case P::Stage::WriteSampleCount:return 0x1aa8;
 case P::Stage::WriteSumHigh:return 0x1aae;
 case P::Stage::WriteSumLow:return 0x1ab4;
 case P::Stage::DivideAverage:return 0x1aba;
 case P::Stage::RetainAverage:return 0x1abe;
 case P::Stage::BindRateFactor:return 0x1ac0;
 case P::Stage::MultiplyRate:return 0x1ac4;
 case P::Stage::DivideRate:return 0x1ac6;
 case P::Stage::ReadEstimate:return 0x1ac8;
 case P::Stage::DifferenceRate:return 0x1ad0;
 case P::Stage::RetainError:return 0x1ad2;
 case P::Stage::ReadGain:return 0x1ad4;
 case P::Stage::MultiplyCorrection:return 0x1ada;
 case P::Stage::DoubleCorrection:return 0x1ade;
 case P::Stage::AddEstimate:return 0x1ae0;
 case P::Stage::AddSecondCorrection:return 0x1ae2;
 case P::Stage::WriteEstimate:return 0x1ae4;
 case P::Stage::QuarterEstimate:return 0x1aec;
 case P::Stage::QuarterEstimateAgain:return 0x1aee;
 case P::Stage::RestoreAverage:return 0x1af0;
 case P::Stage::ClearAverageLow:return 0x1af2;
 case P::Stage::QuarterAverage:return 0x1af4;
 case P::Stage::QuarterAverageAgain:return 0x1af6;
 case P::Stage::DifferencePeriod:return 0x1af8;
 case P::Stage::WritePeriod:return 0x1afa;
 case P::Stage::ReadIntegral:return 0x1b02;
 case P::Stage::AddIntegralError:return 0x1b08;
 case P::Stage::ReadSaturationGain:return 0x1b0a;
 case P::Stage::CompareSaturationGain:return 0x1b10;
 case P::Stage::SelectSaturationGain:return 0x1b14;
 case P::Stage::ClearIntegral:return 0x1b16;
 case P::Stage::BindIntegralFactor:return 0x1b18;
 case P::Stage::MultiplyIntegral:return 0x1b1c;
 case P::Stage::DoubleIntegral:return 0x1b20;
 case P::Stage::WriteIntegral:return 0x1b22;
 case P::Stage::SelectIntegralSign:return 0x1b28;
 case P::Stage::NegateIntegralMagnitude:return 0x1b2a;
 case P::Stage::CompareIntegralThreshold:return 0x1b2c;
 case P::Stage::SelectIntegralThreshold:return 0x1b30;
 case P::Stage::SetMaximumGain:return 0x1b32;
 case P::Stage::FinishMaximumGain:return 0x1b36;
 case P::Stage::ReadDecayGain:return 0x1b38;
 case P::Stage::DecayGain:return 0x1b3e;
 case P::Stage::SelectGainSign:return 0x1b42;
 case P::Stage::CompareGainFloor:return 0x1b44;
 case P::Stage::SelectGainFloor:return 0x1b48;
 case P::Stage::SetMinimumGain:return 0x1b4a;
 case P::Stage::WriteGain:return 0x1b4e;
 case P::Stage::ReturnPulse:return 0x1b54;
 default:return 0;}}
 case 2:{const auto stage=std::get<R>(j.task).stage();switch(stage){
 case R::Stage::ReadCounter:return 0x946;
 case R::Stage::ReadOverflow:return 0x94a;
 case R::Stage::CompareCounter:return 0x950;
 case R::Stage::SelectCounter:return 0x954;
 case R::Stage::ReadFlag:return 0x956;
 case R::Stage::SelectFlag:return 0x95a;
 case R::Stage::IncrementOverflow:return 0x95c;
 case R::Stage::ScaleFirst:return 0x95e;
 case R::Stage::ScaleSecond:return 0x960;
 case R::Stage::ScaleThird:return 0x962;
 case R::Stage::ScaleFourth:return 0x964;
 case R::Stage::Return:return 0x966;
 default:return 0;}}
 case 3:{const auto stage=std::get<W>(j.task).stage();switch(stage){
 case W::Stage::RetainValue:return 0xaf4;
 case W::Stage::PrepareByte:return 0xaf6;
 case W::Stage::PublishByte:return 0xaf8;
 case W::Stage::RetainPriority:return 0xafe;
 case W::Stage::MaskPriority:return 0xb00;
 case W::Stage::PrepareEnable:return 0xb02;
 case W::Stage::Enable:return 0xb04;
 case W::Stage::RestorePriority:return 0xb06;
 case W::Stage::RestoreValue:return 0xb08;
 case W::Stage::Return:return 0xb0a;
 default:return 0;}}
 }return 0;}
 static unsigned bytes(uint32_t at){switch(at){
 case 0x946: case 0x950: case 0x956: case 0x1970: case 0x1974: case 0x199e: case 0x19ea: case 0x19ee: case 0x1a0e: case 0x1a1e: case 0x1a44: case 0x1a80: case 0x1a8a: case 0x1a98: case 0x1a9c: case 0x1aba: case 0x1ac0: case 0x1ada: case 0x1b10: case 0x1b18: case 0x1b1c: case 0x1b2c: case 0x1b32: case 0x1b3e: case 0x1b44: case 0x1b4a:return 4;
 case 0x94a: case 0xaf8: case 0x1988: case 0x1994: case 0x19aa: case 0x19ca: case 0x19d0: case 0x19da: case 0x19e4: case 0x19f6: case 0x1a00: case 0x1a08: case 0x1a12: case 0x1a22: case 0x1a2a: case 0x1a38: case 0x1a3e: case 0x1a48: case 0x1a50: case 0x1a7a: case 0x1a84: case 0x1a90: case 0x1aa0: case 0x1aa8: case 0x1aae: case 0x1ab4: case 0x1ad4: case 0x1b02: case 0x1b0a: case 0x1b22: case 0x1b38: case 0x1b4e:return 6;
 case 0x1978: case 0x1980: case 0x19a2: case 0x19b0: case 0x19b8: case 0x19c2: case 0x1a30: case 0x1a58: case 0x1a68: case 0x1a72: case 0x1ac8: case 0x1ae4: case 0x1afa:return 8;
 default:return 2;}}
 std::array<Job,16>jobs_{};unsigned depth_=0;Metrics metrics_{};
};
}
