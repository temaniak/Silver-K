#include "ksStatePostEditCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StatePostEditTask;using S=T::Stage;StatePostEditPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,25> points{{{0x6bf0,4},{0x6bf4,2},{0x6bf6,8},{0x6bfe,2},{0x6c00,8},{0x6c08,2},{0x6c0a,2},{0x6c0c,2},{0x6c0e,4},{0x6c12,2},{0x6c14,2},{0x6c16,2},{0x6c18,2},{0x6c1a,2},{0x6c1c,2},{0x6c1e,2},{0x6c20,8},{0x6c28,2},{0x6c2a,4},{0x6c2e,2},{0x6c30,2},{0x6c32,2},{0x6c34,2},{0x6c36,4},{0x6c3a,2}}};Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStatePostEditPartialForTests(StatePostEditPartialTest f){partialTest=f;}
bool StatePostEditCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x6bf0){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native postedit capacity exceeded");selected->emplace(uint32_t(h.regs[1].er),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what)->uint32_t{return observed=what==T::Read::Enabled?uint8_t(h.read8(task.base()+103)):uint32_t(h.read32(what==T::Read::Override?0x167a6:0x167bc));};if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native postedit admission differs");switch(stage){case S::RetainReference:{f.push(r[1].er);break;}
case S::RetainRequest:{f.push(r[6].r);break;}
case S::ReadOverride:{f.move(r[0].er,observed);break;}
case S::AdmitOverride:{;break;}
case S::ReadCurrent:{f.move(r[0].er,observed);break;}
case S::ComputeField:{r[1].er=h.sub32(r[1].er,r[0].er);break;}
case S::Compare217:{h.sub8(r[1].rl,0xffffffd9);break;}
case S::Admit217:{;break;}
case S::RefreshRate:{f.call(0xa182);break;}
case S::SkipState:{;break;}
case S::Compare102:{h.sub8(r[1].rl,0x66);break;}
case S::Admit102:{;break;}
case S::Compare103:{h.sub8(r[1].rl,0x67);break;}
case S::Admit103:{;break;}
case S::Compare84:{h.sub8(r[1].rl,0x54);break;}
case S::Admit84:{;break;}
case S::ReadEnabled:{f.move(r[0].rl,observed);break;}
case S::AdmitEnabled:{;break;}
case S::RefreshState:{f.call(0xa188);break;}
case S::SkipCompare218:{;break;}
case S::Compare218:{h.sub8(r[1].rl,0xffffffda);break;}
case S::Admit218:{;break;}
case S::RestoreRequest:{f.pop(r[6].r);break;}
case S::RestoreReference:{f.pop(r[1].er);break;}
case S::ReturnPost:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native postedit clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
