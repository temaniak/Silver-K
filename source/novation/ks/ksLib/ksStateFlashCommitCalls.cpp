#include "ksStateFlashCommitCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateFlashCommitTask;using S=T::Stage;StateFlashCommitPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,11> points{{{0x6662,2},{0x6664,6},{0x666a,4},{0x666e,4},{0x6672,4},{0x6676,6},{0x667c,2},{0x667e,4},{0x6682,4},{0x6686,4},{0x668a,2}}};Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStateFlashCommitPartialForTests(StateFlashCommitPartialTest f){partialTest=f;}
bool StateFlashCommitCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x6662){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native flash-commit capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint16_t observed=0;auto read=[&]()->unsigned{return observed=h.read16(0xffed6a);};auto write=[&](uint16_t value){f.store16(0xffed6a,value);};if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native flash-commit admission differs");switch(stage){case S::ClearPending:{r[0].r=h.sub16(r[0].r,r[0].r);break;}
case S::WritePending:{;break;}
case S::DispatchProgram:{f.call(0x11ae);break;}
case S::RetainSource:{f.push(r[5].er);break;}
case S::RetainTarget:{f.push(r[6].er);break;}
case S::ReadPending:{f.move(r[0].r,observed);break;}
case S::AdmitPending:{;break;}
case S::DispatchIdle:{f.call(0x64c);break;}
case S::RestoreTarget:{f.pop(r[6].er);break;}
case S::RestoreSource:{f.pop(r[5].er);break;}
case S::ReturnCommit:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native flash-commit clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
