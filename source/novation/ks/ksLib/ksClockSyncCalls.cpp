#include "ksClockSyncCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
ClockSyncPartialTest partialTest=nullptr;using T=ClockSyncTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,88> points{{{0x267a,6},{0x2680,2},{0x2682,6},{0x2688,2},{0x268a,2},{0x268c,2},{0x268e,6},{0x2694,6},{0x269a,8},{0x26a2,2},{0x26a4,4},{0x26a8,2},{0x26aa,2},{0x26ac,2},{0x26ae,2},{0x26b0,4},{0x26b4,2},{0x26b6,6},{0x26bc,2},{0x26be,8},{0x26c6,2},{0x26c8,2},{0x26ca,2},{0x26cc,2},{0x26ce,2},{0x26d0,4},{0x26d4,2},{0x26d6,8},{0x26de,8},{0x26e6,2},{0x26e8,6},{0x26ee,2},{0x26f0,6},{0x26f6,8},{0x26fe,2},{0x2700,6},{0x2706,4},{0x270a,2},{0x270c,2},{0x270e,2},{0x2710,8},{0x2718,6},{0x271e,2},{0x2720,2},{0x2722,2},{0x2724,2},{0x2726,2},{0x2728,2},{0x272a,2},{0x272c,2},{0x272e,2},{0x2730,2},{0x2732,2},{0x2734,2},{0x2736,6},{0x273c,2},{0x273e,6},{0x2744,4},{0x2748,2},{0x274a,4},{0x274e,2},{0x2750,2},{0x2752,6},{0x2758,2},{0x275a,8},{0x2762,2},{0x2764,2},{0x2766,2},{0x2768,6},{0x276e,2},{0x2770,8},{0x2778,4},{0x277c,4},{0x2780,8},{0x2788,6},{0x278e,8},{0x2796,2},{0x2798,2},{0x279a,8},{0x27a2,2},{0x27a4,4},{0x27a8,2},{0x27aa,4},{0x27ae,4},{0x27b2,4},{0x27b6,2},{0x27b8,4},{0x27bc,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
}
void setClockSyncPartialForTests(ClockSyncPartialTest value){partialTest=value;}
bool ClockSyncCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 if(pc==0x267a){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native clock sync capacity exceeded");slot->emplace(T::Kind::Update,h.getSP());++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field)->uint32_t{switch(field){case T::Read::Divider:return uint8_t(h.read8(0xffee98));case T::Read::Source:return uint8_t(h.read8(0x169ef));case T::Read::Flags:return uint8_t(h.read8(0xffee97));case T::Read::InternalRate:return uint8_t(h.read8(0x16950));case T::Read::Period:return uint32_t(h.read32(0xffee7e));case T::Read::Active:return uint8_t(h.read8(0xffee96));case T::Read::Signal:return uint16_t(h.read16(0xffeea0));case T::Read::Synchronized:return uint32_t(h.read32(0xffee8e));case T::Read::Gain:return uint16_t(h.read16(0xffeea4));case T::Read::Coefficient:return uint32_t(h.read32(0xffee92));case T::Read::PreviousPeriod:return uint32_t(h.read32(0xffee8a));}return 0;};
 auto write=[&](T::Write field,uint32_t value){switch(field){case T::Write::Divider:f.store8(0xffee98,int8_t(value));break;case T::Write::Flags:h.write8(int8_t(value),0xffee97);break;case T::Write::Active:f.store8(0xffee96,int8_t(value));break;case T::Write::Period:f.store32(0xffee7e,int32_t(value));break;case T::Write::Synchronized:f.store32(0xffee8e,int32_t(value));break;case T::Write::Gain:f.store16(0xffeea4,int16_t(value));break;case T::Write::Coefficient:f.store32(0xffee92,int32_t(value));break;case T::Write::PreviousPeriod:f.store32(0xffee8a,int32_t(value));break;}};
 if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native clock sync admission differs");
 switch(stage){case S::ReadDivider:f.move(r[0].rl,task.divider());break;
case S::ToggleDivider:r[0].rl^=0x1;value(h,r[0].rl);break;
case S::TestDivider:flag(h,4,(uint8_t(r[0].rl)&(1u<<0x0))==0);break;
case S::ReturnIdle:f.ret();break;
case S::SelectFlags:f.move(r[2].er,0xffee97);break;
case S::SelectGlobals:f.move(r[1].er,0x168fc);break;
case S::ReadSource:f.move(r[0].rl,task.source());break;
case S::ReadSourceFlags:f.move(r[0].rl,task.source());break;
case S::DecrementSource:f.inc(r[0].rl,-1);break;
case S::CaptureSourceFlag:flag(h,1,uint8_t(r[0].rl)&(1u<<0x1));break;
case S::MaskActive:r[0].rl&=0x1;value(h,r[0].rl);break;
case S::ReadInternalRate:f.move(r[0].rl,task.internal());break;
case S::BiasInternalRate:r[0].rl=h.add8(r[0].rl,0x40);break;
case S::NormalizeInternalRate:r[0].r&=0xff;value(h,r[0].r);break;
case S::ScaleInternalFour:f.shift(r[0].r,2,true);break;
case S::ScaleInternalSixteen:f.shift(r[0].r,2,true);break;
case S::ScaleInternalThirtyTwo:f.shift(r[0].r,1,true);break;
case S::BindInternalWeight:f.move(r[0].e,0xd1b7);break;
case S::MultiplyInternalRate:r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20);break;
case S::ReadPeriod:f.move(r[2].er,task.rate());break;
case S::ScalePeriod:f.shift(r[2].er,2,true);break;
case S::ReadActive:f.move(r[0].rl,task.active());break;
case S::ReadSignal:f.move(r[0].r,task.signal());break;
case S::ReadSynchronized:f.move(r[2].er,task.rate());break;
case S::ComputeSignalError:r[0].r=h.sub16(r[0].r,r[2].e);break;
case S::ReadGain:f.move(r[0].e,task.gain());break;
case S::ComputeCorrection:f.multiplySigned(r[0].e,r[0].er);break;
case S::DoubleCorrection:f.shift(r[0].er,1,true);break;
case S::AddCorrection:r[2].er=h.add32(r[2].er,r[0].er);break;
case S::AddSecondCorrection:r[2].er=h.add32(r[2].er,r[0].er);break;
case S::BindResolution:f.move(r[1].er,0x4000000);break;
case S::SetResolutionPasses:f.move(r[0].rl,0x5);break;
case S::TestResolution:f.move(r[2].e,r[2].e);break;
case S::DoubleRate:f.shift(r[2].er,1,true);break;
case S::DoubleResolution:f.shift(r[1].er,1,true);break;
case S::DecrementResolutionPasses:f.inc(r[0].rl,-1);break;
case S::DivideHigh:f.divideUnsigned(r[2].e,r[1].er);break;
case S::RetainHigh:f.move(r[0].e,r[1].r);break;
case S::ClearLow:r[1].r=h.sub16(r[1].r,r[1].r);break;
case S::DivideLow:f.divideUnsigned(r[2].e,r[1].er);break;
case S::CombineCoefficient:f.move(r[1].e,r[0].e);break;
case S::ReadActiveAgain:f.move(r[0].rl,task.active());break;
case S::ReadGainAgain:f.move(r[0].r,task.gain());break;
case S::CompareMinimumGain:h.sub16(r[0].r,0x80);break;
case S::CompareTransitionGain:h.sub16(r[0].r,0x800);break;
case S::DecrementTransitionGain:f.inc(r[0].r,-1);break;
case S::ReadPreviousCoefficient:f.move(r[2].er,task.previous());break;
case S::DifferenceCoefficient:r[2].er=h.sub32(r[2].er,r[1].er);break;
case S::NegateCoefficientDifference:r[2].er=h.sub32(0,r[2].er);break;
case S::CompareCoefficientThreshold:h.sub32(r[2].er,0x1000);break;
case S::SelectCoefficientWord:f.move(r[4].r,0x4);break;
case S::SendCoefficient:f.call(0x24c6);break;
case S::ReadPublishPeriod:f.move(r[1].er,task.period());break;
case S::QuantizePeriod:r[1].er&=0xfffff000;value(h,r[1].er);break;
case S::ReadPreviousPeriod:f.move(r[2].er,task.previous());break;
case S::ComparePeriod:h.sub32(r[2].er,r[1].er);break;
case S::ScalePublishPeriod:f.shift(r[1].er,2,true);break;
case S::SelectPeriodWord:f.move(r[4].r,0x3);break;
case S::RetainPeriodWord:f.push(r[4].r);break;
case S::RetainPeriodValue:f.push(r[1].er);break;
case S::SendPeriodAddress:f.call(0x24be);break;
case S::RestorePeriodValue:f.pop(r[1].er);break;
case S::RestorePeriodWord:f.pop(r[4].r);break;
case S::SendPeriodValue:f.call(0x24c6);break;
case S::ReturnUpdate:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native clock sync clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
