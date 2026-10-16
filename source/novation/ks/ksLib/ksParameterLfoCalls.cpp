#include "ksParameterLfoCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {using S=ParameterLfoTask::Stage;ParameterLfoPartialTest partialTest=nullptr;struct Point{unsigned location,length;};struct Binding{unsigned entry,end;std::array<Point,95> points;};
constexpr std::array<Binding,2> bindings{{{0x8be0,0x8d06,{{{0x8be0,8},{0x8be8,2},{0x8bea,2},{0x8bec,6},{0x8bf2,4},{0x8bf6,8},{0x8bfe,2},{0x8c00,2},{0x8c02,2},{0x8c04,4},{0x8c08,8},{0x8c10,2},{0x8c12,2},{0x8c14,4},{0x8c18,2},{0x8c1a,2},{0x8c1c,2},{0x8c1e,2},{0x8c20,2},{0x8c22,6},{0x8c28,4},{0x8c2c,2},{0x8c2e,8},{0x8c36,2},{0x8c38,2},{0x8c3a,2},{0x8c3c,4},{0x8c40,2},{0x8c42,2},{0x8c44,4},{0x8c48,6},{0x8c4e,4},{0x8c52,2},{0x8c54,4},{0x8c58,2},{0x8c5a,2},{0x8c5c,2},{0x8c5e,2},{0x8c60,2},{0x8c62,2},{0x8c64,2},{0x8c66,2},{0x8c68,2},{0x8c6a,2},{0x8c6c,2},{0x8c6e,2},{0x8c70,2},{0x8c72,4},{0x8c76,2},{0x8c78,4},{0x8c7c,2},{0x8c7e,8},{0x8c86,2},{0x8c88,6},{0x8c8e,8},{0x8c96,2},{0x8c98,2},{0x8c9a,2},{0x8c9c,2},{0x8c9e,2},{0x8ca0,2},{0x8ca2,2},{0x8ca4,2},{0x8ca6,4},{0x8caa,8},{0x8cb2,2},{0x8cb4,2},{0x8cb6,4},{0x8cba,2},{0x8cbc,2},{0x8cbe,2},{0x8cc0,2},{0x8cc2,2},{0x8cc4,6},{0x8cca,4},{0x8cce,2},{0x8cd0,2},{0x8cd2,8},{0x8cda,2},{0x8cdc,2},{0x8cde,2},{0x8ce0,2},{0x8ce2,2},{0x8ce4,2},{0x8ce6,2},{0x8ce8,2},{0x8cea,2},{0x8cec,6},{0x8cf2,2},{0x8cf4,4},{0x8cf8,2},{0x8cfa,2},{0x8cfc,4},{0x8d00,2},{0x8d02,4}}}},{0x8d3e,0x8e64,{{{0x8d3e,8},{0x8d46,2},{0x8d48,2},{0x8d4a,6},{0x8d50,4},{0x8d54,8},{0x8d5c,2},{0x8d5e,2},{0x8d60,2},{0x8d62,4},{0x8d66,8},{0x8d6e,2},{0x8d70,2},{0x8d72,4},{0x8d76,2},{0x8d78,2},{0x8d7a,2},{0x8d7c,2},{0x8d7e,2},{0x8d80,6},{0x8d86,4},{0x8d8a,2},{0x8d8c,8},{0x8d94,2},{0x8d96,2},{0x8d98,2},{0x8d9a,4},{0x8d9e,2},{0x8da0,2},{0x8da2,4},{0x8da6,6},{0x8dac,4},{0x8db0,2},{0x8db2,4},{0x8db6,2},{0x8db8,2},{0x8dba,2},{0x8dbc,2},{0x8dbe,2},{0x8dc0,2},{0x8dc2,2},{0x8dc4,2},{0x8dc6,2},{0x8dc8,2},{0x8dca,2},{0x8dcc,2},{0x8dce,2},{0x8dd0,4},{0x8dd4,2},{0x8dd6,4},{0x8dda,2},{0x8ddc,8},{0x8de4,2},{0x8de6,6},{0x8dec,8},{0x8df4,2},{0x8df6,2},{0x8df8,2},{0x8dfa,2},{0x8dfc,2},{0x8dfe,2},{0x8e00,2},{0x8e02,2},{0x8e04,4},{0x8e08,8},{0x8e10,2},{0x8e12,2},{0x8e14,4},{0x8e18,2},{0x8e1a,2},{0x8e1c,2},{0x8e1e,2},{0x8e20,2},{0x8e22,6},{0x8e28,4},{0x8e2c,2},{0x8e2e,2},{0x8e30,8},{0x8e38,2},{0x8e3a,2},{0x8e3c,2},{0x8e3e,2},{0x8e40,2},{0x8e42,2},{0x8e44,2},{0x8e46,2},{0x8e48,2},{0x8e4a,6},{0x8e50,2},{0x8e52,4},{0x8e56,2},{0x8e58,2},{0x8e5a,4},{0x8e5e,2},{0x8e60,4}}}}}};
Point point(const ParameterLfoTask& t,unsigned b){if(t.terminalAwait())return {bindings[b].end,0};return bindings[b].points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}
unsigned depth(const ParameterLfoTask& t){const auto s=t.stage();return (s>=S::HalfRate&&s<=S::RestoreRate)||(s==S::AwaitCoefficient&&t.resumeStage()==S::RestoreRate)?2:0;}}
void setParameterLfoPartialForTests(ParameterLfoPartialTest fn){partialTest=fn;}
bool ParameterLfoCalls::candidate(unsigned p){for(const auto& b:bindings)if(p>=b.entry&&p<=b.end)return true;return false;}
void ParameterLfoCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token-depth(c->task)==h.getSP()&&c->task.terminalAwait()&&point(c->task,c->binding).location==unsigned(h.getPC())){c->task.resumeChild();c.reset();++completed;}}
bool ParameterLfoCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token-depth(c->task)==h.getSP()){selected=&c;break;}
 for(unsigned b=0;b<bindings.size();++b)if(pc==bindings[b].entry){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native LFO capacity exceeded");selected->emplace(h.getSP(),b);++started;break;}
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint32_t(r[0].er));const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task,call.binding).length);uint32_t observed=0;
 auto read=[&](ParameterLfoTask::Read what)->uint32_t{using R=ParameterLfoTask::Read;const unsigned channel=task.channel();switch(what){
 case R::RateBase:return observed=uint16_t(h.read16(0x15138));case R::DelayBase:return observed=uint16_t(h.read16(0x1513a));case R::RatePreset:case R::DelayPreset:return observed=uint32_t(h.read32(0x159f4+task.presetOffset()));
 default:{const unsigned field=what==R::Mode||what==R::LateMode?131+channel:what==R::Shape?135+channel:what==R::RateSelector?73+3*channel:what==R::Rate?72+3*channel:what==R::Polarity?133+channel:what==R::DelaySelector?137+channel:74+3*channel;return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+field)));}}};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native LFO admission mismatch");
 switch(stage){case S::ReadMode:{f.move(r[1].rh,observed);break;}
case S::DoubleMode:{f.shift(r[1].rh,2,true);break;}
case S::ClearModeHigh:{r[1].e=h.sub16(r[1].e,r[1].e);break;}
case S::OffsetMode:{r[1].er=h.add32(r[1].er,0x52000);break;}
case S::DispatchMode:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native LFO packet differs");f.call(0x2520);break;}
case S::ReadShape:{f.move(r[1].rh,observed);break;}
case S::DoubleShape:{f.shift(r[1].rh,1,true);break;}
case S::ClearShapeLow:{r[1].rl=h.sub8(r[1].rl,r[1].rl);break;}
case S::CaptureShape:{f.move(r[1].e,r[1].r);break;}
case S::DispatchShape:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native LFO packet differs");f.call(0x2520);break;}
case S::ReadRateSelector:{f.move(r[2].rl,observed);break;}
case S::AdmitRateSelector:{;break;}
case S::ClearRatePresetFirst:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::DispatchRatePresetFirst:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native LFO packet differs");f.call(0x2520);break;}
case S::DecrementRatePreset:{f.inc(r[2].rl,-1);break;}
case S::DoubleRateIndex0:{f.shift(r[2].rl,1,true);break;}
case S::DoubleRateIndex1:{f.shift(r[2].rl,1,true);break;}
case S::ClearRateIndexWord:{r[2].r&=0xff;value(h,r[2].r);break;}
case S::ClearRateIndex:{r[2].er&=0xffff;value(h,r[2].er);break;}
case S::AddRateTable:{r[2].er=h.add32(r[2].er,0x159f4);break;}
case S::ReadRatePreset:{f.move(r[1].er,observed);break;}
case S::JumpRateSecond:{;break;}
case S::ReadRate:{f.move(r[0].rh,observed);break;}
case S::ClearRateLow:{f.move(r[0].rl,0x0);break;}
case S::RetainRate:{f.push(r[0].r);break;}
case S::HalfRate:{f.shift(r[0].r,1,false);break;}
case S::SetRateScale:{f.move(r[0].e,0xaaab);break;}
case S::MultiplyRateScale:{r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20);break;}
case S::CaptureRateScale:{f.move(r[0].r,r[0].e);break;}
case S::OffsetRate:{r[0].r=h.add16(r[0].r,0x4700);break;}
case S::ReadRateBase:{f.move(r[0].e,observed);break;}
case S::SubtractRateBase:{r[0].e=h.sub16(r[0].e,0x4470);break;}
case S::AddRateBase:{r[0].r=h.add16(r[0].r,r[0].e);break;}
case S::DispatchRateCoefficient:{if(uint16_t(r[0].r)!=task.scaleInput())throw std::runtime_error("Native LFO coefficient input differs");f.call(0xa69e);break;}
case S::RestoreRate:{f.pop(r[2].r);break;}
case S::CompareRate:{h.sub8(r[2].rh,0x20);break;}
case S::AdmitRateClamp:{;break;}
case S::ClampRate:{f.move(r[2].rh,0x20);break;}
case S::DoubleWeight0:{f.shift(r[2].rh,1,true);break;}
case S::DoubleWeight1:{f.shift(r[2].rh,1,true);break;}
case S::CaptureRateHigh:{f.move(r[1].r,r[0].e);break;}
case S::MultiplyRateHigh:{r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[2].r));h.clockI(20);break;}
case S::MultiplyRateLow:{r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[2].r));h.clockI(20);break;}
case S::CaptureRateLow:{f.move(r[0].r,r[0].e);break;}
case S::ClearRateLowProduct:{r[0].e=h.sub16(r[0].e,r[0].e);break;}
case S::AddRateProducts:{r[1].er=h.add32(r[1].er,r[0].er);break;}
case S::DoubleRateOutput:{f.shift(r[1].er,1,true);break;}
case S::DispatchRateFirst:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native LFO packet differs");f.call(0x2520);break;}
case S::ClearRateSecond:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::DispatchRateSecond:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native LFO packet differs");f.call(0x2520);break;}
case S::ClearFlags:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::ReadPolarity:{f.move(r[0].rl,observed);break;}
case S::AdmitPolarity:{;break;}
case S::SetPolarity:{f.move(r[1].er,0x40000000);break;}
case S::ReadLateMode:{f.move(r[0].rl,observed);break;}
case S::CompareMode4:{h.sub8(r[0].rl,0x4);break;}
case S::AdmitMode4:{;break;}
case S::CompareMode5:{h.sub8(r[0].rl,0x5);break;}
case S::AdmitMode5:{;break;}
case S::IncrementMode:{f.inc(r[1].rh,1);break;}
case S::CompareMode2:{h.sub8(r[0].rl,0x2);break;}
case S::AdmitMode2:{;break;}
case S::AddModeTwo:{r[1].rh=h.add8(r[1].rh,0x2);break;}
case S::DispatchFlags:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native LFO packet differs");f.call(0x2520);break;}
case S::ReadDelaySelector:{f.move(r[2].rl,observed);break;}
case S::AdmitDelaySelector:{;break;}
case S::ClearDelayPresetFirst:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::DispatchDelayPresetFirst:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native LFO packet differs");f.call(0x2520);break;}
case S::DecrementDelayPreset:{f.inc(r[2].rl,-1);break;}
case S::DoubleDelayIndex0:{f.shift(r[2].rl,1,true);break;}
case S::DoubleDelayIndex1:{f.shift(r[2].rl,1,true);break;}
case S::ClearDelayIndexWord:{r[2].r&=0xff;value(h,r[2].r);break;}
case S::ClearDelayIndex:{r[2].er&=0xffff;value(h,r[2].er);break;}
case S::AddDelayTable:{r[2].er=h.add32(r[2].er,0x159f4);break;}
case S::ReadDelayPreset:{f.move(r[1].er,observed);break;}
case S::HalfDelayPreset:{f.shift(r[1].er,1,false);break;}
case S::JumpDelaySecond:{;break;}
case S::ReadDelay:{f.move(r[0].rl,observed);break;}
case S::InvertDelay:{r[0].rl^=0x7f;value(h,r[0].rl);break;}
case S::CaptureDelay:{f.move(r[2].rh,r[0].rl);break;}
case S::SetDelayMultiplier:{f.move(r[0].rh,0x50);break;}
case S::MultiplyDelay:{r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;}
case S::SetDelayThreshold:{f.move(r[2].rl,0x73);break;}
case S::SubtractDelayThreshold:{r[2].rh=h.sub8(r[2].rh,r[2].rl);break;}
case S::AdmitDelayCorrection:{;break;}
case S::ClearDelayCorrection:{r[2].rl=h.sub8(r[2].rl,r[2].rl);break;}
case S::AddDelayCorrection:{r[0].r=h.add16(r[0].r,r[2].r);break;}
case S::ReadDelayBase:{f.move(r[0].e,observed);break;}
case S::AddDelayBase:{r[0].r=h.add16(r[0].r,r[0].e);break;}
case S::DispatchDelayCoefficient:{if(uint16_t(r[0].r)!=task.scaleInput())throw std::runtime_error("Native LFO coefficient input differs");f.call(0xa69e);break;}
case S::HalfDelayCoefficient:{f.shift(r[0].er,1,false);break;}
case S::CaptureDelayCoefficient:{f.move(r[1].er,r[0].er);break;}
case S::DispatchDelayFirst:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native LFO packet differs");f.call(0x2520);break;}
case S::ClearDelaySecond:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::DispatchDelaySecond:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native LFO packet differs");f.call(0x2520);break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native LFO clocks differ");if(task.awaitingChild()){h.pc=h.makepc(f.next);return true;}h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
