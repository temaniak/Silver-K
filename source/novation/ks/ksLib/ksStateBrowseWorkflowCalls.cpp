#include "ksStateBrowseWorkflowCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateBrowseWorkflowPartialTest partialTest=nullptr;using T=StateBrowseWorkflowTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,126> points{{{0x5db2,6},{0x5db8,2},{0x5dba,6},{0x5dc0,2},{0x5dc2,2},{0x5dc4,4},{0x5dc8,2},{0x5dca,2},{0x5dcc,2},{0x5dce,2},{0x5dd0,2},{0x5dd2,6},{0x5dd8,2},{0x5dda,4},{0x5dde,4},{0x5de2,4},{0x5de6,4},{0x5dea,2},{0x5dec,6},{0x5df2,2},{0x5df4,2},{0x5df6,4},{0x5dfa,2},{0x5dfc,2},{0x5dfe,2},{0x5e00,2},{0x5e02,2},{0x5e04,6},{0x5e0a,2},{0x5e0c,4},{0x5e10,4},{0x5e14,4},{0x5e18,6},{0x5e1e,2},{0x5e20,4},{0x5e24,2},{0x5e26,6},{0x5e2c,2},{0x5e2e,2},{0x5e30,4},{0x5e34,2},{0x5e36,2},{0x5e38,2},{0x5e3a,2},{0x5e3c,2},{0x5e3e,6},{0x5e44,2},{0x5e46,4},{0x5e4a,4},{0x5e4e,4},{0x5e52,6},{0x5e58,2},{0x5e5a,4},{0x5e5e,2},{0x5e60,2},{0x5e62,2},{0x5e64,6},{0x5e6a,2},{0x5e6c,4},{0x5e70,6},{0x5e76,4},{0x5e7a,2},{0x5e7c,2},{0x5e7e,2},{0x5e80,2},{0x5e82,6},{0x5e88,4},{0x5e8c,2},{0x5e8e,2},{0x5e90,4},{0x5e94,2},{0x5e96,2},{0x5e98,4},{0x5e9c,4},{0x5ea0,4},{0x5ea4,6},{0x5eaa,4},{0x5eae,2},{0x5eb0,4},{0x5eb4,6},{0x5eba,4},{0x5ebe,2},{0x5ec0,6},{0x5ec6,8},{0x5ece,4},{0x5ed2,6},{0x5ed8,4},{0x5edc,6},{0x5ee2,2},{0x5ee4,2},{0x5ee6,4},{0x5eea,4},{0x5eee,4},{0x5ef2,2},{0x5ef4,6},{0x5efa,2},{0x5efc,6},{0x5f02,2},{0x5f04,2},{0x5f06,2},{0x5f08,6},{0x5f0e,4},{0x5f12,4},{0x5f16,4},{0x5f1a,4},{0x5f1e,2},{0x5f20,4},{0x5f24,8},{0x5f2c,6},{0x5f32,4},{0x5f36,2},{0x5f38,2},{0x5f3a,2},{0x5f3c,2},{0x5f3e,2},{0x5f40,2},{0x5f42,6},{0x5f48,8},{0x5f50,4},{0x5f54,2},{0x5f56,2},{0x5f58,2},{0x5f5a,2},{0x5f5c,2},{0x5f5e,4},{0x5f62,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::Refresh:return 0x5340;case T::Exit::Notice:return 0x55b8;default:throw std::runtime_error("Missing StateBrowseWorkflow exit");}}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateBrowseWorkflowPartialForTests(StateBrowseWorkflowPartialTest value){partialTest=value;}
bool StateBrowseWorkflowCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x5db2:kind=T::Kind::Browse;break;case 0x5e60:kind=T::Kind::Repeat;break;case 0x5e7c:kind=T::Kind::Slow;break;case 0x5e80:kind=T::Kind::Fast;break;case 0x5eb0:kind=T::Kind::Stop;break;case 0x5ebe:kind=T::Kind::Stopped;break;case 0x5ec0:kind=T::Kind::Notice;break;case 0x5edc:kind=T::Kind::Open;break;case 0x5efc:kind=T::Kind::Close;break;case 0x5f20:kind=T::Kind::Save;break;case 0x5f42:kind=T::Kind::Restore;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateBrowseWorkflow capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[6].r));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint8_t(r[0].rl));const auto stage=task.stage();
 auto accessCost=[&](uint32_t ref,unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){const unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};unsigned store=3;switch(stage){case S::ReadBrowseMode:store=accessCost(uint32_t(0x167ba),1,-1);break;case S::ReadBrowseMulti:store=accessCost(uint32_t(0x167d5),1,-1);break;case S::ReadProgramPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadBrowsePart:store=accessCost(uint32_t(0x16799),1,-1);break;case S::ReadBankPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadBankPartsPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadPartPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadPartDisplayPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadPartShowPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::WriteRepeatDelta:store=accessCost(uint32_t(0x167f6),1,h.lastwrite);break;case S::ReadRepeatDelta:store=accessCost(uint32_t(0x167f6),1,-1);break;case S::ReadBrowseGlobal:store=accessCost(uint32_t(r[6].er+229),1,-1);break;case S::ReadOpenedFlag:store=accessCost(uint32_t(0x16782),1,-1);break;case S::WriteOpenedFlag:store=accessCost(uint32_t(0x16782),1,h.lastwrite);break;case S::ReadClosedFlag:store=accessCost(uint32_t(0x16782),1,-1);break;case S::WriteClosedFlag:store=accessCost(uint32_t(0x16782),1,h.lastwrite);break;case S::ReadSavedProgram:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadSavedByte:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::WriteSavedByte:store=accessCost(uint32_t(r[1].er),1,h.lastwrite);break;case S::ReadRestoreProgram:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadRestoreByte:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::WriteRestoreByte:store=accessCost(uint32_t(r[1].er),1,h.lastwrite);break;default:break;} const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{if(field==T::Read::Patch)return uint32_t(h.read32(0x167bc));return uint8_t(h.read8(field==T::Read::Mode?0x167ba:field==T::Read::Multi?0x167d5:field==T::Read::Page?0x16790:field==T::Read::Part?0x16799:field==T::Read::RepeatDelta?0x167f6:field==T::Read::Global?0x169e1:field==T::Read::Opened?0x16782:ref));};auto write=[&](T::Write field,uint32_t ref,uint8_t value){f.store8(field==T::Write::RepeatDelta?0x167f6:field==T::Write::Opened?0x16782:ref,int8_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateBrowseWorkflow admission differs");
 switch(stage){case S::ReadBrowseMode:f.move(r[0].rl,task.value());break;
case S::ReadBrowseMulti:f.move(r[0].rl,task.value());break;
case S::SaveProgramEvent:f.push(r[6].r);break;
case S::StepProgram:f.call(0x5fb0);break;
case S::RestoreProgramEvent:f.pop(r[6].r);break;
case S::TestProgramResult:f.move(r[0].rl,r[0].rl);break;
case S::CompareProgramResult:h.sub8(r[0].rl,0x1);break;
case S::ReadProgramPage:f.move(r[0].rl,task.value());break;
case S::ArmProgramRepeat:f.call(0x5e60);break;
case S::LoadProgramContext:f.call(0x62e6);break;
case S::RefreshProgramBrowser:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ShowProgramBrowser:f.call(0x6094);break;
case S::ReturnProgramBrowser:f.ret();break;
case S::ReadBrowsePart:f.move(r[0].rl,task.value());break;
case S::SaveBankEvent:f.push(r[6].r);break;
case S::StepBank:f.call(0x53a8);break;
case S::RestoreBankEvent:f.pop(r[6].r);break;
case S::TestBankResult:f.move(r[0].rl,r[0].rl);break;
case S::CompareBankResult:h.sub8(r[0].rl,0x1);break;
case S::ReadBankPage:f.move(r[0].rl,task.value());break;
case S::LoadBankContext:f.call(0x633a);break;
case S::RefreshBankBrowser:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ShowBankBrowser:f.call(0x5470);break;
case S::ReadBankPartsPage:f.move(r[0].rl,task.value());break;
case S::ShowBankParts:f.call(0x54fe);break;
case S::ReturnBankBrowser:f.ret();break;
case S::ReadPartPage:f.move(r[0].rl,task.value());break;
case S::SavePartEvent:f.push(r[6].r);break;
case S::StepPart:f.call(0x5fb0);break;
case S::RestorePartEvent:f.pop(r[6].r);break;
case S::TestPartResult:f.move(r[0].rl,r[0].rl);break;
case S::ComparePartResult:h.sub8(r[0].rl,0x1);break;
case S::ReadPartDisplayPage:f.move(r[0].rl,task.value());break;
case S::PreparePart:f.call(0x45b6);break;
case S::LoadPartContext:f.call(0x62e6);break;
case S::RefreshPartBrowser:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadPartShowPage:f.move(r[0].rl,task.value());break;
case S::ShowPartParts:f.call(0x54fe);break;
case S::ReturnPartBrowser:f.ret();break;
case S::CompareRepeatKey:h.sub8(r[6].rl,0xfffffff0);break;
case S::BindRepeatDelay:f.move(r[6].r,0x190);break;
case S::BindRepeatCallback:f.move(r[5].er,0x5e80);break;
case S::ScheduleRepeat:f.call(0x4134);break;
case S::ReturnRepeat:f.ret();break;
case S::BindSlowWeight:f.move(r[6].rl,0xa);break;
case S::BindFastWeight:f.move(r[6].rl,0x9);break;
case S::ReadRepeatDelta:f.move(r[6].rh,task.delta());break;
case S::MultiplyRepeatDelta:f.multiplySigned(r[6].rh,r[6].r);break;
case S::BindRepeatHigh:f.move(r[6].rh,r[6].rl);break;
case S::ClearRepeatLow:f.move(r[6].rl,0x0);break;
case S::StepRepeatedProgram:f.call(0x5fb0);break;
case S::CompareRepeatedResult:h.sub8(r[0].rl,0x3);break;
case S::LoadRepeatedContext:f.call(0x62e6);break;
case S::RefreshRepeatedScreen:f.call(0x5340);break;
case S::BindNextRepeatDelay:f.move(r[6].r,0x12c);break;
case S::BindNextRepeatCallback:f.move(r[5].er,0x5e7c);break;
case S::ScheduleNextRepeat:f.call(0x4134);break;
case S::ReturnRepeated:f.ret();break;
case S::BindStopDelay:f.move(r[6].r,0x64);break;
case S::BindStopCallback:f.move(r[5].er,0x5ebe);break;
case S::ScheduleStop:f.call(0x4134);break;
case S::ReturnStop:f.ret();break;
case S::BindBrowseGlobal:f.move(r[6].er,0x168fc);break;
case S::ReadBrowseGlobal:f.move(r[6].rl,task.value());break;
case S::ApplyBrowseGlobal:f.call(0xaf3a);break;
case S::BindBrowseNotice:f.move(r[6].er,0x14edf);break;
case S::ShowBrowseNotice:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadOpenedFlag:f.move(r[0].rl,task.value());break;
case S::SaveOriginalProgram:f.call(0x5f20);break;
case S::PrepareOpenedProgram:f.call(0x6382);break;
case S::RefreshOpenedPrograms:f.call(0x7354);break;
case S::RefreshOpenedVoices:f.call(0x6c3c);break;
case S::BindOpenedFlag:f.move(r[0].rl,0x1);break;
case S::ReturnOpened:f.ret();break;
case S::ReadClosedFlag:f.move(r[0].rl,task.value());break;
case S::RestoreOriginalProgram:f.call(0x5f42);break;
case S::ClearOpenedFlag:f.move(r[0].rl,0x0);break;
case S::ResetClosedMidi:f.call(0x3074);break;
case S::RefreshClosedPrograms:f.call(0x7354);break;
case S::RefreshClosedVoices:f.call(0x6c3c);break;
case S::RefreshClosedEvents:f.call(0x9fe4);break;
case S::ReturnClosed:f.ret();break;
case S::PrepareSavedProgram:f.call(0x687e);break;
case S::ReadSavedProgram:f.move(r[0].er,task.source());break;
case S::BindSavedBuffer:f.move(r[1].er,0x167fc);break;
case S::BindSavedLength:f.move(r[2].e,0x100);break;
case S::ReadSavedByte:f.move(r[2].rl,task.value());r[0].er+=1;h.clockI(2);break;
case S::NextSavedDestination:f.inc(r[1].er,1);break;
case S::TickSavedLength:f.inc(r[2].e,-1);break;
case S::ReturnSaved:f.ret();break;
case S::BindRestoreBuffer:f.move(r[0].er,0x167fc);break;
case S::ReadRestoreProgram:f.move(r[1].er,task.destination());break;
case S::BindRestoreLength:f.move(r[2].e,0x100);break;
case S::ReadRestoreByte:f.move(r[2].rl,task.value());r[0].er+=1;h.clockI(2);break;
case S::NextRestoreDestination:f.inc(r[1].er,1);break;
case S::TickRestoreLength:f.inc(r[2].e,-1);break;
case S::PublishRestoredProgram:f.call(0x6726);break;
case S::ReturnRestored:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateBrowseWorkflow clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
