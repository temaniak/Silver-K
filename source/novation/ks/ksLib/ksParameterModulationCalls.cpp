#include "ksParameterModulationCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
using S=ParameterModulationTask::Stage;
ParameterModulationPartialTest partialTest=nullptr;
struct Point{unsigned location,length;};struct Binding{unsigned entry,end;std::array<Point,37> points;};
constexpr std::array<Binding,4> bindings{{{0x7f92,0x8018,{{{0x7f92,8},{0x7f9a,2},{0x7f9c,2},{0x7f9e,8},{0x7fa6,2},{0x7fa8,6},{0x7fae,4},{0x7fb2,2},{0x7fb4,8},{0x7fbc,2},{0x7fbe,6},{0x7fc4,4},{0x7fc8,2},{0x7fca,8},{0x7fd2,2},{0x7fd4,6},{0x7fda,4},{0x7fde,2},{0x7fe0,4},{0x7fe4,4},{0x7fe8,2},{0x7fea,4},{0x7fee,2},{0x7ff0,2},{0x7ff2,2},{0x7ff4,2},{0x7ff6,2},{0x7ff8,4},{0x7ffc,2},{0x7ffe,4},{0x8002,2},{0x8004,2},{0x8006,8},{0x800e,4},{0x8012,2},{0x0,0},{0x8014,4}}}},{0x8206,0x828c,{{{0x8206,8},{0x820e,2},{0x8210,2},{0x8212,8},{0x821a,2},{0x821c,6},{0x8222,4},{0x8226,2},{0x8228,8},{0x8230,2},{0x8232,6},{0x8238,4},{0x823c,2},{0x823e,8},{0x8246,2},{0x8248,6},{0x824e,4},{0x8252,2},{0x8254,4},{0x8258,4},{0x825c,2},{0x825e,4},{0x8262,2},{0x8264,2},{0x8266,2},{0x8268,2},{0x826a,2},{0x826c,4},{0x8270,2},{0x8272,4},{0x8276,2},{0x8278,2},{0x827a,8},{0x8282,4},{0x8286,2},{0x0,0},{0x8288,4}}}},{0x847a,0x8500,{{{0x847a,8},{0x8482,2},{0x8484,2},{0x8486,8},{0x848e,2},{0x8490,6},{0x8496,4},{0x849a,2},{0x849c,8},{0x84a4,2},{0x84a6,6},{0x84ac,4},{0x84b0,2},{0x84b2,8},{0x84ba,2},{0x84bc,6},{0x84c2,4},{0x84c6,2},{0x84c8,4},{0x84cc,4},{0x84d0,2},{0x84d2,4},{0x84d6,2},{0x84d8,2},{0x84da,2},{0x84dc,2},{0x84de,2},{0x84e0,4},{0x84e4,2},{0x84e6,4},{0x84ea,2},{0x84ec,2},{0x84ee,8},{0x84f6,4},{0x84fa,2},{0x0,0},{0x84fc,4}}}},{0x87c4,0x884a,{{{0x87c4,8},{0x87cc,2},{0x87ce,2},{0x87d0,8},{0x87d8,2},{0x87da,6},{0x87e0,4},{0x87e4,2},{0x87e6,8},{0x87ee,2},{0x87f0,6},{0x87f6,4},{0x87fa,2},{0x87fc,8},{0x8804,2},{0x8806,6},{0x880c,4},{0x8810,2},{0x8812,4},{0x8816,4},{0x881a,2},{0x881c,4},{0x8820,2},{0x8822,2},{0x8824,2},{0x8826,2},{0x8828,2},{0x882a,4},{0x882e,2},{0x8830,4},{0x8834,2},{0x8836,2},{0x8838,8},{0x8840,4},{0x8844,2},{0x0,0},{0x8846,4}}}}}};
Point point(const ParameterModulationTask& task,unsigned b){if(task.awaitingWord())return {bindings[b].end,0};return bindings[b].points[unsigned(task.stage())];}
}
void setParameterModulationPartialForTests(ParameterModulationPartialTest fn){partialTest=fn;}
bool ParameterModulationCalls::step(ControllerCpu& h){using namespace controllerDetail;
 const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(selected&&(*selected)->task.awaitingWord()&&point((*selected)->task,(*selected)->binding).location==pc){(*selected)->task.resumeWord();selected->reset();selected=nullptr;++completed;}
 for(unsigned b=0;b<bindings.size();++b)if(pc==bindings[b].entry){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
  if(!selected)throw std::runtime_error("Native oscillator-modulation capacity exceeded");selected->emplace(h.getSP(),b);++started;break;}
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();
  if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;const auto stage=task.stage();auto& r=h.regs;
 const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task,call.binding).length);
 uint32_t observed=0;auto read=[&](ParameterModulationTask::Read what)->uint32_t{switch(what){
 case ParameterModulationTask::Read::Bias:return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+task.biasOffset())));
 case ParameterModulationTask::Read::Depth:return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+task.depthOffset())));
 case ParameterModulationTask::Read::Modulation:return observed=uint8_t(h.read8(0x17518+task.modulationOffset()));
 case ParameterModulationTask::Read::Curve:return observed=uint16_t(h.read16(0x15200+task.curveIndex()));}return 0;};
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

 case S::DispatchWord:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native oscillator-modulation payload projection differs");f.call(0x2520);break;}

 default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native oscillator-modulation clocks differ");
 if(stage==S::DispatchWord){h.pc=h.makepc(f.next);return true;}

 h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
