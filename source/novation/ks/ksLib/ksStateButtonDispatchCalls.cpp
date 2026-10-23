#include "ksStateButtonDispatchCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateButtonDispatchPartialTest partialTest=nullptr;using T=StateButtonDispatchTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,35> points{{{0x44cc,4},{0x44d0,2},{0x44d2,2},{0x44d4,2},{0x44d6,6},{0x44dc,2},{0x44de,2},{0x44e0,2},{0x44e2,2},{0x44e4,2},{0x44e6,2},{0x44e8,4},{0x44ec,2},{0x44ee,4},{0x44f2,2},{0x44f4,2},{0x44f6,2},{0x44f8,6},{0x44fe,2},{0x4500,2},{0x4502,2},{0x4504,2},{0x4506,2},{0x4508,2},{0x450a,2},{0x450c,2},{0x450e,4},{0x4512,2},{0x4514,2},{0x4516,4},{0x451a,2},{0x451c,4},{0x4520,2},{0x4522,4},{0x4526,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
}
void setStateButtonDispatchPartialForTests(StateButtonDispatchPartialTest value){partialTest=value;}
bool StateButtonDispatchCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 if(pc==0x44cc||pc==0x4514){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native button dispatch capacity exceeded");slot->emplace(pc==0x44cc?T::Kind::Button:T::Kind::Action,h.getSP(),uint8_t(h.regs[1].rh));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint16_t(r[6].r));const auto stage=task.stage();
 const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field)->uint8_t{return uint8_t(h.read8(0xfcc4+task.offset()+(field==T::Read::Tag?0:field==T::Read::Key?1:2)));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native button dispatch admission differs");
 switch(stage){case S::PopButton:f.call(0x1412);break;
case S::TestButton:f.move(r[6].rh,r[6].rh);break;
case S::ReturnEmpty:f.ret();break;
case S::SelectUiState:f.move(r[2].er,0x1678f);break;
case S::CaptureGroup:f.move(r[5].rl,r[6].rl);break;
case S::CaptureKey:r[6].rl&=0x1f;value(h,r[6].rl);break;
case S::MaskGroup:r[5].rl&=0xffffffe0;value(h,r[5].rl);break;
case S::ComparePartGroup:h.sub8(r[5].rl,0x20);break;
case S::SelectPartGroup:if(task.complete())f.next=0x4542;break;
case S::ComparePageGroup:h.sub8(r[5].rl,0x40);break;
case S::SelectPageGroup:h.clockI(2);if(task.complete())f.next=0x45d4;break;
case S::CompareFunctionGroup:h.sub8(r[5].rl,0x60);break;
case S::SelectFunctionGroup:h.clockI(2);if(task.complete())f.next=0x4c58;break;
case S::SetDescriptorStride:f.move(r[0].rl,0x3);break;
case S::ScaleDescriptor:r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[6].rl));h.clockI(12);break;
case S::NormalizeDescriptor:r[0].er&=0xffff;value(h,r[0].er);break;
case S::LocateDescriptor:r[0].er=h.add32(r[0].er,0xfcc4);break;
case S::ReadTag:f.move(r[1].rh,task.tag());r[0].er+=1;h.clockI(2);break;
case S::ReadKey:f.move(r[6].rl,task.key());r[0].er+=1;h.clockI(2);break;
case S::ReadValue:f.move(r[0].rl,task.value());break;
case S::SaveTag:f.push(r[1].r);break;
case S::NormalizeTag:r[1].rh&=0x7f;value(h,r[1].rh);break;
case S::DispatchAction:f.call(0x4514);break;
case S::RestoreTag:f.pop(r[1].r);break;
case S::TestRefresh:flag(h,4,(uint8_t(r[1].rh)&(1u<<0x7))==0);break;
case S::SelectRefresh:h.clockI(2);if(task.complete())f.next=0x4e8a;break;
case S::ReturnButton:f.ret();break;
case S::CompareDirectAction:h.sub8(r[1].rh,0x0);break;
case S::SelectDirectAction:h.clockI(2);if(task.complete())f.next=0x4e7c;break;
case S::CompareToggleAction:h.sub8(r[1].rh,0x1);break;
case S::SelectToggleAction:h.clockI(2);if(task.complete())f.next=0x4698;break;
case S::CompareValueAction:h.sub8(r[1].rh,0x2);break;
case S::SelectValueAction:h.clockI(2);if(task.complete())f.next=0x49e8;break;
case S::ReturnUnknown:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native button dispatch clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
