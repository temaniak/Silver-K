#include "ksClockPulseCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
ClockPulsePartialTest partialTest=nullptr;using T=ClockPulseTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,90> points{{{0x2848,2},{0x284a,6},{0x2850,2},{0x2852,2},{0x2854,6},{0x285a,2},{0x285c,6},{0x2862,4},{0x2866,6},{0x286c,2},{0x286e,2},{0x2870,2},{0x2872,4},{0x2876,6},{0x287c,2},{0x287e,6},{0x2884,8},{0x288c,6},{0x2892,6},{0x2898,4},{0x289c,6},{0x28a2,2},{0x28a4,6},{0x28aa,2},{0x28ac,8},{0x28b4,2},{0x28b6,2},{0x28b8,2},{0x28ba,2},{0x28bc,8},{0x28c4,2},{0x28c6,8},{0x28ce,6},{0x28d4,4},{0x28d8,6},{0x28de,4},{0x28e2,2},{0x28e4,6},{0x28ea,2},{0x28ec,4},{0x28f0,4},{0x28f4,6},{0x28fa,2},{0x28fc,6},{0x2902,6},{0x2908,6},{0x290e,4},{0x2912,2},{0x2914,4},{0x2918,2},{0x291a,2},{0x291c,8},{0x2924,2},{0x2926,2},{0x2928,6},{0x292e,4},{0x2932,2},{0x2934,2},{0x2936,2},{0x2938,8},{0x2940,2},{0x2942,2},{0x2944,2},{0x2946,2},{0x2948,2},{0x294a,8},{0x2952,6},{0x2958,2},{0x295a,6},{0x2960,4},{0x2964,2},{0x2966,2},{0x2968,4},{0x296c,4},{0x2970,2},{0x2972,6},{0x2978,2},{0x297a,2},{0x297c,4},{0x2980,2},{0x2982,4},{0x2986,2},{0x2988,6},{0x298e,4},{0x2992,2},{0x2994,4},{0x2998,2},{0x299a,4},{0x299e,6},{0x29a4,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
}
void setClockPulsePartialForTests(ClockPulsePartialTest value){partialTest=value;}
bool ClockPulseCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 if(pc==0x2848||pc==0x285c){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native clock pulse capacity exceeded");slot->emplace(pc==0x2848?T::Kind::Latched:T::Kind::Edge,h.getSP(),uint8_t(h.regs[1].rl),uint8_t(h.regs[2].rl));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint16_t(r[0].r));const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field)->uint32_t{switch(field){case T::Read::Latched:return uint8_t(h.read8(0xffee9a));case T::Read::Flags:return uint8_t(h.read8(0xffee97));case T::Read::Active:return uint8_t(h.read8(0xffee96));case T::Read::Pulses:return uint16_t(h.read16(0xffee78));case T::Read::PulseCount:return uint16_t(h.read16(0xffeea8));case T::Read::Timer:return uint32_t(h.read32(0xffee78));case T::Read::Sum:return uint32_t(h.read32(0xffeeaa));case T::Read::Samples:return uint16_t(h.read16(0xffee9c));case T::Read::PreviousTick:return uint16_t(h.read16(0xffee9e));case T::Read::Estimate:return uint32_t(h.read32(0xffeea0));case T::Read::Gain:return uint16_t(h.read16(0xffeea4));case T::Read::Integral:return uint16_t(h.read16(0xffeea6));}return 0;};
 auto write=[&](T::Write field,uint32_t value){switch(field){case T::Write::Latched:f.store8(0xffee9a,int8_t(value));break;case T::Write::Flags:h.write8(int8_t(value),0xffee97);break;case T::Write::PreviousTick:f.store16(0xffee9e,int16_t(value));break;case T::Write::Samples:f.store16(0xffee9c,int16_t(value));break;case T::Write::Sum:f.store32(0xffeeaa,int32_t(value));break;case T::Write::PulseCount:f.store16(0xffeea8,int16_t(value));break;case T::Write::Estimate:f.store32(0xffeea0,int32_t(value));break;case T::Write::Gain:f.store16(0xffeea4,int16_t(value));break;case T::Write::Integral:f.store16(0xffeea6,int16_t(value));break;case T::Write::SumHigh:f.store16(0xffeeaa,int16_t(value));break;case T::Write::SumLow:f.store16(0xffeeac,int16_t(value));break;case T::Write::Period:f.store32(0xffee7e,int32_t(value));break;}};
 if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native clock pulse admission differs");
 switch(stage){case S::BindLatchedIndex:f.move(r[1].rl,0x0);break;
case S::ReadLatchedSource:f.move(r[2].rl,task.source());break;
case S::ConsumeLatched:f.call(0x285c);break;
case S::ClearLatched:f.move(r[0].rl,0x0);break;
case S::ReturnLatched:f.ret();break;
case S::SelectFlags:f.move(r[0].er,0xffee97);break;
case S::ReadActive:f.move(r[0].rl,task.active());break;
case S::CompareResetSource:h.sub8(r[2].rl,0xffffffff);break;
case S::ReadResetTick:f.call(0x25fe);break;
case S::ClearResetCount:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::ReadResetPulses:f.move(r[0].r,task.pulses());break;
case S::FinishReset:h.clockI(2);break;
case S::ReadPulseCount:f.move(r[2].e,task.pulses());break;
case S::IncrementPulseCount:f.inc(r[2].e,1);break;
case S::ClearPulseFraction:r[2].r=h.sub16(r[2].r,r[2].r);break;
case S::ReadTimer:f.move(r[0].er,task.timer());break;
case S::DifferenceTimer:r[0].er=h.sub32(r[0].er,r[2].er);break;
case S::CaptureTimerSign:f.move(r[2].r,r[0].e);break;
case S::ExtendTimerSign:r[2].r=int8_t(r[2].r);value(h,r[2].r);break;
case S::BindTimerSign:f.move(r[0].e,r[2].r);break;
case S::ReadSum:f.move(r[2].er,task.sum());break;
case S::AccumulateTimer:r[2].er=h.add32(r[2].er,r[0].er);break;
case S::ReadSamples:f.move(r[1].e,task.samples());break;
case S::IncrementSamples:r[1].e=h.add16(r[1].e,0x200);break;
case S::ReadTick:f.call(0x25fe);break;
case S::RetainTick:f.move(r[0].e,r[0].r);break;
case S::ReadPreviousTick:f.move(r[1].r,task.baseline());break;
case S::ElapsedTick:r[0].r=h.sub16(r[0].r,r[1].r);break;
case S::CompareElapsed:h.sub16(r[0].r,0x3e80);break;
case S::SelectElapsed:h.clockI(2);break;
case S::ClearSampleCount:r[0].e=h.sub16(r[0].e,r[0].e);break;
case S::DivideAverage:f.divideSigned(r[1].e,r[2].er);break;
case S::RetainAverage:f.push(r[2].r);break;
case S::BindRateFactor:f.move(r[1].r,0xc350);break;
case S::MultiplyRate:r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[1].e));h.clockI(20);break;
case S::DivideRate:f.divideUnsigned(r[0].r,r[1].er);break;
case S::ReadEstimate:f.move(r[0].er,task.estimate());break;
case S::DifferenceRate:r[1].r=h.sub16(r[1].r,r[0].e);break;
case S::RetainError:f.move(r[2].r,r[1].r);break;
case S::ReadGain:f.move(r[1].e,task.gain());break;
case S::MultiplyCorrection:f.multiplySigned(r[1].e,r[1].er);break;
case S::DoubleCorrection:f.shift(r[1].er,1,true);break;
case S::AddEstimate:r[0].er=h.add32(r[0].er,r[1].er);break;
case S::AddSecondCorrection:r[1].er=h.add32(r[1].er,r[0].er);break;
case S::QuarterEstimate:f.shift(r[1].er,2,false);break;
case S::RestoreAverage:f.pop(r[0].e);break;
case S::ClearAverageLow:r[0].r=h.sub16(r[0].r,r[0].r);break;
case S::QuarterAverage:f.arithmeticRight(r[0].er,2);break;
case S::DifferencePeriod:r[1].er=h.sub32(r[1].er,r[0].er);break;
case S::ReadIntegral:f.move(r[0].e,task.integral());break;
case S::AddIntegralError:r[0].e=h.add16(r[0].e,r[2].r);break;
case S::ReadSaturationGain:f.move(r[0].r,task.gain());break;
case S::CompareSaturationGain:h.sub16(r[0].r,0x4000);break;
case S::ClearIntegral:r[0].e=h.sub16(r[0].e,r[0].e);break;
case S::BindIntegralFactor:f.move(r[0].r,0x7e00);break;
case S::MultiplyIntegral:f.multiplySigned(r[0].e,r[0].er);break;
case S::DoubleIntegral:f.shift(r[0].er,1,true);break;
case S::NegateIntegralMagnitude:r[0].e=h.sub16(0,r[0].e);break;
case S::CompareIntegralThreshold:h.sub16(r[0].e,0x1800);break;
case S::SetMaximumGain:f.move(r[0].r,0x4000);break;
case S::ReadDecayGain:f.move(r[0].r,task.gain());break;
case S::DecayGain:r[0].r=h.sub16(r[0].r,0x800);break;
case S::CompareGainFloor:h.sub16(r[0].r,0x80);break;
case S::SetMinimumGain:f.move(r[0].r,0x80);break;
case S::ReturnPulse:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native clock pulse clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
