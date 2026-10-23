#include "ksStateEditWorkflowCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateEditWorkflowPartialTest partialTest=nullptr;using T=StateEditWorkflowTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,153> points{{{0x4d2c,6},{0x4d32,4},{0x4d36,2},{0x4d38,6},{0x4d3e,4},{0x4d42,2},{0x4d44,2},{0x4d46,6},{0x4d4c,2},{0x4d4e,6},{0x4d54,2},{0x4d56,2},{0x4d58,4},{0x4d5c,2},{0x4d5e,6},{0x4d64,2},{0x4d66,4},{0x4d6a,2},{0x4d6c,6},{0x4d72,2},{0x4d74,2},{0x4d76,6},{0x4d7c,6},{0x4d82,4},{0x4d86,4},{0x4d8a,4},{0x4d8e,2},{0x4d90,6},{0x4d96,2},{0x4d98,2},{0x4d9a,6},{0x4da0,2},{0x4da2,2},{0x4da4,2},{0x4da6,2},{0x4da8,2},{0x4daa,2},{0x4dac,6},{0x4db2,4},{0x4db6,4},{0x4dba,4},{0x4dbe,2},{0x4dc0,4},{0x4dc4,4},{0x4dc8,4},{0x4dcc,2},{0x4dce,2},{0x4dd0,2},{0x4dd2,4},{0x4dd6,2},{0x4dd8,2},{0x4dda,2},{0x4ddc,4},{0x4de0,2},{0x4de2,4},{0x4de6,4},{0x4dea,2},{0x4dec,2},{0x4dee,4},{0x4df2,2},{0x4df4,2},{0x4df6,2},{0x4df8,2},{0x4dfa,2},{0x4dfc,2},{0x4dfe,4},{0x4e02,4},{0x4e06,2},{0x4e08,2},{0x4e0a,6},{0x4e10,2},{0x4e12,4},{0x4e16,2},{0x4e18,2},{0x4e1a,2},{0x4e1c,6},{0x4e22,2},{0x4e24,2},{0x4e26,4},{0x4e2a,2},{0x4e2c,2},{0x4e2e,2},{0x4e30,2},{0x4e32,6},{0x4e38,2},{0x4e3a,2},{0x4e3c,2},{0x4e3e,2},{0x4e40,2},{0x4e42,6},{0x4e48,2},{0x4e4a,2},{0x4e4c,4},{0x4e50,4},{0x4e54,6},{0x4e5a,4},{0x4e5e,2},{0x4e60,2},{0x4e62,6},{0x4e68,2},{0x4e6a,2},{0x4e6c,6},{0x4e72,2},{0x4e74,2},{0x4e76,2},{0x4e78,2},{0x4e7a,2},{0x4e7c,2},{0x4e7e,2},{0x4e80,2},{0x4e82,2},{0x4e84,2},{0x4e86,2},{0x4e88,2},{0x4e8a,6},{0x4e90,8},{0x4e98,2},{0x4e9a,4},{0x4e9e,4},{0x4ea2,2},{0x4ea4,6},{0x4eaa,2},{0x4eac,2},{0x4eae,2},{0x4eb0,2},{0x4eb2,2},{0x4eb4,2},{0x4eb6,2},{0x4eb8,2},{0x4eba,8},{0x4ec2,8},{0x4eca,2},{0x4ecc,2},{0x4ece,2},{0x4ed0,2},{0x4ed2,2},{0x4ed4,6},{0x4eda,2},{0x4edc,2},{0x4ede,2},{0x4ee0,2},{0x4ee2,2},{0x4ee4,6},{0x4eea,4},{0x4eee,2},{0x4ef0,2},{0x4ef2,8},{0x4efa,2},{0x4efc,2},{0x4efe,6},{0x4f04,2},{0x4f06,4},{0x4f0a,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::Name:return 0x75fa;case T::Exit::Calibration:return 0x1fd8;case T::Exit::SpecialOne:return 0x2ada;case T::Exit::SpecialTwo:return 0x2a8e;case T::Exit::SpecialThree:return 0x2aa2;case T::Exit::SpecialFour:return 0x2ac6;case T::Exit::CloseNote:return 0x5efc;default:throw std::runtime_error("Missing edit workflow exit");}}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateEditWorkflowPartialForTests(StateEditWorkflowPartialTest value){partialTest=value;}
bool StateEditWorkflowCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x4d2c:kind=T::Kind::Restore;break;case 0x4d46:kind=T::Kind::Select;break;case 0x4d90:kind=T::Kind::SelectNote;break;case 0x4dc0:kind=T::Kind::PanelEvent;break;case 0x4df2:kind=T::Kind::Velocity;break;case 0x4e0a:kind=T::Kind::NoteOn;break;case 0x4e2c:kind=T::Kind::NoteOff;break;case 0x4e60:kind=T::Kind::ResolveNote;break;case 0x4e7c:kind=T::Kind::DirectEdit;break;case 0x4e8a:kind=T::Kind::Publish;break;case 0x4ea4:kind=T::Kind::AutomaticEdit;break;case 0x4eaa:kind=T::Kind::AutomaticEvent;break;case 0x4eb0:kind=T::Kind::ManualEdit;break;case 0x4efe:kind=T::Kind::EditActive;break;case 0x4f06:kind=T::Kind::ApplyVelocity;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateEditWorkflow capacity exceeded");slot->emplace(*kind,h.getSP(),uint32_t(h.regs[6].er));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(!(h.ccr&4),uint32_t(r[6].er));const auto stage=task.stage();
 const auto store=byteClocks(stage==S::WriteRestoreScreen?0x16785:stage==S::WriteRestorePage?0x16787:stage==S::WriteNoteOffNext?0x16783:stage==S::WriteSelectArmed?0x16795:stage==S::WriteNoteSelect?0x167ad:stage==S::WriteEditContext?0x167a6:stage==S::WriteSelectRouting?0x167ac:0,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{switch(field){case T::Read::PreviousScreen:return uint8_t(h.read8(0x16786));case T::Read::PreviousPage:return uint8_t(h.read8(0x16788));case T::Read::Other:return uint8_t(h.read8(0x1679f));case T::Read::UiPage:return uint8_t(h.read8(0x16790));case T::Read::Group:return uint8_t(h.read8(0x167ac));case T::Read::Routing:return uint16_t(h.read16(0x167a4));case T::Read::Armed:return uint8_t(h.read8(0x16795));case T::Read::Fallback:return uint8_t(h.read8(0x16783));case T::Read::Key:return uint8_t(h.read8(0x167ad));case T::Read::Publish:return uint8_t(h.read8(0x169f9));case T::Read::Automatic:return uint8_t(h.read8(0x16793));case T::Read::Patch:return uint32_t(h.read32(0x167bc));case T::Read::Edit:return uint8_t(h.read8(0x16784));case T::Read::Byte:return uint8_t(h.read8(ref));case T::Read::Handler:call.handler=uint32_t(h.read32(ref));return 0;}return 0;};auto write=[&](T::Write field,uint32_t v){switch(field){case T::Write::Screen:f.store8(0x16785,int8_t(v));break;case T::Write::Page:f.store8(0x16787,int8_t(v));break;case T::Write::Armed:f.store8(0x16795,int8_t(v));break;case T::Write::Routing:f.store16(0x167ac,int16_t(v));break;case T::Write::Key:f.store8(0x167ad,int8_t(v));break;case T::Write::Fallback:f.store8(0x16783,int8_t(v));break;case T::Write::Context:f.store32(0x167a6,int32_t(v));break;}};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateEditWorkflow admission differs");
 switch(stage){case S::BindRestoreScreen:f.move(r[0].er,0x16785);break;
case S::ReadRestoreScreen:f.move(r[1].rl,task.screen());break;
case S::BindRestorePage:f.move(r[0].er,0x16787);break;
case S::ReadRestorePage:f.move(r[1].rl,task.page());break;
case S::ReturnRestore:f.ret();break;
case S::ReadSelectOther:f.move(r[0].rl,task.other());break;
case S::TestSelectOther:h.sub8(r[0].rl,0x2);break;
case S::SelectOtherName:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadSelectPage:f.move(r[0].rl,task.page());break;
case S::TestSelectPage:h.sub8(r[0].rl,0x2);break;
case S::SelectPageName:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadSelectGroup:f.move(r[0].rl,task.group());break;
case S::TestSelectGroup:h.sub8(r[0].rl,0x4);break;
case S::ReadSelectRouting:f.move(r[0].r,task.routing());break;
case S::RefreshSelectRouting:f.call(0x6ec6);break;
case S::LoadSelectProgram:f.call(0x62e6);break;
case S::ResetSelectPlayback:f.call(0x48b6);break;
case S::ReturnSelect:f.ret();break;
case S::ReadNoteSelectPage:f.move(r[0].rl,task.page());break;
case S::TestNoteSelectPage:h.sub8(r[0].rl,0x2);break;
case S::ReadNoteSelectArmed:f.move(r[0].rl,task.armed());break;
case S::TestNoteSelectLow:h.sub8(r[6].rl,0x24);break;
case S::TestNoteSelectHigh:h.sub8(r[6].rl,0x54);break;
case S::SubtractNoteSelectBase:r[6].rl=h.add8(r[6].rl,0xffffffdc);break;
case S::RefreshNoteSelect:f.call(0x6ec6);break;
case S::LoadNoteSelect:f.call(0x6382);break;
case S::ResetNoteSelect:f.call(0x48be);break;
case S::ReturnNoteSelect:f.ret();break;
case S::ObservePanelCalibration:f.call(0x590a);break;
case S::SelectPanelCalibration:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadPanelEncoder:f.call(0x15da);break;
case S::TestPanelEncoder:f.move(r[6].e,r[6].e);break;
case S::TestPanelSpecialOne:h.sub8(r[6].rl,0x20);break;
case S::SelectPanelSpecialOne:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::TestPanelSpecialTwo:h.sub8(r[6].rl,0x21);break;
case S::TestPanelSpecialThree:h.sub8(r[6].rl,0x22);break;
case S::SelectPanelSpecialThree:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::TestPanelSpecialFour:h.sub8(r[6].rl,0x23);break;
case S::SelectPanelSpecialFour:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ApplyPanelParameter:f.call(0x4eb0);break;
case S::ReturnPanelEvent:f.ret();break;
case S::ScalePanelSpecialTwo:f.shift(r[6].rh,1,false);break;
case S::EnterPanelSpecialTwo:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::SaveVelocityEvent:f.push(r[6].r);break;
case S::BindVelocityValue:f.move(r[1].rl,r[6].rh);break;
case S::SquareVelocityValue:r[1].r=uint8_t(r[1].r)*uint32_t(uint8_t(r[1].rl));h.clockI(12);break;
case S::DoubleVelocityValue:f.shift(r[1].r,1,true);break;
case S::BindVelocityCurve:f.move(r[1].e,r[1].r);break;
case S::ClearVelocityCurveHigh:r[1].rh=h.sub8(r[1].rh,r[1].rh);break;
case S::BindVelocityScale:f.move(r[4].r,0x18);break;
case S::ApplyVelocityScale:f.call(0x24c6);break;
case S::RestoreVelocityEvent:f.pop(r[6].r);break;
case S::ReturnVelocity:f.ret();break;
case S::ReadNoteOnPage:f.move(r[0].rl,task.page());break;
case S::CloseNoteOnPage:f.call(0x5edc);break;
case S::ResolveNoteOn:f.call(0x4e60);break;
case S::TestNoteOn:f.move(r[6].rl,r[6].rl);break;
case S::ReadNoteOnFallback:f.move(r[6].rl,task.key());break;
case S::AddNoteOnBase:r[6].rl=h.add8(r[6].rl,0x24);break;
case S::BindNoteOnVelocity:f.move(r[6].rh,0x7f);break;
case S::StartNoteOn:f.call(0x2b6a);break;
case S::ReturnNoteOn:f.ret();break;
case S::ResolveNoteOff:f.call(0x4e60);break;
case S::TestNoteOff:f.move(r[6].rl,r[6].rl);break;
case S::ReadNoteOffFallback:f.move(r[6].rl,task.key());break;
case S::BindNoteOffNext:f.move(r[0].rl,r[6].rl);break;
case S::AddNoteOffNext:r[0].rl=h.add8(r[0].rl,0x6);break;
case S::TestNoteOffNext:h.sub8(r[0].rl,0x30);break;
case S::ClearNoteOffNext:f.move(r[0].rl,0x0);break;
case S::AddNoteOffBase:r[6].rl=h.add8(r[6].rl,0x24);break;
case S::BindNoteOffVelocity:f.move(r[6].rh,0x40);break;
case S::StopNoteOff:f.call(0x2b60);break;
case S::ResetNoteOffMidi:f.call(0x3074);break;
case S::ReadNoteOffPage:f.move(r[0].rl,task.page());break;
case S::SelectNoteOffPage:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReturnNoteOff:f.ret();break;
case S::ClearResolvedNote:f.move(r[6].rl,0x0);break;
case S::ReadResolvedGroup:f.move(r[0].rl,task.group());break;
case S::TestResolvedGroup:h.sub8(r[0].rl,0x4);break;
case S::ReadResolvedKey:f.move(r[0].rl,task.storedKey());break;
case S::TestResolvedKey:h.sub8(r[0].rl,0x30);break;
case S::BindResolvedNote:f.move(r[6].rl,r[0].rl);break;
case S::AddResolvedBase:r[6].rl=h.add8(r[6].rl,0x24);break;
case S::ReturnResolvedNote:f.ret();break;
case S::SaveDirectEvent:f.push(r[6].r);break;
case S::TestDirectEvent:f.move(r[6].rh,r[6].rh);break;
case S::AddDirectIndex:r[6].rl=h.add8(r[6].rl,0x26);break;
case S::ApplyDirectEdit:f.call(0x4eb0);break;
case S::RestoreDirectEvent:f.pop(r[6].r);break;
case S::ReturnDirect:f.ret();break;
case S::BindPublishBase:f.move(r[0].er,0x168fc);break;
case S::ReadPublishEnabled:f.move(r[0].rl,task.publish());break;
case S::BindPublishIndex:f.move(r[1].e,0xd1);break;
case S::PublishParameter:f.call(0x6ace);break;
case S::ReturnPublish:f.ret();break;
case S::ReadAutomaticIndex:f.move(r[6].rl,task.key());break;
case S::SaveAutomaticEvent:f.push(r[6].r);break;
case S::BindAutomaticFlag:f.move(r[5].rh,0x1);break;
case S::SaveManualEvent:f.push(r[6].r);break;
case S::BindManualFlag:f.move(r[5].rh,0x0);break;
case S::SaveEditIndex:f.move(r[5].rl,r[6].rl);break;
case S::TestDynamicIndex:h.sub8(r[6].rl,0x1f);break;
case S::ReadDynamicPatch:f.move(r[1].er,task.patch());break;
case S::ReadDynamicVariant:f.move(r[1].rl,task.variant());break;
case S::AddDynamicIndex:r[6].rl=h.add8(r[6].rl,r[1].rl);break;
case S::BindDescriptorIndex:f.move(r[0].rl,r[6].rl);break;
case S::BindDescriptorStride:f.move(r[0].rh,0x8);break;
case S::ScaleDescriptorIndex:r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;
case S::NormalizeDescriptorIndex:r[0].er&=0xffff;value(h,r[0].er);break;
case S::LocateDescriptor:r[0].er=h.add32(r[0].er,0xfd66);break;
case S::ReadDescriptorCategory:f.move(r[1].rl,task.category());break;
case S::DoubleDescriptorCategory:f.shift(r[1].rl,1,true);break;
case S::QuadrupleDescriptorCategory:f.shift(r[1].rl,1,true);break;
case S::NormalizeCategoryWord:r[1].r&=0xff;value(h,r[1].r);break;
case S::NormalizeCategoryLong:r[1].er&=0xffff;value(h,r[1].er);break;
case S::LocateHandlerTable:r[1].er=h.add32(r[1].er,0x1013e);break;
case S::ReadEditHandler:f.move(r[1].er,int32_t(call.handler));break;
case S::DispatchEditHandler:f.call(call.handler&0xffffff);break;
case S::ClearEditContext:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::RestoreEditEvent:f.pop(r[6].r);break;
case S::ReturnEdit:f.ret();break;
case S::ReadEditActive:f.move(r[1].rl,task.edit());break;
case S::ReturnEditActive:f.ret();break;
case S::ApplyVelocity:f.call(0x4df2);break;
case S::ReturnVelocityApply:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateEditWorkflow clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
