#include "ksParameterCurveCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {using S=ParameterCurveTask::Stage;ParameterCurvePartialTest partialTest=nullptr;struct Point{unsigned location,length;};struct Binding{unsigned entry,end,offset;std::array<Point,13> points;};
constexpr std::array<Binding,4> bindings{{{0x7f66,0x7f92,11,{{{0x7f66,4},{0x7f6a,8},{0x7f72,2},{0x7f74,2},{0x7f76,2},{0x7f78,2},{0x7f7a,2},{0x7f7c,2},{0x7f7e,2},{0x7f80,8},{0x7f88,4},{0x7f8c,2},{0x7f8e,4}}}},{0x81da,0x8206,19,{{{0x81da,4},{0x81de,8},{0x81e6,2},{0x81e8,2},{0x81ea,2},{0x81ec,2},{0x81ee,2},{0x81f0,2},{0x81f2,2},{0x81f4,8},{0x81fc,4},{0x8200,2},{0x8202,4}}}},{0x844e,0x847a,27,{{{0x844e,4},{0x8452,8},{0x845a,2},{0x845c,2},{0x845e,2},{0x8460,2},{0x8462,2},{0x8464,2},{0x8466,2},{0x8468,8},{0x8470,4},{0x8474,2},{0x8476,4}}}},{0x8798,0x87c4,52,{{{0x8798,4},{0x879c,8},{0x87a4,2},{0x87a6,2},{0x87a8,2},{0x87aa,2},{0x87ac,2},{0x87ae,2},{0x87b0,2},{0x87b2,8},{0x87ba,4},{0x87be,2},{0x87c0,4}}}}}};
Point point(const ParameterCurveTask& t,unsigned b){return t.awaitingWord()?Point{bindings[b].end,0}:bindings[b].points[unsigned(t.stage())];}}
void setParameterCurvePartialForTests(ParameterCurvePartialTest fn){partialTest=fn;}
bool ParameterCurveCalls::candidate(unsigned p){for(const auto& b:bindings)if(p>=b.entry&&p<=b.end)return true;return false;}
void ParameterCurveCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingWord()&&point(c->task,c->binding).location==unsigned(h.getPC())){c->task.resumeWord();c.reset();++completed;}}
bool ParameterCurveCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 for(unsigned b=0;b<bindings.size();++b)if(pc==bindings[b].entry){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native curve capacity exceeded");selected->emplace(h.getSP(),b);++started;break;}
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;const auto& binding=bindings[call.binding];const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task,call.binding).length);uint32_t observed=0;
 auto read=[&](bool curve)->uint32_t{return observed=curve?uint16_t(h.read16(0x15200+task.index())):uint8_t(h.read8(int32_t(uint32_t(r[6].er)+binding.offset)));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native curve admission mismatch");
 switch(stage){case S::SetFactor:{f.move(r[2].r,0x5555);break;}
case S::ReadValue:{f.move(r[1].rl,int8_t(observed));break;}
case S::Bias:{r[1].rl=h.add8(r[1].rl,0xffffffc0);break;}
case S::AdmitSign:{;break;}
case S::NegateValue:{r[1].rl=h.sub8(0,r[1].rl);break;}
case S::NegateFactor:{r[2].r=h.sub16(0,r[2].r);break;}
case S::DoubleValue:{f.shift(r[1].rl,1,true);break;}
case S::ClearWord:{r[1].r&=0xff;value(h,r[1].r);break;}
case S::ClearIndex:{r[1].er&=0xffff;value(h,r[1].er);break;}
case S::ReadCurve:{f.move(r[1].r,int16_t(observed));break;}
case S::Multiply:{f.multiplySigned(r[2].r,r[1].er);break;}
case S::DoubleOutput:{f.shift(r[1].er,1,true);break;}
case S::Dispatch:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native curve packet differs");f.call(0x2520);break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native curve clocks differ");if(stage==S::Dispatch){h.pc=h.makepc(f.next);return true;}h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
