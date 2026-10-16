#include "ksStateAuditionCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateAuditionTask;using S=T::Stage;StateAuditionPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,20> points{{{0x6e0c,4},{0x6e10,2},{0x6e12,2},{0x6e14,4},{0x6e18,2},{0x6e1a,2},{0x6e1c,4},{0x6e20,2},{0x6e22,2},{0x6e24,4},{0x6e28,2},{0x6e2a,2},{0x6e2c,4},{0x6e30,2},{0x6e32,2},{0x6e34,4},{0x6e38,2},{0x6e3a,2},{0x6e3c,4},{0x6e40,2}}};Point point(const T& t){if(t.awaitingPoll())return {0x6e10,2};if(t.awaitingNote())return {t.event()==T::Event::On?0x2b6au:0x2b60u,0};return points[unsigned(t.awaitingConfigure()?t.resumeStage():t.stage())];}}
void setStateAuditionPartialForTests(StateAuditionPartialTest f){partialTest=f;}
bool StateAuditionCalls::retireReturns(ControllerCpu& h){bool retired=false;for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingNote()&&point(c->task).location==h.getPC()){c->task.retireNote();c.reset();++completed;retired=true;}return retired;}
bool StateAuditionCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x6e0c){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native audition capacity exceeded");selected->emplace(h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingPoll())task.resumePoll(uint8_t(r[6].rl));if(task.awaitingConfigure())task.resumeConfigure();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);if(task.advanceBefore(cost)!=cost)throw std::runtime_error("Native audition admission differs");switch(stage){case S::Poll:{f.call(0x13ae);break;}
case S::CompareOn:{h.sub8(r[6].rl,0x20);break;}
case S::AdmitOn:{;break;}
case S::SetOnTarget:{f.move(r[4].r,0x8057);break;}
case S::ClearOnPayload:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::SetOnMode:{f.move(r[1].rh,0x1);break;}
case S::ConfigureOn:{f.call(0x24c6);break;}
case S::SetOnNote:{f.move(r[6].rl,0x3c);break;}
case S::SetOnVelocity:{f.move(r[6].rh,0x7f);break;}
case S::TailOn:{h.clockI(2);break;}
case S::CompareOff:{h.sub8(r[6].rl,0x10);break;}
case S::AdmitOff:{;break;}
case S::SetOffTarget:{f.move(r[4].r,0x8057);break;}
case S::ClearOffPayload:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::SetOffMode:{f.move(r[1].rh,0x2);break;}
case S::ConfigureOff:{f.call(0x24c6);break;}
case S::SetOffNote:{f.move(r[6].rl,0x3c);break;}
case S::SetOffVelocity:{f.move(r[6].rh,0x40);break;}
case S::TailOff:{h.clockI(2);break;}
case S::Return:{f.ret();break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native audition clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(task.awaitingPoll()||task.awaitingConfigure()?f.next:point(task).location);return true;}}
