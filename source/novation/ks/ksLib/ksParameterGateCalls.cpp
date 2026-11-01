#include "ksParameterGateCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {using S=ParameterGateTask::Stage;ParameterGatePartialTest partialTest=nullptr;struct Point{unsigned location,length;};
constexpr std::array<Point,7> setup{{{0x85a8,6},{0x85ae,6},{0x85b4,6},{0x85ba,2},{0x85bc,2},{0x85be,2},{0x85c0,8}}};constexpr std::array<std::array<Point,5>,7> outputs{{{{{0x85c8,2},{0x85ca,2},{0x85cc,2},{0x85ce,2},{0x85d0,2}}},{{{0x85e8,2},{0x85ea,2},{0x85ec,2},{0x85ee,2},{0x85f0,2}}},{{{0x8608,2},{0x860a,2},{0x860c,2},{0x860e,2},{0x8610,2}}},{{{0x8628,2},{0x862a,2},{0x862c,2},{0x862e,2},{0x8630,2}}},{{{0x8648,2},{0x864a,2},{0x864c,2},{0x864e,2},{0x8650,2}}},{{{0x8668,2},{0x866a,2},{0x866c,2},{0x866e,2},{0x8670,2}}},{{{0x8688,2},{0x868a,2},{0x868c,2},{0x868e,2},{0x8690,2}}}}};
constexpr std::array<unsigned,7> readPoints{{0x85d2,0x85f2,0x8612,0x8632,0x8652,0x8672,0x8692}},dispatchPoints{{0x85e4,0x8604,0x8624,0x8644,0x8664,0x8684,0x86a4}},returnPoints{{0x85e8,0x8608,0x8628,0x8648,0x8668,0x8688,0x86a8}};
Point point(const ParameterGateTask& t){if(t.awaitingOutput())return {returnPoints[t.output()],0};const unsigned s=unsigned(t.stage());return s<7?setup[s]:outputs[t.output()][s-7];}}
void setParameterGatePartialForTests(ParameterGatePartialTest fn){partialTest=fn;}
bool ParameterGateCalls::candidate(unsigned p){if(p==0x86a8)return true;for(const auto& a:setup)if(p==a.location)return true;for(const auto& group:outputs)for(const auto& a:group)if(p==a.location)return true;return false;}
void ParameterGateCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingOutput()&&c->task.output()==6&&point(c->task).location==unsigned(h.getPC())){c->task.resumeOutput();c.reset();++completed;}}
bool ParameterGateCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0x85a8){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native parameter-gate capacity exceeded");selected->emplace(h.getSP(),0);++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& task=(**selected).task;auto& r=h.regs;if(task.awaitingOutput())task.resumeOutput();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](ParameterGateTask::Read what)->uint32_t{using R=ParameterGateTask::Read;return observed=uint8_t(h.read8(what==R::Gate?0x167a3:what==R::Part?0x17526:what==R::Active?0x16798:int32_t(uint32_t(r[6].er)+222)));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native parameter-gate admission mismatch");
 switch(stage){case S::ReadGate:{f.move(r[2].rl,int8_t(observed));break;}
case S::ReadPart:{f.move(r[0].rl,int8_t(observed));break;}
case S::ReadActive:{f.move(r[0].rh,int8_t(observed));break;}
case S::CompareParts:{h.sub8(r[0].rh,r[0].rl);break;}
case S::AdmitPart:{;break;}
case S::ClearGate:{f.move(r[2].rl,0x0);break;}
case S::ReadMode:{f.move(r[2].rh,int8_t(observed));break;}
case S::CaptureGate:{f.move(r[2].rl,r[2].rl);break;}
case S::AdmitGate:{;break;}
case S::ClearPacket:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::CompareMode:{constexpr int required[]{2,2,1,1,0,0,0};h.sub8(r[2].rh,required[task.output()]);break;}
case S::AdmitMode:{;break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native parameter-gate clocks differ");
 if(task.awaitingOutput()){const bool zero=task.child()==ParameterGateTask::Child::Zero;if(zero&&r[1].er)throw std::runtime_error("Native parameter-gate zero packet differs");h.pc=h.makepc(zero?dispatchPoints[task.output()]:readPoints[task.output()]);return true;}
 h.pc=h.makepc(point(task).location);return true;
}
}
