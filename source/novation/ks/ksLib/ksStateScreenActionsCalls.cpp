#include "ksStateScreenActionsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateScreenActionsPartialTest partialTest=nullptr;using T=StateScreenActionsTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,130> points{{{0x4862,2},{0x4864,2},{0x4866,6},{0x486c,2},{0x486e,2},{0x4870,2},{0x4872,6},{0x4878,2},{0x487a,2},{0x487c,6},{0x4882,2},{0x4884,2},{0x4886,2},{0x4888,6},{0x488e,2},{0x4890,8},{0x4898,6},{0x489e,4},{0x48a2,2},{0x48a4,6},{0x48aa,4},{0x48ae,4},{0x48b2,4},{0x48b6,2},{0x48b8,6},{0x48be,4},{0x48c2,2},{0x48c4,4},{0x48c8,4},{0x48cc,2},{0x48ce,6},{0x48d4,6},{0x48da,2},{0x48dc,2},{0x48de,6},{0x48e4,6},{0x48ea,6},{0x48f0,6},{0x48f6,6},{0x48fc,6},{0x4902,6},{0x4908,6},{0x490e,6},{0x4914,6},{0x491a,6},{0x4920,4},{0x4924,4},{0x4928,2},{0x492a,6},{0x4930,6},{0x4936,6},{0x493c,6},{0x4942,6},{0x4948,2},{0x494a,6},{0x4950,4},{0x4954,2},{0x4956,2},{0x4958,4},{0x495c,4},{0x4960,4},{0x4964,4},{0x4968,4},{0x496c,4},{0x4970,2},{0x4972,2},{0x4974,4},{0x4978,2},{0x497a,6},{0x4980,2},{0x4982,4},{0x4986,2},{0x4988,4},{0x498c,6},{0x4992,4},{0x4996,6},{0x499c,2},{0x499e,6},{0x49a4,2},{0x49a6,4},{0x49aa,4},{0x49ae,2},{0x49b0,4},{0x49b4,2},{0x49b6,6},{0x49bc,6},{0x49c2,4},{0x49c6,4},{0x49ca,6},{0x49d0,2},{0x49d2,6},{0x49d8,4},{0x49dc,4},{0x49e0,4},{0x49e4,4},{0x49e8,2},{0x49ea,4},{0x49ee,2},{0x49f0,6},{0x49f6,6},{0x49fc,4},{0x4a00,4},{0x4a04,6},{0x4a0a,2},{0x4a0c,2},{0x4a0e,2},{0x4a10,6},{0x4a16,2},{0x4a18,6},{0x4a1e,2},{0x4a20,6},{0x4a26,4},{0x4a2a,4},{0x4a2e,4},{0x4a32,4},{0x4a36,2},{0x4a38,2},{0x4a3a,8},{0x4a42,2},{0x4a44,2},{0x4a46,2},{0x4a48,4},{0x4a4c,4},{0x4a50,2},{0x4a52,2},{0x4a54,8},{0x4a5c,2},{0x4a5e,2},{0x4a60,2},{0x4a62,4}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::Release:return 0x4862;case T::Exit::ResetVoices:return 0x4bd0;case T::Exit::ShowUi:return 0x559e;case T::Exit::NameStep:return 0x757c;case T::Exit::PanelStep:return 0x752c;case T::Exit::SpecialStep:return 0xb632;case T::Exit::MultiStep:return 0x5552;case T::Exit::Refresh:return 0x5340;case T::Exit::ApplyPatch:return 0x6ae6;default:throw std::runtime_error("Missing screen action exit");}}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateScreenActionsPartialForTests(StateScreenActionsPartialTest value){partialTest=value;}
bool StateScreenActionsCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x4862:kind=T::Kind::Return;break;case 0x4864:kind=T::Kind::Single;break;case 0x48b6:kind=T::Kind::Playback;break;case 0x48be:kind=T::Kind::ResetPlayback;break;case 0x48cc:kind=T::Kind::ClearEdit;break;case 0x48dc:kind=T::Kind::ClearUi;break;case 0x4924:kind=T::Kind::Multi;break;case 0x496c:kind=T::Kind::Down;break;case 0x4974:kind=T::Kind::Up;break;case 0x497a:kind=T::Kind::Step;break;case 0x49b0:kind=T::Kind::ToggleEdit;break;case 0x49e8:kind=T::Kind::SelectEdit;break;case 0x4a32:kind=T::Kind::ToggleOne;break;case 0x4a4c:kind=T::Kind::ToggleTwo;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateScreenActions capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[6].r),!(h.ccr&4));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(!(h.ccr&4));const auto stage=task.stage();
 const auto store=byteClocks(stage==S::WriteUiMulti?0x167d5:stage==S::WriteUiPage?0x16790:stage==S::WriteUiPart?0x16799:stage==S::WriteEditEventOn?0x16784:stage==S::WriteUiTranspose?0x167a3:stage==S::WriteSinglePreset?0x167ae:stage==S::WriteUiNote?0x16783:stage==S::WriteEditEventActive?0x16784:stage==S::WriteSingleReload?0x167a2:stage==S::WriteMultiRequest?0x167d0:stage==S::WriteSingleMode?0x167ba:stage==S::WriteEditToggleLatch?0x16792:stage==S::WritePlaybackPage?0x16790:stage==S::WriteMultiHeld?0x167d1:stage==S::WriteMultiMode?0x167ba:stage==S::WriteEditActive?0x16784:stage==S::WriteEditToggleValue?0x16784:stage==S::WriteEditLatch?0x16792:stage==S::WriteUiLast?0x167fb:stage==S::WriteUiSpecial?0x1679e:stage==S::WriteUiAuxiliary?0x167a0:stage==S::WriteUiOther?0x1679f:stage==S::WriteEditEventLatch?0x16792:stage==S::WriteUiEdit?0x16784:stage==S::WriteUiLatch?0x16792:0,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read f,uint32_t ref)->uint32_t{switch(f){case T::Read::EditPart:return uint8_t(h.read8(0x16799));case T::Read::Mode:return uint8_t(h.read8(0x167ba));case T::Read::ActivePart:return uint8_t(h.read8(0x16798));case T::Read::Current:return uint8_t(h.read8(0x167b8));case T::Read::Held:return uint8_t(h.read8(0x167b9));case T::Read::UiPage:return uint8_t(h.read8(0x16790));case T::Read::Special:return uint8_t(h.read8(0x1679e));case T::Read::Multi:return uint8_t(h.read8(0x167d5));case T::Read::Edit:return uint8_t(h.read8(0x16784));case T::Read::Screen:return uint8_t(h.read8(0x16785));case T::Read::Preset:return uint16_t(h.read16(92078+ref));case T::Read::PatchBase:return uint32_t(h.read32(0x167bc));case T::Read::PatchByte:return uint8_t(h.read8(ref));}return 0;};auto write=[&](T::Write field,uint16_t v){switch(field){case T::Write::Mode:f.store8(0x167ba,int8_t(v));break;case T::Write::Preset:f.store16(0x167ae,int16_t(v));break;case T::Write::Reload:f.store8(0x167a2,int8_t(v));break;case T::Write::UiPage:f.store8(0x16790,int8_t(v));break;case T::Write::Edit:f.store8(0x16784,int8_t(v));break;case T::Write::Latch:f.store8(0x16792,int8_t(v));break;case T::Write::Request:f.store8(0x167d0,int8_t(v));break;case T::Write::HeldRequest:f.store8(0x167d1,int8_t(v));break;case T::Write::Clear167fb:f.store8(0x167fb,int8_t(v));break;case T::Write::Clear1679e:f.store8(0x1679e,int8_t(v));break;case T::Write::Clear167a0:f.store8(0x167a0,int8_t(v));break;case T::Write::Clear1679f:f.store8(0x1679f,int8_t(v));break;case T::Write::Clear16784:f.store8(0x16784,int8_t(v));break;case T::Write::Clear16792:f.store8(0x16792,int8_t(v));break;case T::Write::Clear167d5:f.store8(0x167d5,int8_t(v));break;case T::Write::Clear16790:f.store8(0x16790,int8_t(v));break;case T::Write::Clear16799:f.store8(0x16799,int8_t(v));break;case T::Write::Clear167a3:f.store8(0x167a3,int8_t(v));break;case T::Write::Clear16783:f.store8(0x16783,int8_t(v));break;}};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateScreenActions admission differs");
 switch(stage){case S::ReturnAction:f.ret();break;
case S::SelectSingle:if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadSingleEditPart:f.move(r[0].rh,task.part());break;
case S::SaveSinglePart:f.push(r[0].r);break;
case S::ResetSingleUi:f.call(0x48dc);break;
case S::RestoreSinglePart:f.pop(r[0].r);break;
case S::ReadSingleMode:f.move(r[0].rl,task.mode());break;
case S::BindSingleMode:f.move(r[0].rl,0x0);break;
case S::TestSinglePart:f.move(r[0].rh,r[0].rh);break;
case S::ClearSingleAddress:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::ReadSinglePart:f.move(r[0].rl,task.active());break;
case S::ScaleSinglePart:f.shift(r[0].rl,1,true);break;
case S::ReadSinglePreset:f.move(r[0].r,task.preset());break;
case S::RefreshSingleRouting:f.call(0x6ec6);break;
case S::MarkSingleReload:f.move(r[0].rl,0x1);break;
case S::LoadSingleProgram:f.call(0x62e6);break;
case S::RefreshSingleScreen:f.call(0x5340);break;
case S::ResetSingleVoices:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ClearPlaybackPage:f.move(r[0].rl,0x0);break;
case S::ResetPlaybackContext:f.call(0x601a);break;
case S::ClearPlaybackEdit:f.call(0x48cc);break;
case S::RefreshPlaybackScreen:f.call(0x5340);break;
case S::ResetPlaybackVoices:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ClearEditValue:f.move(r[0].rl,0x0);break;
case S::ReturnEditClear:f.ret();break;
case S::ClearUiValue:f.move(r[0].rl,0x0);break;
case S::ShowClearedUi:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::SelectMulti:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ResetMultiUi:f.call(0x48dc);break;
case S::ReadMultiCurrent:f.move(r[0].rl,task.mask());break;
case S::ReadMultiHeld:f.move(r[0].rl,task.held());break;
case S::ReadMultiMode:f.move(r[0].rh,task.mode());break;
case S::BindMultiMode:f.move(r[0].rl,0x1);break;
case S::RefreshMultiRouting:f.call(0x6ec6);break;
case S::TestMultiMode:f.move(r[0].rh,r[0].rh);break;
case S::LoadMultiProgram:f.call(0x633a);break;
case S::ResolveMultiParts:f.call(0x457e);break;
case S::RefreshResolvedMulti:f.call(0x6ec6);break;
case S::RefreshMultiScreen:f.call(0x5340);break;
case S::ResetMultiVoices:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::SelectStepDown:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindStepDown:f.move(r[6].rl,0xffffffff);break;
case S::SelectStepUp:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindStepUp:f.move(r[6].rl,0x1);break;
case S::ReadStepPage:f.move(r[0].rl,task.page());break;
case S::TestStepName:h.sub8(r[0].rl,0x2);break;
case S::SelectStepName:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::TestStepPanel:h.sub8(r[0].rl,0x3);break;
case S::SelectStepPanel:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadStepSpecial:f.move(r[0].rl,task.special());break;
case S::SelectStepSpecial:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadStepMode:f.move(r[0].rl,task.mode());break;
case S::ReadStepMulti:f.move(r[0].rl,task.multi());break;
case S::EnterMultiStep:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ApplyProgramStep:f.call(0x752c);break;
case S::ReturnProgramStep:f.ret();break;
case S::SelectEditToggle:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ClearEditToggleLatch:f.move(r[0].rl,0x0);break;
case S::ReadEditTogglePage:f.move(r[0].rl,task.page());break;
case S::SelectEditTogglePage:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ResetEditToggleContext:f.call(0x601a);break;
case S::ReadEditToggleValue:f.move(r[6].rh,task.edit());break;
case S::ToggleEditValue:r[6].rh^=0x1;value(h,r[6].rh);break;
case S::ObserveEditToggleGate:f.call(0x590a);break;
case S::SelectEditToggleGate:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReloadEditCalibration:f.call(0x1f84);break;
case S::RefreshEditToggle:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::TestEditEvent:f.move(r[6].rh,r[6].rh);break;
case S::SelectEditEvent:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ClearEditEventLatch:f.move(r[0].rl,0x0);break;
case S::ReadEditEventPage:f.move(r[0].rl,task.page());break;
case S::SelectEditEventPage:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ResetEditEventContext:f.call(0x601a);break;
case S::ReadEditEventScreen:f.move(r[6].rh,task.screen());break;
case S::CompareEditEventScreen:h.sub8(r[6].rl,r[6].rh);break;
case S::BindEditEventOn:f.move(r[0].rl,0x1);break;
case S::ReadEditEventActive:f.move(r[0].rl,task.edit());break;
case S::ToggleEditEventActive:r[0].rl^=0x1;value(h,r[0].rl);break;
case S::SelectEditEventActive:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ApplyEditEvent:f.call(0x58ae);break;
case S::RefreshEditEvent:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::SelectPatchToggleOne:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ClearPatchToggleOne:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::BindPatchToggleOne:f.move(r[0].rl,0xffffffd7);break;
case S::ReadPatchToggleOneBase:f.move(r[1].er,task.patch());break;
case S::LocatePatchToggleOne:r[1].er=h.add32(r[1].er,r[0].er);break;
case S::ReadPatchToggleOne:f.move(r[6].rh,task.value());break;
case S::FlipPatchToggleOne:r[6].rh^=(1u<<0x0);break;
case S::ApplyPatchToggleOne:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::SelectPatchToggleTwo:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ClearPatchToggleTwo:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::BindPatchToggleTwo:f.move(r[0].rl,0xffffffd5);break;
case S::ReadPatchToggleTwoBase:f.move(r[1].er,task.patch());break;
case S::LocatePatchToggleTwo:r[1].er=h.add32(r[1].er,r[0].er);break;
case S::ReadPatchToggleTwo:f.move(r[6].rh,task.value());break;
case S::FlipPatchToggleTwo:r[6].rh^=(1u<<0x0);break;
case S::ApplyPatchToggleTwo:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateScreenActions clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
