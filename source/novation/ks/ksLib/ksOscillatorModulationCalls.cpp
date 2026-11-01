#include "ksOscillatorModulationCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
using S=OscillatorModulationTask::Stage;
OscillatorModulationPartialTest partialTest=nullptr;
struct Point{unsigned location,length;};
constexpr std::array<Point,37> points{{{0x0,8},{0x8,2},{0xa,2},{0xc,8},{0x14,2},{0x16,6},{0x1c,4},{0x20,2},{0x22,8},{0x2a,2},{0x2c,6},{0x32,4},{0x36,2},{0x38,8},{0x40,2},{0x42,6},{0x48,4},{0x4c,2},{0x4e,4},{0x52,4},{0x56,2},{0x58,4},{0x5c,2},{0x5e,2},{0x60,2},{0x62,2},{0x64,2},{0x66,4},{0x6a,2},{0x6c,4},{0x70,2},{0x72,2},{0x74,8},{0x7c,4},{0x80,2},{0x82,4},{0x86,4}}};
Point point(const OscillatorModulationTask& task){if(task.stage()==S::Return)return {0x7a9a,2};
 if(task.awaitingWord())return {0x7872+0x8a*(task.output()+1),0};
 const auto p=points[unsigned(task.stage())];return {0x7872+0x8a*task.output()+p.location,p.length};}
}
void setOscillatorModulationPartialForTests(OscillatorModulationPartialTest fn){partialTest=fn;}
bool OscillatorModulationCalls::step(ControllerCpu& h){using namespace controllerDetail;
 const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(selected&&(*selected)->task.awaitingWord()&&point((*selected)->task).location==pc)(*selected)->task.resumeWord();
 if(pc==0x7872){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
  if(!selected)throw std::runtime_error("Native oscillator-modulation capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();
  if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& task=(**selected).task;const auto stage=task.stage();auto& r=h.regs;
 const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 uint32_t observed=0;auto read=[&](OscillatorModulationTask::Read what)->uint32_t{switch(what){
 case OscillatorModulationTask::Read::Bias:return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+task.biasOffset())));
 case OscillatorModulationTask::Read::Depth:return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+task.depthOffset())));
 case OscillatorModulationTask::Read::Modulation:return observed=uint8_t(h.read8(0x17518+task.modulationOffset()));
 case OscillatorModulationTask::Read::Curve:return observed=uint16_t(h.read16(0x15200+task.curveIndex()));}return 0;};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native oscillator-modulation admission mismatch");
 switch(stage){
 case S::ReadBias:{f.move(r[1].rl,int8_t(observed));break;}
 case S::Bias:{r[1].rl=h.add8(r[1].rl,0xffffffc0);break;}
 case S::SignBias:{r[1].r=int8_t(r[1].r);value(h,r[1].r);break;}
 case S::ReadDepth0:{f.move(r[0].rl,int8_t(observed));break;}
 case S::BiasDepth0:{r[0].rl=h.add8(r[0].rl,0xffffffc0);break;}
 case S::ReadGlobal0:{f.move(r[0].rh,int8_t(observed));break;}
 case S::Multiply0:{f.multiplySigned(r[0].rh,r[0].r);break;}
 case S::BeginSum:{f.move(r[2].r,r[0].r);break;}
 case S::ReadDepth1:{f.move(r[0].rl,int8_t(observed));break;}
 case S::BiasDepth1:{r[0].rl=h.add8(r[0].rl,0xffffffc0);break;}
 case S::ReadGlobal1:{f.move(r[0].rh,int8_t(observed));break;}
 case S::Multiply1:{f.multiplySigned(r[0].rh,r[0].r);break;}
 case S::AddSum:{r[2].r=h.add16(r[2].r,r[0].r);break;}
 case S::ReadDepth2:{f.move(r[0].rl,int8_t(observed));break;}
 case S::BiasDepth2:{r[0].rl=h.add8(r[0].rl,0xffffffc0);break;}
 case S::ReadGlobal2:{f.move(r[0].rh,int8_t(observed));break;}
 case S::Multiply2:{f.multiplySigned(r[0].rh,r[0].r);break;}
 case S::FinishSum:{r[0].r=h.add16(r[0].r,r[2].r);break;}
 case S::SetScale:{f.move(r[0].e,0x200);break;}
 case S::Scale:{f.multiplySigned(r[0].e,r[0].er);break;}
 case S::CaptureScaled:{f.move(r[0].r,r[0].e);break;}
 case S::SetFactor:{f.move(r[2].r,0x5555);break;}
 case S::AddBias:{r[1].r=h.add16(r[1].r,r[0].r);break;}
 case S::CaptureCombined:{f.move(r[0].r,r[1].r);break;}
 case S::AdmitSign:{;break;}
 case S::NegateMagnitude:{r[1].r=h.sub16(0,r[1].r);break;}
 case S::NegateFactor:{r[2].r=h.sub16(0,r[2].r);break;}
 case S::CompareMagnitude:{h.sub16(r[1].r,0x40);break;}
 case S::AdmitClamp:{;break;}
 case S::ClampMagnitude:{f.move(r[1].r,0x40);break;}
 case S::DoubleIndex:{f.shift(r[1].r,1,true);break;}
 case S::ClearIndex:{r[1].er&=0xffff;value(h,r[1].er);break;}
 case S::ReadTable:{f.move(r[1].r,int16_t(observed));break;}
 case S::MultiplyTable:{f.multiplySigned(r[2].r,r[1].er);break;}
 case S::DoubleOutput:{f.shift(r[1].er,1,true);break;}
 case S::SetOffset:{f.move(r[4].r,int16_t(task.offset()));break;}
 case S::DispatchWord:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native oscillator-modulation payload projection differs");f.call(0x785a);break;}
 case S::Return:f.ret();break;
 default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native oscillator-modulation clocks differ");
 if(stage==S::DispatchWord){h.pc=h.makepc(f.next);return true;}
 if(stage==S::Return){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}
 h.pc=h.makepc(point(task).location);return true;
}
}
