#include "ksStateStoreServicesCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateStoreServicesTask;using S=T::Stage;StateStoreServicesPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,19> points{{{0x668c,6},{0x6692,8},{0x669a,2},{0x669c,6},{0x66a2,6},{0x66a8,4},{0x66ac,4},{0x66b0,2},{0x66b2,4},{0x66b6,4},{0x66ba,2},{0x66bc,6},{0x66c2,8},{0x66ca,2},{0x66cc,2},{0x66ce,2},{0x66d0,2},{0x66d2,6},{0x66d8,4}}};Point point(const T& t){return t.awaitingTail()?Point{t.service()==T::Service::Program?0x6662u:0x55b8u,0}:points[unsigned(t.awaitingMessage()?t.resumeStage():t.stage())];}}
void setStateStoreServicesPartialForTests(StateStoreServicesPartialTest f){partialTest=f;}
void StateStoreServicesCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingTail()&&point(c->task).location==unsigned(h.getPC())){c->task.retireTail();c.reset();++completed;}}
bool StateStoreServicesCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x668c||pc==0x66bc||pc==0x66d2){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native store service capacity exceeded");selected->emplace(pc==0x668c?T::Kind::Offset:pc==0x66bc?T::Kind::Check:T::Kind::Message,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingMessage())task.resumeMessage();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint8_t observed=0;auto read=[&]()->unsigned{return observed=uint8_t(h.read8(0x168fc+239));};if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native store service admission differs");switch(stage){case S::SetFlagBase:{f.move(r[0].er,0x168fc);break;}
case S::ReadFlag:{f.move(r[0].rl,observed);break;}
case S::AdmitProgram:{;break;}
case S::AdvanceSource:{r[5].er=h.add32(r[5].er,0x100);break;}
case S::AdvanceTarget:{r[6].er=h.add32(r[6].er,0x100);break;}
case S::RetainSource:{f.push(r[5].er);break;}
case S::RetainTarget:{f.push(r[6].er);break;}
case S::DispatchMessage:{f.call(0x66d2);break;}
case S::RestoreTarget:{f.pop(r[6].er);break;}
case S::RestoreSource:{f.pop(r[5].er);break;}
case S::ReturnOffset:{f.ret();break;}
case S::SetCheckBase:{f.move(r[0].er,0x168fc);break;}
case S::ReadCheckFlag:{f.move(r[0].rl,observed);break;}
case S::AdmitCheck:{;break;}
case S::DispatchCheckMessage:{f.call(0x66d2);break;}
case S::ClearResult:{f.move(r[0].rl,0x0);break;}
case S::ReturnCheck:{f.ret();break;}
case S::SetMessage:{f.move(r[6].er,0x14ea6);break;}
case S::DispatchDisplay:{h.clockI(2);break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native store service clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingMessage())call.token=h.getSP();h.pc=h.makepc(task.awaitingMessage()?f.next:point(task).location);return true;}}
