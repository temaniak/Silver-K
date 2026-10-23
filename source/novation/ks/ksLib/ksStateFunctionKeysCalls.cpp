#include "ksStateFunctionKeysCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateFunctionKeysPartialTest partialTest=nullptr;using T=StateFunctionKeysTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,72> points{{{0x4c58,2},{0x4c5a,4},{0x4c5e,6},{0x4c64,2},{0x4c66,4},{0x4c6a,6},{0x4c70,2},{0x4c72,2},{0x4c74,4},{0x4c78,2},{0x4c7a,2},{0x4c7c,4},{0x4c80,2},{0x4c82,2},{0x4c84,4},{0x4c88,2},{0x4c8a,2},{0x4c8c,4},{0x4c90,4},{0x4c94,2},{0x4c96,4},{0x4c9a,2},{0x4c9c,2},{0x4c9e,2},{0x4ca0,2},{0x4ca2,2},{0x4ca4,2},{0x4ca6,2},{0x4ca8,2},{0x4caa,2},{0x4cac,2},{0x4cae,2},{0x4cb0,2},{0x4cb2,4},{0x4cb6,8},{0x4cbe,8},{0x4cc6,4},{0x4cca,2},{0x4ccc,2},{0x4cce,2},{0x4cd0,2},{0x4cd2,2},{0x4cd4,2},{0x4cd6,6},{0x4cdc,4},{0x4ce0,2},{0x4ce2,2},{0x4ce4,2},{0x4ce6,2},{0x4ce8,2},{0x4cea,2},{0x4cec,4},{0x4cf0,4},{0x4cf4,4},{0x4cf8,4},{0x4cfc,2},{0x4cfe,4},{0x4d02,2},{0x4d04,2},{0x4d06,2},{0x4d08,6},{0x4d0e,6},{0x4d14,2},{0x4d16,2},{0x4d18,2},{0x4d1a,2},{0x4d1c,2},{0x4d1e,4},{0x4d22,2},{0x4d24,2},{0x4d26,4},{0x4d2a,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::Name:return 0x75a4;case T::Exit::Semitone:return 0x4c1e;case T::Exit::Special:return 0xb17c;case T::Exit::Refresh:return 0x5340;case T::Exit::Screen:return 0x58ae;default:throw std::runtime_error("Missing function key exit");}}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateFunctionKeysPartialForTests(StateFunctionKeysPartialTest value){partialTest=value;}
bool StateFunctionKeysCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x4c58:kind=T::Kind::Event;break;case 0x4cf4:kind=T::Kind::Refresh;break;case 0x4d04:kind=T::Kind::Clear;break;case 0x4d16:kind=T::Kind::Release;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateFunctionKeys capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[6].r),uint32_t(h.regs[2].er));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(!(h.ccr&4));const auto stage=task.stage();
 const auto store=byteClocks(stage==S::WriteFunctionClear||stage==S::SetFunctionBit||stage==S::SetFunctionZero||stage==S::ClearReleaseBit||stage==S::ClearReleaseZero?task.record():stage==S::WriteFunctionEdit?0x16784:stage==S::WriteFunctionPage?0x16790:0x1679e,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{switch(field){case T::Read::UiPage:return uint8_t(h.read8(0x16790));case T::Read::Other:return uint8_t(h.read8(0x1679f));case T::Read::Patch:return uint32_t(h.read32(0x167bc));case T::Read::Flags:case T::Read::Byte:return uint8_t(h.read8(ref));}return 0;};auto write=[&](T::Write field,uint32_t ref,uint8_t value){if(field==T::Write::RawFlags)h.write8(value,ref);else f.store8(field==T::Write::Flags?ref:field==T::Write::Edit?0x16784:field==T::Write::UiPage?0x16790:0x1679e,int8_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateFunctionKeys admission differs");
 switch(stage){case S::TestFunctionEvent:f.move(r[6].rh,r[6].rh);break;
case S::SelectFunctionEvent:h.clockI(2);break;
case S::ReadFunctionPage:f.move(r[0].rl,task.page());break;
case S::TestFunctionPage:h.sub8(r[0].rl,0x2);break;
case S::SelectFunctionPage:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadFunctionOther:f.move(r[0].rl,task.other());break;
case S::TestFunctionOther:h.sub8(r[0].rl,0x2);break;
case S::SelectFunctionOtherName:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReturnFunctionOther:f.ret();break;
case S::TestFunctionKey:f.move(r[6].rl,r[6].rl);break;
case S::SelectFunctionKey:h.clockI(2);break;
case S::TestFunctionBit:h.sub8(r[6].rl,0x5);break;
case S::TestFunctionZero:h.sub8(r[6].rl,0x9);break;
case S::ObserveFunctionEdit:f.call(0x4efe);break;
case S::ObserveFunctionCalibration:f.call(0x590a);break;
case S::SaveCalibrationKey:f.move(r[6].rh,r[6].rl);break;
case S::ReadCalibrationChord:f.move(r[0].rl,task.flags());break;
case S::MaskCalibrationChord:r[0].rl&=0x31;value(h,r[0].rl);break;
case S::BindCalibrationFirst:f.move(r[6].rl,0x4);break;
case S::TestCalibrationFirst:h.sub8(r[0].rl,0x11);break;
case S::BindCalibrationSecond:f.move(r[6].rl,0x5);break;
case S::TestCalibrationSecond:h.sub8(r[0].rl,0x21);break;
case S::RestoreCalibrationKey:f.move(r[6].rl,r[6].rh);break;
case S::SelectCalibrationPanel:f.call(0x1f92);break;
case S::ReadCalibrationPatch:f.move(r[6].er,task.patch());break;
case S::ReadCalibrationTranspose:f.move(r[6].rh,task.transpose());break;
case S::ConvertCalibrationTranspose:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadGeneralChord:f.move(r[0].rl,task.flags());break;
case S::MaskGeneralChord:r[0].rl&=0x6;value(h,r[0].rl);break;
case S::TestGeneralChord:h.sub8(r[0].rl,0x6);break;
case S::ClearGeneralChord:f.call(0x4d04);break;
case S::BindGeneralSpecial:f.move(r[0].rl,0x0);break;
case S::EnterGeneralSpecial:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReturnGeneralSpecial:f.ret();break;
case S::ReadDumpChord:f.move(r[0].rl,task.flags());break;
case S::MaskDumpChord:r[0].rl&=0x38;value(h,r[0].rl);break;
case S::TestDumpChord:h.sub8(r[0].rl,0x38);break;
case S::ClearDumpChord:f.call(0x4d04);break;
case S::DumpFirmware:f.call(0x18f8);break;
case S::RefreshDump:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ObserveFunctionRefresh:f.call(0x4efe);break;
case S::SelectFunctionRefresh:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindFunctionReset:f.move(r[6].rh,0x0);break;
case S::ApplyFunctionReset:f.call(0x5db2);break;
case S::ReturnFunction:f.ret();break;
case S::BindFunctionClear:f.move(r[0].rl,0x0);break;
case S::ReturnFunctionClear:f.ret();break;
case S::TestReleaseKey:f.move(r[6].rl,r[6].rl);break;
case S::TestReleaseBit:h.sub8(r[6].rl,0x5);break;
case S::TestReleaseZero:h.sub8(r[6].rl,0x9);break;
case S::ReturnRelease:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateFunctionKeys clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
