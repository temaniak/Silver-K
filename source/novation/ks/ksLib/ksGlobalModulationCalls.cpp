#include "ksGlobalModulationCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
using S=GlobalModulationTask::Stage;GlobalModulationPartialTest partialTest=nullptr;
struct Point{unsigned location,length;};
constexpr std::array<Point,9> setup{{{0x7a9c,6},{0x7aa2,6},{0x7aa8,2},{0x7aaa,2},{0x7aac,2},{0x7aae,2},{0x7ab0,2},{0x7ab2,2},{0x7ab4,2}}};
constexpr std::array<Point,8> outputs{{{0x0,6},{0x6,2},{0x8,2},{0xa,2},{0xc,2},{0xe,4},{0x12,2},{0x14,4}}};
Point point(const GlobalModulationTask& t){const auto s=unsigned(t.stage());if(s<9)return setup[s];
 if(t.awaitingWord())return {0x7ab6+0x18*(t.output()+1),0};if(t.stage()==S::Return)return {0x7b46,2};
 const auto p=outputs[s-9];return {0x7ab6+0x18*t.output()+p.location,p.length};}
}
void setGlobalModulationPartialForTests(GlobalModulationPartialTest fn){partialTest=fn;}
bool GlobalModulationCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(selected&&(*selected)->task.awaitingWord()&&point((*selected)->task).location==pc)(*selected)->task.resumeWord();
 if(pc==0x7a9c){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
 if(!selected)throw std::runtime_error("Native global-modulation capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& task=(**selected).task;const auto stage=task.stage();auto& r=h.regs;const auto cost=task.nextClocks();const auto before=h.cycles;Boundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](GlobalModulationTask::Read what)->uint32_t{unsigned address=what==GlobalModulationTask::Read::Part?0x17526:what==GlobalModulationTask::Read::Base?0x167ba:0x17518+task.levelIndex();return observed=uint8_t(h.read8(address));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native global-modulation admission mismatch");
 switch(stage){case S::ReadPart:{f.move(r[2].rl,int8_t(observed));break;}
case S::ReadBase:{f.move(r[0].rl,int8_t(observed));break;}
case S::Difference:{r[2].rl=h.sub8(r[2].rl,r[0].rl);break;}
case S::AdmitNegative:{;break;}
case S::ClampNegative:{f.move(r[2].rl,0x0);break;}
case S::CaptureDifference:{f.move(r[0].rl,r[2].rl);break;}
case S::DoubleDifference:{f.shift(r[0].rl,1,true);break;}
case S::AddDifference:{r[2].rl=h.add8(r[2].rl,r[0].rl);break;}
case S::ClearDifference:{r[2].r&=0xff;value(h,r[2].r);break;}
case S::ReadLevel:{f.move(r[1].rh,int8_t(observed));break;}
case S::CaptureLevel:{f.move(r[1].rl,r[1].rh);break;}
case S::DoubleLevel:{f.shift(r[1].rl,1,true);break;}
case S::CaptureWord:{f.move(r[1].e,r[1].r);break;}
case S::ClearMiddle:{r[1].rh=h.sub8(r[1].rh,r[1].rh);break;}
case S::SetOffset:{f.move(r[4].r,int16_t(0x19+task.levelIndex()));break;}
case S::AddOffset:{r[4].r=h.add16(r[4].r,r[2].r);break;}
case S::DispatchWord:{if((uint32_t(r[1].er)>>8)!=task.dspWord()||uint16_t(r[4].r)!=task.offset())throw std::runtime_error("Native global-modulation payload projection differs");f.call(task.targetA()?0x24c6:0x24be);break;}
case S::Return:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native global-modulation clocks differ");
 if(stage==S::DispatchWord){h.pc=h.makepc(f.next);return true;}
 if(stage==S::Return){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}
 h.pc=h.makepc(point(task).location);return true;
}
}
