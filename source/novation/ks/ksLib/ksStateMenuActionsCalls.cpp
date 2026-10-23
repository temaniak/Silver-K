#include "ksStateMenuActionsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateMenuActionsPartialTest partialTest=nullptr;using T=StateMenuActionsTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,81> points{{{0x4528,4},{0x452c,6},{0x4532,4},{0x4536,4},{0x453a,4},{0x453e,4},{0x4698,2},{0x469a,2},{0x469c,2},{0x469e,6},{0x46a4,4},{0x46a8,2},{0x46aa,2},{0x46ac,4},{0x46b0,6},{0x46b6,2},{0x46b8,4},{0x46bc,6},{0x46c2,4},{0x46c6,4},{0x46ca,2},{0x46cc,4},{0x46d0,4},{0x46d4,4},{0x46d8,2},{0x46da,4},{0x46de,6},{0x46e4,4},{0x46e8,4},{0x46ec,4},{0x46f0,6},{0x46f6,2},{0x46f8,4},{0x46fc,6},{0x4702,4},{0x4706,4},{0x470a,2},{0x470c,4},{0x4710,4},{0x4714,4},{0x4718,2},{0x471a,4},{0x471e,6},{0x4724,4},{0x4728,4},{0x472c,4},{0x4730,2},{0x4732,6},{0x4738,6},{0x473e,4},{0x4742,2},{0x4744,4},{0x4748,6},{0x474e,4},{0x4752,4},{0x4756,6},{0x475c,2},{0x475e,6},{0x4764,2},{0x4766,2},{0x4768,4},{0x476c,6},{0x4772,4},{0x4776,4},{0x477a,2},{0x477c,2},{0x477e,6},{0x4784,4},{0x4788,2},{0x478a,4},{0x478e,2},{0x4790,6},{0x4796,4},{0x479a,2},{0x479c,4},{0x47a0,2},{0x47a2,2},{0x47a4,6},{0x47aa,4},{0x47ae,6},{0x47b4,4}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::Release:return 0x4862;case T::Exit::RefreshToggle:return 0x6c3c;case T::Exit::Auxiliary:return 0xb83c;case T::Exit::ReleaseNavigation:return 0x5eb0;case T::Exit::PreviousName:return 0x75a0;case T::Exit::PreviousSpecial:return 0xb60c;case T::Exit::NextName:return 0x7548;case T::Exit::NextSpecial:return 0xb5e4;case T::Exit::Edit:return 0x5db2;case T::Exit::PreviousProgram:return 0x58e4;case T::Exit::NextProgram:return 0x58ce;case T::Exit::Notice:return 0x48cc;case T::Exit::FirstPage:return 0x5ec0;case T::Exit::SecondPage:return 0x66ea;case T::Exit::ProgramStore:return 0x482a;case T::Exit::MultiStore:return 0x484a;case T::Exit::PanelStore:return 0x47d6;case T::Exit::PartStore:return 0x4858;case T::Exit::CommonStore:return 0x47b8;case T::Exit::Handler:return 0x0;default:throw std::runtime_error("Missing menu action exit");}}
unsigned byteClocks(unsigned at,int previous){if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateMenuActionsPartialForTests(StateMenuActionsPartialTest value){partialTest=value;}
bool StateMenuActionsCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x4528:kind=T::Kind::Toggle;break;case 0x453a:kind=T::Kind::Auxiliary;break;case 0x4698:kind=T::Kind::Dispatch;break;case 0x46ac:kind=T::Kind::Previous;break;case 0x46da:kind=T::Kind::RepeatPrevious;break;case 0x46ec:kind=T::Kind::Next;break;case 0x471a:kind=T::Kind::RepeatNext;break;case 0x472c:kind=T::Kind::Store;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateMenuActions capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[6].r),!(h.ccr&4));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(!(h.ccr&4));const auto stage=task.stage();
 const auto store=byteClocks(stage==S::FlipToggle?0x167a1:stage==S::ClearStoreSpecial?0x1679e:0x1679f,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field)->uint32_t{switch(field){case T::Read::Toggle:return uint8_t(h.read8(0x167a1));case T::Read::UiPage:return uint8_t(h.read8(0x16790));case T::Read::Special:return uint8_t(h.read8(0x1679e));case T::Read::Edit:return uint8_t(h.read8(0x16784));case T::Read::Screen:return uint8_t(h.read8(0x16785));case T::Read::Page:return uint8_t(h.read8(0x16787));case T::Read::Mode:return uint8_t(h.read8(0x167ba));case T::Read::Part:return uint8_t(h.read8(0x16799));case T::Read::Handler:call.handler=uint32_t(h.read32(0xfd1e + task.index()));return 0;}return 0;};auto write=[&](T::Write field,uint8_t v){switch(field){case T::Write::Toggle:h.write8(v,0x167a1);break;case T::Write::Special:f.store8(0x1679e,int8_t(v));break;case T::Write::Auxiliary:f.store8(0x1679f,int8_t(v));break;}};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateMenuActions admission differs");
 switch(stage){case S::SelectToggle:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindToggle:f.move(r[0].er,0x167a1);break;
case S::RefreshToggle:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::SelectAuxiliary:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::EnterAuxiliary:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ClearActionIndex:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::ReadActionIndex:f.move(r[0].rl,r[6].rl);break;
case S::ScaleActionIndex:f.shift(r[0].rl,2,true);break;
case S::BindActionTable:r[0].er=h.add32(r[0].er,0xfd1e);break;
case S::ReadActionHandler:f.move(r[0].er,int32_t(call.handler));break;
case S::TestActionEvent:f.move(r[6].rh,r[6].rh);break;
case S::DispatchAction:f.jump(call.handler&0xffffff,false);break;
case S::SelectPrevious:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadPreviousPage:f.move(r[0].rl,task.page());break;
case S::TestPreviousPage:h.sub8(r[0].rl,0x2);break;
case S::RoutePreviousName:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadPreviousSpecial:f.move(r[0].rl,task.special());break;
case S::RoutePreviousSpecial:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ObservePreviousEdit:f.call(0x4efe);break;
case S::SelectPreviousEdit:if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindPreviousDelta:f.move(r[6].r,0xffff);break;
case S::EditPrevious:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindPreviousDelay:f.move(r[6].r,0x190);break;
case S::JoinPreviousRepeat:if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindPreviousRepeat:f.move(r[6].r,0xc8);break;
case S::BindPreviousCallback:f.move(r[5].er,0x46da);break;
case S::SchedulePrevious:f.call(0x4134);break;
case S::PreviousProgram:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::SelectNext:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadNextPage:f.move(r[0].rl,task.page());break;
case S::TestNextPage:h.sub8(r[0].rl,0x2);break;
case S::RouteNextName:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadNextSpecial:f.move(r[0].rl,task.special());break;
case S::RouteNextSpecial:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ObserveNextEdit:f.call(0x4efe);break;
case S::SelectNextEdit:if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindNextDelta:f.move(r[6].r,0x1ff);break;
case S::EditNext:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindNextDelay:f.move(r[6].r,0x190);break;
case S::JoinNextRepeat:if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindNextRepeat:f.move(r[6].r,0xc8);break;
case S::BindNextCallback:f.move(r[5].er,0x471a);break;
case S::ScheduleNext:f.call(0x4134);break;
case S::NextProgram:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::SelectStore:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ClearStoreFlag:f.move(r[0].rl,0x0);break;
case S::ObserveCalibrationGate:f.call(0x590a);break;
case S::SelectCalibrationGate:if(task.complete())f.next=exitAddress(task.exit());break;
case S::SaveCalibration:f.call(0x2070);break;
case S::BindCalibrationNotice:f.move(r[6].er,0x14ec9);break;
case S::ShowCalibrationNotice:f.call(0x55b8);break;
case S::FinishCalibration:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadStoreEdit:f.move(r[0].rl,task.edit());break;
case S::SelectStoreEdit:if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadStoreScreen:f.move(r[0].rl,task.screen());break;
case S::TestProgramScreen:h.sub8(r[0].rl,0x0);break;
case S::SelectProgramScreen:if(task.complete())f.next=exitAddress(task.exit());break;
case S::StoreProgram:f.call(0x69e2);break;
case S::BindProgramNotice:f.move(r[6].er,0x14eba);break;
case S::ShowProgramNotice:f.call(0x55b8);break;
case S::FinishProgram:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::TestStoreScreen:h.sub8(r[0].rl,0x9);break;
case S::SelectStoreScreen:if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadStorePage:f.move(r[0].rl,task.page());break;
case S::SelectFirstStorePage:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::DecrementStorePage:f.inc(r[0].rl,-1);break;
case S::SelectSecondStorePage:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReturnStore:f.ret();break;
case S::ReadStoreUi:f.move(r[0].rl,task.ui());break;
case S::SelectProgramStore:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::TestMultiStore:h.sub8(r[0].rl,0x1);break;
case S::SelectMultiStore:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::TestPanelStore:h.sub8(r[0].rl,0x3);break;
case S::SelectPanelStore:if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadStoreMode:f.move(r[0].rl,task.mode());break;
case S::SelectStoreMode:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadStorePart:f.move(r[0].rl,task.part());break;
case S::SelectStorePart:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateMenuActions clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
