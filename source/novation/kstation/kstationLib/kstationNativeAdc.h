#pragma once
#include "kstationAdcService.h"
#include "kstationHdiWait.h"
#include <array>
#include <stdexcept>

namespace kstation {
// Temporary projection of the typed ADC continuation onto the legacy caller
// frame. No instruction decoding takes place in this adapter.
class NativeAdc final {
 using T=AdcInterruptTask;using S=T::Stage;using A=AdcSampleTask;using C=A::Stage;
 struct Desc {uint32_t at;unsigned length,stack,reg;};
inline static constexpr Desc root[]={{0x670,2,1,0},{0x672,6,0,0},{0x678,4,0,0},{0x67c,2,0,0},{0x67e,2,0,0},{0x680,6,0,0},{0x686,2,0,0},{0x688,2,0,0},{0x68a,2,0,0},{0x68c,2,0,0},{0x68e,2,0,0},{0x690,2,2,0},{0x692,2,8,0},{0x694,2,1,8},{0x696,4,3,1},{0x69a,4,3,2},{0x69e,2,1,3},{0x6a0,4,3,4},{0x6a4,2,5,0},{0x6a6,4,4,4},{0x6aa,2,2,3},{0x6ac,4,4,2},{0x6b0,4,4,1},{0x6b4,2,2,8},{0x6b6,2,2,0},{0x6b8,2,8,0},{0x6f8,2,0,0},{0x6fa,2,0,0},{0x6fc,2,0,0},{0x6fe,2,1,8},{0x700,4,3,1},{0x704,4,3,2},{0x708,2,1,3},{0x70a,4,3,4},{0x70e,2,5,0},{0x710,4,4,4},{0x714,2,0,0},{0x716,6,0,0},{0x71c,2,0,0},{0x71e,2,0,0},{0x720,6,0,0},{0x726,2,0,0},{0x728,4,0,0},{0x72c,2,0,0},{0x72e,4,0,0},{0x732,4,0,0},{0x736,2,0,0},{0x738,4,0,0},{0x73c,2,0,0},{0x73e,4,0,0},{0x742,2,0,0},{0x744,2,0,0},{0x746,2,0,0},{0x748,4,0,0},{0x74c,4,0,0},{0x750,2,0,0},{0x752,2,0,0},{0x754,2,0,0},{0x756,4,0,0},{0x75a,2,0,0},{0x75c,4,0,0},{0x760,2,0,0},{0x762,2,1,1},{0x764,6,0,0},{0x76a,2,0,0},{0x76c,2,0,0},{0x76e,2,2,1},{0x770,4,0,0},{0x774,2,0,0},{0x776,4,0,0},{0x77a,2,0,0},{0x77c,2,0,0},{0x77e,2,0,0},{0x780,2,0,0},{0x782,6,0,0},{0x788,2,0,0},{0x78a,2,0,0},{0x78c,6,0,0},{0x792,2,0,0},{0x794,2,0,0},{0x796,2,0,0},{0x798,4,0,0},{0x79c,2,0,0},{0x79e,2,0,0},{0x7a0,2,0,0},{0x7a2,2,0,0},{0x7a4,6,0,0},{0x7aa,2,0,0},{0x7ac,2,0,0},{0x7ae,6,0,0},{0x7b4,2,0,0},{0x7b6,2,0,0},{0x7b8,2,0,0},{0x7ba,2,5,0},{0x7f0,4,3,4},{0x7f4,4,3,5},{0x7f8,2,0,0},{0x7fa,2,0,0},{0x7fc,2,0,0},{0x7fe,6,0,0},{0x804,2,0,0},{0x806,2,0,0},{0x808,6,0,0},{0x80e,2,0,0},{0x810,6,0,0},{0x816,2,0,0},{0x818,2,0,0},{0x81a,2,0,0},{0x81c,4,0,0},{0x820,6,0,0},{0x826,4,4,5},{0x82a,4,4,4},{0x82e,2,7,0},{0x7bc,2,0,0},{0x7be,2,0,0},{0x7c0,2,0,0},{0x7c2,4,0,0},{0x7c6,6,0,0},{0x7cc,2,0,0},{0x7ce,6,0,0},{0x7d4,2,0,0},{0x7d6,2,0,0},{0x7d8,4,6,0},{0x65a,6,0,0},{0x660,2,0,0},{0x662,2,0,0},{0x664,2,0,0},{0x666,2,0,0},{0x668,6,0,0},{0x66e,2,7,0},{0x7dc,2,0,0},{0x7de,2,0,0},{0x7e0,2,2,3},{0x7e2,4,4,2},{0x7e6,4,4,1},{0x7ea,2,2,8},{0x7ec,2,2,0},{0x7ee,2,8,0}};
inline static constexpr Desc child[]={{0x6ba,2,0,0},{0x6bc,6,0,0},{0x6c2,2,0,0},{0x6c4,6,0,0},{0x6ca,6,0,0},{0x6d0,2,0,0},{0x6d2,2,0,0},{0x6d4,2,0,0},{0x6d6,2,0,0},{0x6d8,4,0,0},{0x6dc,2,0,0},{0x6de,2,0,0},{0x6e0,4,0,0},{0x6e4,2,0,0},{0x6e6,2,0,0},{0x6e8,2,0,0},{0x6ea,4,0,0},{0x6ee,2,0,0},{0x6f0,2,0,0},{0x6f2,2,0,0},{0x6f4,2,0,0},{0x6f6,2,7,0}};
 struct Job {T task{};A sample{};bool sampling=false;};
public:
 struct CodeRange {uint32_t begin,end;};
 inline static constexpr std::array<CodeRange,3> CodeRanges{{{60*4,60*4+4},{0x65a,0x830},{0x84d2,0x85d2}}};
 struct Result {bool stepped=false,entered=false,returned=false;};
 static bool candidate(uint32_t pc,uint64_t pending){return pending||(pc>=0x65a&&pc<0x830);}
 bool active()const{return depth_!=0;}unsigned depth()const{return depth_;}void reset(){depth_=0;}
 Result step(Stage1Cpu& cpu){
  if(cpu.interruptMode2||cpu.pending_irqs_high)return {};
  bool enter=false;
  if(cpu.pending_irqs&&!(cpu.ccr&0x80)){
   if((cpu.pending_irqs&((uint64_t(1)<<60)-1))||!(cpu.pending_irqs&(uint64_t(1)<<60)))return {};
   if(depth_==jobs_.size())throw std::logic_error("Native K ADC continuation capacity");enter=true;
  } else if(!depth_||uint32_t(cpu.getPC())!=boundary(jobs_[depth_-1])){
   const auto pc=uint32_t(cpu.getPC());if(pc>=0x65a&&pc<0x830)throw std::logic_error("Native K ADC unowned boundary");return {};
  }
  if(cpu.pending_irqs){const bool ue=(cpu.read8(0xfffff2)&8)!=0;if(enter){
   cpu.pending_irqs&=~(uint64_t(1)<<60);const auto saved=(uint32_t(cpu.getPC())&0xffffff)|(uint32_t(cpu.ccr)<<24);
   const auto sp=cpu.getSP()-4;cpu.write32(int32_t(saved),int(sp));cpu.setSP(sp);cpu.clockI(4);
   cpu.ccr|=0x80;if(!ue)cpu.ccr|=0x10;
   if((cpu.read32(60*4)&0xffffff)!=0x670)throw std::logic_error("Native K ADC vector binding changed");
   cpu.pc=cpu.makepc(0x670);cpu.irqDepth(cpu.irqDepth()+1);jobs_[depth_++]=Job{};
  }}
  auto&j=jobs_[depth_-1];const auto d=j.sampling?child[unsigned(j.sample.stage())]:root[unsigned(j.task.stage())];
  if(uint32_t(cpu.getPC())!=d.at)throw std::logic_error("Native K ADC caller boundary differs");
  unsigned rc=0,wc=0;uint32_t ra=0,wa=0;unsigned rn=0,wn=0;
  if(j.sampling) sampleAccess(j.sample,ra,rn,wa,wn);else rootAccess(j.task,ra,rn,wa,wn);
  if(rn)rc=busCost(ra,rn,int(d.at+d.length-1));if(wn)wc=busCost(wa,wn,cpu.writeHistory());
  unsigned sc=stackCost(cpu,d);const auto begin=cpu.cycles;
  cpu.fetchClocks(d.length);cpu.pc=cpu.makepc(int(d.at+1));
  if(d.stack)stackProject(cpu,d);else if(j.sampling)sampleProject(cpu,j.sample.stage());else rootProject(cpu,j.task.stage());
  unsigned cost,used;
  if(j.sampling){A::Credits credits{rc,wc,sc};cost=j.sample.nextClocks(credits);
   used=j.sample.advanceBefore(cost,credits,[&](A::Read r){return sampleRead(cpu,j.sample,r);},[&](uint16_t v){sampleWrite(cpu,j.sample,v);});
   if(j.sample.complete()){j.sampling=false;if(!j.task.resumeSample())throw std::logic_error("ADC sample resume");}
  }else{T::Credits credits{rc,wc,sc};cost=j.task.nextClocks(credits);
   used=j.task.advanceBefore(cost,credits,[&](T::Read r){return rootRead(cpu,j.task,r);},[&](T::Write w,uint16_t v){rootWrite(cpu,j.task,w,v);});
   if(j.task.awaitingSample()){j.sample=A{};j.sampling=true;}
  }
  if(used!=cost||cpu.cycles-begin!=cost)throw std::logic_error("Native K ADC phase clocks differ");
  const bool returned=j.task.complete();if(returned)--depth_;else {const auto next=boundary(j);if(d.stack==7&&uint32_t(cpu.getPC())!=next)throw std::logic_error("Native K ADC return frame changed");cpu.pc=cpu.makepc(int(next));}
  return {true,enter,returned};
 }
private:
 static uint32_t boundary(const Job&j){return j.sampling?child[unsigned(j.sample.stage())].at:root[unsigned(j.task.stage())].at;}
 static unsigned busCost(uint32_t at,unsigned bytes,int previous){unsigned cost=0;for(unsigned n=0;n<bytes;++n){const auto a=(at+n)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((previous&1)||a!=uint32_t(previous|1))cost+=a<0x400000?3:1;previous=int(a);}else{cost+=a>=0xffffa0?1:3;previous=-1;}}return cost;}
 static unsigned stackCost(Stage1Cpu&c,const Desc&d){if(!d.stack)return 0;const bool push=d.stack==1||d.stack==3||d.stack==5||d.stack==6;const unsigned size=(d.stack==1||d.stack==2)?2:4;const auto at=push?c.getSP()-size:c.getSP();const int prev=push?c.writeHistory():int(d.at+d.length-1);const unsigned extra=d.stack==5?0:2;return 3*(d.length/2)+extra+busCost(at,size,prev);}
 static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(v==0?4:0));}
 static uint32_t arithmetic(Stage1Cpu&c,unsigned bits,uint32_t a,uint32_t b,bool sub){const uint32_t mask=bits==32?0xffffffffu:((1u<<bits)-1),sign=1u<<(bits-1),half=sign>>4;const uint32_t r=(sub?a-b:a+b)&mask;const uint32_t carry=sub?((~a&b)|(~a&r)|(b&r)):((a&b)|(a&~r)|(b&~r));const uint32_t ov=sub?((a&~b&~r)|(~a&b&r)):((a&b&~r)|(~a&~b&r));c.ccr=uint8_t((c.ccr&~0x2f)|(r?0:4)|((r&sign)?8:0)|((carry&sign)?1:0)|((carry&half)?32:0)|((ov&sign)?2:0));return r;}
 template<class R>static void increment(Stage1Cpu&c,R&v,int direction,unsigned amount,unsigned bits){const auto old=int64_t(v),r=old+direction*int64_t(amount);val(c,int(r));const uint64_t sign=uint64_t(1)<<(bits-1);if(direction>0?((~uint64_t(old)&uint64_t(r)&sign)!=0):((uint64_t(old)&~uint64_t(r)&sign)!=0))c.ccr|=2;v=R(r);}
 template<class R>static void shift(Stage1Cpu&c,R&v,unsigned bits,bool left){uint32_t x=uint32_t(v)&(bits==32?0xffffffffu:((1u<<bits)-1));const bool carry=left?((x>>(bits-1))&1):(x&1);uint32_t result=left?x<<1:x>>1;c.ccr=uint8_t((c.ccr&~1)|(carry?1:0));val(c,int32_t(result<<(32-bits)));v=R(result);}
 static void stackProject(Stage1Cpu&c,const Desc&d){const auto k=d.stack;if(k==1||k==3){const unsigned size=k==1?2:4;const auto sp=c.getSP()-size;c.setSP(sp);if(size==2)c.write16(c.reg16(d.reg),int(sp));else c.write32(c.reg32(d.reg),int(sp));c.clockI(2);if(size==2)val(c,c.reg16(d.reg));else val(c,c.reg32(d.reg));}
  else if(k==2||k==4){const unsigned size=k==2?2:4;const auto sp=c.getSP();if(size==2)c.reg16(d.reg)=int16_t(c.read16(int(sp)));else c.reg32(d.reg)=int32_t(c.read32(int(sp)));c.setSP(sp+size);c.clockI(2);if(size==2)val(c,c.reg16(d.reg));else val(c,c.reg32(d.reg));}
  else if(k==5||k==6){const auto sp=c.getSP()-4;c.write32(int32_t(d.at+d.length),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);if(k==6)c.clockI(2);}
  else {c.irqDepth(c.irqDepth()-1);c.clockI(2);const auto sp=c.getSP();c.setSP(sp+4);const auto saved=uint32_t(c.read32(int(sp)));if(k==8)c.ccr=uint8_t(saved>>24);c.pc=c.makepc(int(saved&0xffffff));}}
 static void rootProject(Stage1Cpu&c,S s){switch(s){
case S::SelectScan:c.clockI(2);break;
case S::RetainRemaining:c.reg8(0)=c.reg8(8);val(c,c.reg8(0));break;
case S::DecrementRemaining:increment(c,c.reg8(8),-1,1,8);break;
case S::MaskBusy:c.reg8(8)&=127u;val(c,c.reg8(8));break;
case S::CompareRemaining:arithmetic(c,8,c.reg8(0),1u,true);break;
case S::MaskStatus:c.reg8(8)&=95u;val(c,c.reg8(8));break;
case S::ClearGroup:c.reg32(0)=arithmetic(c,32,c.reg32(0),c.reg32(0),true);break;
case S::CaptureChannel:c.reg8(11)=c.reg8(8);val(c,c.reg8(11));break;
case S::ScaleGroup:shift(c,c.reg8(8),8,true);break;
case S::LocateHistory:c.reg32(0)=arithmetic(c,32,c.reg32(0),50128u,false);break;
case S::SetCount:c.reg8(3)=4u;val(c,c.reg8(3));break;
case S::SetThreshold:c.reg16(10)=512u;val(c,c.reg16(10));break;
case S::SetArmedThreshold:c.reg16(10)=2048u;val(c,c.reg16(10));break;
case S::Difference:c.reg16(9)=arithmetic(c,16,c.reg16(9),c.reg16(1),true);break;
case S::SelectStable:c.clockI(2);break;
case S::NegateDifference:c.reg16(9)=arithmetic(c,16,0,c.reg16(9),true);break;
case S::CompareNegative:arithmetic(c,16,c.reg16(9),c.reg16(10),true);break;
case S::SelectNegative:c.clockI(2);break;
case S::AdjustNegative:c.reg16(1)=arithmetic(c,16,c.reg16(1),256u,true);break;
case S::ComparePositive:arithmetic(c,16,c.reg16(9),c.reg16(10),true);break;
case S::AdjustPositive:c.reg16(1)=arithmetic(c,16,c.reg16(1),256u,false);break;
case S::ClearArmed:c.reg8(10)=arithmetic(c,8,c.reg8(10),c.reg8(10),true);break;
case S::NormalizeScaleGroup:c.reg16(1)&=255;val(c,c.reg16(1));break;
case S::RetainScaleGroup:c.reg16(9)=c.reg16(1);val(c,c.reg16(9));break;
case S::CompareSpecial:arithmetic(c,16,c.reg16(9),28u,true);break;
case S::SetSpecialFactor:c.reg16(9)=33287u;val(c,c.reg16(9));break;
case S::MultiplySpecial:c.reg32(1)=uint32_t(uint16_t(c.reg16(9)))*uint16_t(c.reg32(1));c.clockI(20);break;
case S::ExtractSpecial:c.reg16(1)=c.reg16(9);val(c,c.reg16(1));break;
case S::ClearSpecialHigh:c.reg16(9)=arithmetic(c,16,c.reg16(9),c.reg16(9),true);break;
case S::DoubleSpecial:shift(c,c.reg32(1),32,true);break;
case S::BiasSpecial:c.reg32(1)=arithmetic(c,32,c.reg32(1),512u,true);break;
case S::ClearSpecialFloor:c.reg32(1)=arithmetic(c,32,c.reg32(1),c.reg32(1),true);break;
case S::CompareSpecialCeiling:arithmetic(c,32,c.reg32(1),65280u,true);break;
case S::ClampSpecial:c.reg8(1)=-1;val(c,c.reg8(1));break;
case S::SetNormalFactor:c.reg16(9)=16708u;val(c,c.reg16(9));break;
case S::MultiplyNormal:c.reg32(1)=uint32_t(uint16_t(c.reg16(9)))*uint16_t(c.reg32(1));c.clockI(20);break;
case S::ExtractNormal:c.reg16(1)=c.reg16(9);val(c,c.reg16(1));break;
case S::ClearNormalHigh:c.reg16(9)=arithmetic(c,16,c.reg16(9),c.reg16(9),true);break;
case S::DoubleNormal:shift(c,c.reg32(1),32,true);break;
case S::BiasNormal:c.reg32(1)=arithmetic(c,32,c.reg32(1),256u,true);break;
case S::ClearNormalFloor:c.reg32(1)=arithmetic(c,32,c.reg32(1),c.reg32(1),true);break;
case S::CompareNormalCeiling:arithmetic(c,32,c.reg32(1),32512u,true);break;
case S::ClampNormal:c.reg8(1)=127u;val(c,c.reg8(1));break;
case S::BindEvent:c.reg8(9)=c.reg8(11);val(c,c.reg8(9));break;
case S::CopyEventIndex:c.reg8(12)=c.reg8(9);val(c,c.reg8(12));break;
case S::NormalizeEventWord:c.reg16(4)&=255;val(c,c.reg16(4));break;
case S::NormalizeEventLong:c.reg32(4)&=65535;val(c,c.reg32(4));break;
case S::LocateMapping:c.reg32(4)=arithmetic(c,32,c.reg32(4),34002u,false);break;
case S::NormalizeProducer:c.reg32(4)&=65535;val(c,c.reg32(4));break;
case S::LocateQueue:c.reg32(5)=49616u;val(c,c.reg32(5));break;
case S::LocateQueueEntry:c.reg32(5)=arithmetic(c,32,c.reg32(5),c.reg32(4),false);break;
case S::AdvanceProducer:increment(c,c.reg16(4),1,2,16);break;
case S::MaskProducer:c.reg16(4)&=510u;val(c,c.reg16(4));break;
case S::AdvanceWord:increment(c,c.reg32(0),1,2,32);break;
case S::AdvanceChannel:increment(c,c.reg8(11),1,1,8);break;
case S::DecrementCount:increment(c,c.reg8(3),-1,1,8);break;
case S::MoreChannels:c.clockI(2);break;
case S::IncrementGroup:c.reg8(8)=arithmetic(c,8,c.reg8(8),4u,false);break;
case S::CompareNextGroup:arithmetic(c,8,c.reg8(8),28u,true);break;
case S::ShiftMuxFirst:shift(c,c.reg8(8),8,false);break;
case S::ShiftMuxSecond:shift(c,c.reg8(8),8,false);break;
case S::BindSettleCount:c.reg8(0)=9u;val(c,c.reg8(0));break;
case S::SetConversion:c.reg8(8)=115u;val(c,c.reg8(8));break;
 default:break;}}
 static void sampleProject(Stage1Cpu&c,C s){switch(s){
case C::ClearGroup:c.reg32(0)=arithmetic(c,32,c.reg32(0),c.reg32(0),true);break;
case C::ScaleGroup:shift(c,c.reg8(8),8,true);break;
case C::SelectSamples:c.reg32(0)=arithmetic(c,32,c.reg32(0),50192u,false);break;
case C::SelectAdc:c.reg32(2)=16777184u;val(c,c.reg32(2));break;
case C::SetCount:c.reg8(3)=4u;val(c,c.reg8(3));break;
case C::SetRawWeight:c.reg16(4)=32768u;val(c,c.reg16(4));break;
case C::WeightRaw:c.reg32(4)=uint32_t(uint16_t(c.reg16(1)))*uint16_t(c.reg32(4));c.clockI(20);break;
case C::BindRawHalf:c.reg16(1)=c.reg16(12);val(c,c.reg16(1));break;
case C::SetPreviousWeight:c.reg16(4)=32768u;val(c,c.reg16(4));break;
case C::WeightPrevious:c.reg32(4)=uint32_t(uint16_t(c.reg16(9)))*uint16_t(c.reg32(4));c.clockI(20);break;
case C::AddHalves:c.reg16(1)=arithmetic(c,16,c.reg16(1),c.reg16(12),false);break;
case C::Saturate:c.reg16(1)=65535u;val(c,c.reg16(1));break;
case C::AdvanceSample:increment(c,c.reg32(0),1,2,32);break;
case C::DecrementCount:increment(c,c.reg8(3),-1,1,8);break;
 default:break;}}
 static void sampleAccess(const A&t,uint32_t&ra,unsigned&rn,uint32_t&wa,unsigned&wn){switch(t.stage()){
 case C::ReadGroup:ra=0xc490;rn=1;break;case C::ReadHigh:ra=t.adcAddress();rn=1;break;case C::ReadLow:ra=t.adcAddress()+1;rn=1;break;case C::ReadPrevious:ra=t.sampleAddress();rn=2;break;case C::WriteSample:wa=t.sampleAddress();wn=2;break;default:break;}}
 static void rootAccess(const T&t,uint32_t&ra,unsigned&rn,uint32_t&wa,unsigned&wn){switch(t.stage()){
 case S::ReadRemaining:ra=0xc491;rn=1;break;case S::ReadBusy:case S::ReadStatus:ra=0xffffe8;rn=1;break;
 case S::ReadGroup:case S::ReadScaleGroup:case S::ReadNextGroup:case S::ReadMuxGroup:ra=0xc490;rn=1;break;
 case S::ReadCurrent:ra=t.currentAddress();rn=2;break;case S::ReadHistory:ra=t.historyAddress();rn=2;break;
 case S::ReadArmed:ra=t.armedAddress();rn=1;break;case S::ReadMapping:ra=t.mappingAddress();rn=1;break;case S::ReadProducer:ra=0xc1cc;rn=2;break;
 case S::WriteRemaining:case S::WriteSettleCount:wa=0xc491;wn=1;break;case S::WriteBusy:case S::WriteStatus:case S::WriteConversion:wa=0xffffe8;wn=1;break;
 case S::WriteArmed:wa=t.armedAddress();wn=1;break;case S::WriteHistory:wa=t.historyAddress();wn=2;break;
 case S::WriteEvent:wa=t.eventAddress();wn=2;break;case S::WriteProducer:wa=0xc1cc;wn=2;break;
 case S::WriteNextGroup:wa=0xc490;wn=1;break;case S::WriteMux:wa=0xffffcb;wn=1;break;default:break;}}
 static uint32_t sampleRead(Stage1Cpu&c,const A&t,A::Read r){switch(r){
 case A::Read::Group:c.reg8(8)=c.read8(0xc490);val(c,c.reg8(8));return uint8_t(c.reg8(8));
 case A::Read::High:c.reg8(1)=c.read8(int(t.adcAddress()));++c.reg32(2);val(c,c.reg8(1));c.clockI(2);return uint8_t(c.reg8(1));
 case A::Read::Low:c.reg8(9)=c.read8(int(t.adcAddress()+1));++c.reg32(2);val(c,c.reg8(9));c.clockI(2);return uint8_t(c.reg8(9));
 case A::Read::Previous:c.reg16(9)=int16_t(c.read16(int(t.sampleAddress())));val(c,c.reg16(9));return uint16_t(c.reg16(9));}throw std::logic_error("ADC sample read");}
 static void sampleWrite(Stage1Cpu&c,const A&t,uint16_t v){if(uint16_t(c.reg16(1))!=v)throw std::logic_error("ADC sample projection");c.write16(int16_t(v),int(t.sampleAddress()));val(c,c.reg16(1));}
 static uint32_t rootRead(Stage1Cpu&c,const T&t,T::Read r){int reg=8;uint32_t at=0;bool word=false;
  switch(r){case T::Read::Remaining:at=0xc491;break;case T::Read::Status:at=0xffffe8;break;
  case T::Read::Group:case T::Read::NextGroup:case T::Read::MuxGroup:at=0xc490;break;
  case T::Read::ScaleGroup:at=0xc490;reg=9;break;case T::Read::Current:at=t.currentAddress();reg=1;word=true;break;
  case T::Read::History:at=t.historyAddress();reg=9;word=true;break;case T::Read::Armed:at=t.armedAddress();reg=10;break;
  case T::Read::Mapping:at=t.mappingAddress();reg=9;break;case T::Read::Producer:at=0xc1cc;reg=4;word=true;break;}
  if(word){c.reg16(reg)=int16_t(c.read16(int(at)));val(c,c.reg16(reg));return uint16_t(c.reg16(reg));}
  c.reg8(reg)=c.read8(int(at));val(c,c.reg8(reg));return uint8_t(c.reg8(reg));
 }
 static void rootWrite(Stage1Cpu&c,const T&t,T::Write w,uint16_t v){int reg=8;uint32_t at=0;bool word=false,before=false;
  switch(w){case T::Write::Remaining:at=0xc491;before=true;break;case T::Write::Status:at=0xffffe8;before=true;break;
  case T::Write::Armed:at=t.armedAddress();reg=10;break;case T::Write::History:at=t.historyAddress();reg=1;word=true;break;
  case T::Write::Event:at=t.eventAddress();reg=1;word=true;break;case T::Write::Producer:at=0xc1cc;reg=4;word=true;break;
  case T::Write::Group:at=0xc490;before=true;break;case T::Write::Mux:at=0xffffcb;before=true;break;
  case T::Write::SettleCount:at=0xc491;reg=0;before=true;break;}
  if(word){if(uint16_t(c.reg16(reg))!=v)throw std::logic_error("ADC word projection");if(before)val(c,c.reg16(reg));c.write16(int16_t(v),int(at));if(!before)val(c,c.reg16(reg));}
  else{if(uint8_t(c.reg8(reg))!=uint8_t(v))throw std::logic_error("ADC byte projection");if(before)val(c,c.reg8(reg));c.write8(int8_t(v),int(at));if(!before)val(c,c.reg8(reg));}
 }
 std::array<Job,8>jobs_{};unsigned depth_=0;
};// Foreground owner of the mux helper; the IRQ graph owns its own embedded
// continuation and must never fall back to the legacy decoder.
class NativeAdcMux final {
 using T=AdcMuxTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x65a,0x670}}};
 const T&task()const{return task_;}bool complete()const{return task_.complete();}
 uint32_t boundary()const{constexpr uint32_t pc[]={0x65a,0x660,0x662,0x664,0x666,0x668,0x66e};return complete()?0:pc[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80))||complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);const auto s=task_.stage();const auto at=boundary();const unsigned len=s==S::ReadGroup||s==S::WriteCount?6:2;T::Credits cr{3,s==S::WriteMux?1u:3u,5+busCost(c.getSP(),4,int(at+len-1))};const auto cost=task_.nextClocks(cr);const auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){
 case S::ShiftFirst:case S::ShiftSecond:{auto v=uint8_t(c.reg8(8));c.ccr=uint8_t((c.ccr&~1)|(v&1));v>>=1;c.reg8(8)=int8_t(v);val(c,c.reg8(8));break;}
 case S::BindCount:c.reg8(0)=9;val(c,c.reg8(0));break;
 case S::Return:{c.irqDepth(c.irqDepth()-1);c.clockI(2);const auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));break;}
 default:break;}
 const auto used=task_.advanceBefore(cost,cr,[&](){c.reg8(8)=c.read8(0xc490);val(c,c.reg8(8));return uint8_t(c.reg8(8));},[&](T::Write w,uint8_t v){const int reg=w==T::Write::Mux?8:0;const int at=w==T::Write::Mux?0xffffcb:0xc491;if(uint8_t(c.reg8(reg))!=v)throw std::logic_error("K ADC mux projection differs");val(c,c.reg8(reg));c.write8(int8_t(v),at);});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K ADC mux phase clocks differ");if(!complete())c.pc=c.makepc(int(boundary()));return true;}
private:
 static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static unsigned busCost(uint32_t at,unsigned n,int prev){unsigned cost=0;for(unsigned i=0;i<n;++i){const auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((prev&1)||a!=uint32_t(prev|1))cost+=a<0x400000?3:1;prev=int(a);}else{cost+=a>=0xffffa0?1:3;prev=-1;}}return cost;}
 T task_{};
};

}
