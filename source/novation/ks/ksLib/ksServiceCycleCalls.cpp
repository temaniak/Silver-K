#include "ksServiceCycleCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace {ServiceCyclePartialTest partialTest=nullptr;using S=ServiceCycleTask::Stage;
struct Dispatch {unsigned location,length,target;};constexpr std::array<Dispatch,17> services{{{0x4338,4,0x403a},{0x433c,4,0x4096},{0x4340,4,0x40f2},{0x4344,4,0x414e},{0x4348,4,0x41aa},{0x434c,4,0x4494},{0x4350,4,0x44cc},{0x4354,2,0x43a8},{0x4356,2,0x43c2},{0x4358,4,0x4dc0},{0x435c,4,0x4406},{0x4360,4,0x2a2a},{0x4364,4,0x9f5a},{0x4368,4,0x370e},{0x436c,4,0x6e0c},{0x4370,4,0x562},{0x4374,4,0xa12c}}};
unsigned location(const ServiceCycleTask& t){if(t.stage()==S::NextCycle)return 0x4378;if(t.stage()==S::AwaitChild)return t.index()==16?0x4378:services[t.index()+1].location;return services[t.index()].location;}
}
void setServiceCyclePartialForTests(ServiceCyclePartialTest fn){partialTest=fn;}
bool ServiceCycleCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0x4338&&!selected){for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native service cycle capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||location((*selected)->task)!=pc){if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& task=(*selected)->task;if(task.stage()==S::AwaitChild)task.resumeChild();const auto stage=task.stage();const auto before=h.cycles;
 const auto dispatch=stage==S::NextCycle?Dispatch{0x4378,2,0}:services[task.index()];const auto cost=task.nextClocks();Boundary f(h,pc,dispatch.length);
 if(task.advanceBefore(cost)!=cost)throw std::runtime_error("Native service cycle admission mismatch");
 if(stage==S::NextCycle){++cyclesCompleted;h.pc=h.makepc(0x4338);}else {f.call(dispatch.target);h.pc=h.makepc(f.next);}
 if(h.cycles-before!=cost)throw std::runtime_error("Native service cycle clocks differ");return true;
}
}
