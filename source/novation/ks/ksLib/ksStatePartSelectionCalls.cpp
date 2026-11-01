#include "ksStatePartSelectionCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StatePartSelectionPartialTest partialTest=nullptr;using T=StatePartSelectionTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,100> points{{{0x4542,2},{0x4544,4},{0x4548,6},{0x454e,2},{0x4550,6},{0x4556,2},{0x4558,4},{0x455c,6},{0x4562,2},{0x4564,6},{0x456a,6},{0x4570,4},{0x4574,4},{0x4578,4},{0x457c,2},{0x457e,6},{0x4584,6},{0x458a,2},{0x458c,2},{0x458e,6},{0x4594,2},{0x4596,2},{0x4598,2},{0x459a,2},{0x459c,2},{0x459e,2},{0x45a0,2},{0x45a2,2},{0x45a4,2},{0x45a6,2},{0x45a8,6},{0x45ae,6},{0x45b4,2},{0x45b6,6},{0x45bc,6},{0x45c2,2},{0x45c4,6},{0x45ca,6},{0x45d0,4},{0x45d4,2},{0x45d6,2},{0x45d8,2},{0x45da,6},{0x45e0,2},{0x45e2,6},{0x45e8,4},{0x45ec,2},{0x45ee,6},{0x45f4,2},{0x45f6,6},{0x45fc,6},{0x4602,2},{0x4604,2},{0x4606,2},{0x4608,6},{0x460e,6},{0x4614,2},{0x4616,6},{0x461c,6},{0x4622,6},{0x4628,4},{0x462c,6},{0x4632,2},{0x4634,4},{0x4638,2},{0x463a,6},{0x4640,2},{0x4642,2},{0x4644,2},{0x4646,2},{0x4648,2},{0x464a,2},{0x464c,2},{0x464e,2},{0x4650,2},{0x4652,2},{0x4654,6},{0x465a,4},{0x465e,4},{0x4662,2},{0x4664,2},{0x4666,2},{0x4668,2},{0x466a,2},{0x466c,2},{0x466e,2},{0x4670,2},{0x4672,6},{0x4678,2},{0x467a,2},{0x467c,2},{0x467e,2},{0x4680,6},{0x4686,4},{0x468a,4},{0x468e,2},{0x4690,2},{0x4692,2},{0x4694,2},{0x4696,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStatePartSelectionPartialForTests(StatePartSelectionPartialTest value){partialTest=value;}
bool StatePartSelectionCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x4542:kind=T::Kind::Select;break;case 0x4556:kind=T::Kind::Reconcile;break;case 0x457e:kind=T::Kind::Resolve;break;case 0x45b6:kind=T::Kind::Combine;break;case 0x45d4:kind=T::Kind::Held;break;case 0x463a:kind=T::Kind::Changed;break;case 0x466e:kind=T::Kind::ResetParts;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StatePartSelection capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[6].r),uint8_t(h.regs[0].rl),uint8_t(h.regs[0].rh));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();
 const auto store=byteClocks(stage==S::WriteRequestedPart?0x167d0:stage==S::WriteCurrentSelection?0x167b8:stage==S::WriteEditPart?0x16799:stage==S::WriteResolvedRequest?0x167d0:stage==S::WriteResolvedCurrent?0x167b8:stage==S::WriteCombinedRequest?0x167d1:stage==S::WriteCombinedCurrent?0x167b9:stage==S::WriteHeldRelease?0x16797:stage==S::WritePressedHeld?0x16797:stage==S::WritePressedRequest?0x167d1:stage==S::WriteCombinedHeld?0x16797:stage==S::WriteHeldRequest?0x167d1:stage==S::WriteHeldCurrent?0x167b9:stage==S::WriteChangedPart?0x17526:stage==S::WriteCommonPart?0x17526:0,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field)->uint8_t{switch(field){case T::Read::Mode:return uint8_t(h.read8(0x167ba));case T::Read::ActivePart:return uint8_t(h.read8(0x16798));case T::Read::Preferred:return uint8_t(h.read8(0x167b8));case T::Read::Available:return uint8_t(h.read8(0x167d3));case T::Read::Previous:return uint8_t(h.read8(0x167d2));case T::Read::Held:return uint8_t(h.read8(0x16797));case T::Read::HeldRequest:return uint8_t(h.read8(0x167d1));}return 0;};auto write=[&](T::Write field,uint8_t v){switch(field){case T::Write::Request:f.store8(0x167d0,int8_t(v));break;case T::Write::Current:f.store8(0x167b8,int8_t(v));break;case T::Write::EditPart:f.store8(0x16799,int8_t(v));break;case T::Write::HeldRequest:f.store8(0x167d1,int8_t(v));break;case T::Write::HeldCurrent:f.store8(0x167b9,int8_t(v));break;case T::Write::Held:f.store8(0x16797,int8_t(v));break;case T::Write::Part:f.store8(0x17526,int8_t(v));break;}};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StatePartSelection admission differs");
 switch(stage){case S::TestSelection:f.move(r[6].rh,r[6].rh);break;
case S::SelectSelection:h.clockI(2);if(task.complete())f.next=0x4862;break;
case S::BindCurrentSelection:f.move(r[6].rh,r[6].rl);break;
case S::ResolveSelectedPart:f.call(0x457e);break;
case S::RefreshPartRouting:f.call(0x6ec6);break;
case S::ReadSelectionMode:f.move(r[0].rl,task.mode());break;
case S::ReadActivePart:f.move(r[0].rl,task.active());break;
case S::RefreshEditPart:f.call(0x559e);break;
case S::RefreshScreen:f.call(0x5340);break;
case S::PublishSelection:f.call(0x4e8a);break;
case S::ReturnSelection:f.ret();break;
case S::ReadPreferredMask:f.move(r[0].rl,task.selection());break;
case S::ReadAvailableMask:f.move(r[0].rh,task.available());break;
case S::MaskPreferred:r[0].rl&=r[0].rh;value(h,r[0].rl);break;
case S::ReadPreviousMask:f.move(r[0].rl,task.selection());break;
case S::MaskPrevious:r[0].rl&=r[0].rh;value(h,r[0].rl);break;
case S::BindFirstMask:f.move(r[0].rl,0x1);break;
case S::BindProbeCount:f.move(r[1].rh,0x3);break;
case S::BindProbeMask:f.move(r[1].rl,r[0].rh);break;
case S::TestProbeMask:r[1].rl&=r[0].rl;value(h,r[1].rl);break;
case S::ShiftProbeMask:f.shift(r[0].rl,1,true);break;
case S::DecrementProbeCount:f.inc(r[1].rh,-1);break;
case S::ReturnResolved:f.ret();break;
case S::ReadCombinedMask:f.move(r[0].rl,task.combined());break;
case S::ReadCombinedAvailable:f.move(r[0].rh,task.available());break;
case S::CombineMasks:r[0].rh|=r[0].rl;value(h,r[0].rh);break;
case S::RefreshCombined:h.clockI(2);f.next=0x6ec6;break;
case S::TestHeldEvent:f.move(r[6].rh,r[6].rh);break;
case S::InvertReleasedMask:r[6].rl^=0xf;value(h,r[6].rl);break;
case S::ReadHeldMask:f.move(r[6].rh,task.held());break;
case S::MaskHeldRelease:r[6].rh&=r[6].rl;value(h,r[6].rh);break;
case S::SelectHeldRelease:h.clockI(2);break;
case S::ReturnHeld:f.ret();break;
case S::ReadPressedHeld:f.move(r[6].rh,task.held());break;
case S::BindPressedCurrent:f.move(r[6].rh,r[6].rl);break;
case S::CombineHeld:r[6].rh|=r[6].rl;value(h,r[6].rh);break;
case S::ReadHeldRequest:f.move(r[6].rh,task.combined());break;
case S::CombineHeldRequest:r[6].rh|=r[6].rl;value(h,r[6].rh);break;
case S::ReadRoutingBefore:f.move(r[0].rl,task.before());break;
case S::RefreshHeldRouting:f.call(0x6ec6);break;
case S::ReadRoutingAfter:f.move(r[0].rh,task.after());break;
case S::ReinitializeChanged:f.call(0x463a);break;
case S::RefreshHeldScreen:f.call(0x5340);break;
case S::ReturnHeldSelection:f.ret();break;
case S::ReadChangedMode:f.move(r[1].rl,task.mode());break;
case S::DifferenceChanged:r[0].rh^=r[0].rl;value(h,r[0].rh);break;
case S::MaskRemoved:r[0].rh&=r[0].rl;value(h,r[0].rh);break;
case S::BindChangedPart:f.move(r[1].rl,0x1);break;
case S::BindChangedCount:f.move(r[1].rh,0x4);break;
case S::ShiftRemoved:f.shift(r[0].rh,1,false);break;
case S::SaveChangedMasks:f.push(r[0].r);break;
case S::SaveChangedPart:f.push(r[1].r);break;
case S::ResetChangedVoices:f.call(0x9374);break;
case S::ResetChangedMidi:f.call(0x2bc8);break;
case S::RestoreChangedPart:f.pop(r[1].r);break;
case S::RestoreChangedMasks:f.pop(r[0].r);break;
case S::NextChangedPart:f.inc(r[1].rl,1);break;
case S::DecrementChangedCount:f.inc(r[1].rh,-1);break;
case S::ReturnChanged:f.ret();break;
case S::BindCommonPart:f.move(r[1].rl,0x0);break;
case S::BindCommonCount:f.move(r[1].rh,0x1);break;
case S::ReadCommonMode:f.move(r[0].rl,task.mode());break;
case S::BindSinglePart:f.move(r[1].rl,0x1);break;
case S::BindSingleCount:f.move(r[1].rh,0x4);break;
case S::SaveCommonPart:f.push(r[1].r);break;
case S::ResetCommonMidi:f.call(0x3074);break;
case S::ResetCommonVoices:f.call(0x9374);break;
case S::RestoreCommonPart:f.pop(r[1].r);break;
case S::NextCommonPart:f.inc(r[1].rl,1);break;
case S::DecrementCommonCount:f.inc(r[1].rh,-1);break;
case S::ReturnCommon:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StatePartSelection clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
