#include "ksStatePanelModesCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StatePanelModesPartialTest partialTest=nullptr;using T=StatePanelModesTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,145> points{{{0x4a66,2},{0x4a68,4},{0x4a6c,2},{0x4a6e,4},{0x4a72,4},{0x4a76,4},{0x4a7a,4},{0x4a7e,2},{0x4a80,4},{0x4a84,6},{0x4a8a,4},{0x4a8e,2},{0x4a90,2},{0x4a92,8},{0x4a9a,2},{0x4a9c,2},{0x4a9e,2},{0x4aa0,2},{0x4aa2,2},{0x4aa4,4},{0x4aa8,6},{0x4aae,4},{0x4ab2,4},{0x4ab6,2},{0x4ab8,4},{0x4abc,4},{0x4ac0,2},{0x4ac2,4},{0x4ac6,6},{0x4acc,4},{0x4ad0,2},{0x4ad2,2},{0x4ad4,8},{0x4adc,2},{0x4ade,2},{0x4ae0,2},{0x4ae2,2},{0x4ae4,2},{0x4ae6,2},{0x4ae8,4},{0x4aec,6},{0x4af2,4},{0x4af6,4},{0x4afa,2},{0x4afc,2},{0x4afe,2},{0x4b00,6},{0x4b06,2},{0x4b08,6},{0x4b0e,2},{0x4b10,2},{0x4b12,4},{0x4b16,6},{0x4b1c,2},{0x4b1e,6},{0x4b24,4},{0x4b28,4},{0x4b2c,2},{0x4b2e,2},{0x4b30,4},{0x4b34,2},{0x4b36,4},{0x4b3a,2},{0x4b3c,2},{0x4b3e,4},{0x4b42,2},{0x4b44,2},{0x4b46,2},{0x4b48,2},{0x4b4a,2},{0x4b4c,2},{0x4b4e,2},{0x4b50,6},{0x4b56,2},{0x4b58,2},{0x4b5a,4},{0x4b5e,4},{0x4b62,2},{0x4b64,2},{0x4b66,4},{0x4b6a,2},{0x4b6c,4},{0x4b70,4},{0x4b74,2},{0x4b76,4},{0x4b7a,2},{0x4b7c,2},{0x4b7e,6},{0x4b84,2},{0x4b86,2},{0x4b88,2},{0x4b8a,2},{0x4b8c,2},{0x4b8e,2},{0x4b90,2},{0x4b92,2},{0x4b94,6},{0x4b9a,8},{0x4ba2,4},{0x4ba6,2},{0x4ba8,8},{0x4bb0,2},{0x4bb2,6},{0x4bb8,4},{0x4bbc,6},{0x4bc2,8},{0x4bca,4},{0x4bce,2},{0x4bd0,6},{0x4bd6,6},{0x4bdc,2},{0x4bde,2},{0x4be0,6},{0x4be6,2},{0x4be8,2},{0x4bea,6},{0x4bf0,2},{0x4bf2,4},{0x4bf6,4},{0x4bfa,4},{0x4bfe,2},{0x4c00,6},{0x4c06,2},{0x4c08,2},{0x4c0a,2},{0x4c0c,2},{0x4c0e,2},{0x4c10,6},{0x4c16,6},{0x4c1c,2},{0x4c1e,6},{0x4c24,2},{0x4c26,2},{0x4c28,2},{0x4c2a,2},{0x4c2c,4},{0x4c30,6},{0x4c36,6},{0x4c3c,6},{0x4c42,2},{0x4c44,6},{0x4c4a,6},{0x4c50,2},{0x4c52,4},{0x4c56,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::ReleaseNavigation:return 0x5eb0;case T::Exit::Refresh:return 0x5340;case T::Exit::ApplyScreen:return 0x58ae;case T::Exit::Release:return 0x4862;case T::Exit::PublishToggle:return 0x7354;case T::Exit::NoteOff:return 0x4e2c;case T::Exit::NoteOn:return 0x4e0a;case T::Exit::ResetVoices:return 0x4bd0;default:throw std::runtime_error("Missing panel mode exit");}}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStatePanelModesPartialForTests(StatePanelModesPartialTest value){partialTest=value;}
bool StatePanelModesCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x4a66:kind=T::Kind::ModeKey;break;case 0x4a76:kind=T::Kind::OctaveDown;break;case 0x4a80:kind=T::Kind::RepeatDown;break;case 0x4ab8:kind=T::Kind::OctaveUp;break;case 0x4ac2:kind=T::Kind::RepeatUp;break;case 0x4afc:kind=T::Kind::Reference;break;case 0x4b08:kind=T::Kind::TransposeDown;break;case 0x4b50:kind=T::Kind::TransposeUp;break;case 0x4b94:kind=T::Kind::ApplyTranspose;break;case 0x4bd0:kind=T::Kind::ResetVoices;break;case 0x4c1e:kind=T::Kind::Semitone;break;case 0x4c44:kind=T::Kind::CheckSemitone;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StatePanelModes capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[6].r),!(h.ccr&4),uint32_t(h.regs[2].er));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(!(h.ccr&4));const auto stage=task.stage();
 const auto store=byteClocks(stage==S::ClearTransposeDownContact||stage==S::SetTransposeDownContact||stage==S::ClearTransposeUpContact||stage==S::SetTransposeUpContact?task.record():stage==S::WriteResetMode?0x167ba:stage==S::WriteResetMulti?0x167ba:stage==S::ResetTransposeContext?0x167a6:stage==S::WriteResetPart?0x17526:stage==S::WriteRestoredPart?0x17526:stage==S::WriteRestoredMode?0x167ba:stage==S::WriteOldSemitone?0x1678e:stage==S::WriteTransposeContext?0x167a6:stage==S::WriteNewSemitone?0x1678d:stage==S::WriteTransposeToggle?0x167a3:0,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{switch(field){case T::Read::Patch:return uint32_t(h.read32(0x167bc));case T::Read::Screen:return uint8_t(h.read8(0x16785));case T::Read::Hardware:return uint8_t(h.read8(0xffec14));case T::Read::Toggle:return uint8_t(h.read8(0x167a3));case T::Read::Transpose:return uint8_t(h.read8(0x169df));case T::Read::Type:return uint8_t(h.read8(0x158f2));case T::Read::Mode:return uint8_t(h.read8(0x167ba));case T::Read::Part:return uint8_t(h.read8(0x17526));case T::Read::Semitone:return uint8_t(h.read8(0x1678d));case T::Read::PreviousSemitone:return uint8_t(h.read8(0x1678e));case T::Read::Flags:case T::Read::Byte:return uint8_t(h.read8(ref));}return 0;};auto write=[&](T::Write field,uint32_t ref,uint32_t value){switch(field){case T::Write::Flags:h.write8(uint8_t(value),ref);break;case T::Write::Toggle:f.store8(0x167a3,int8_t(value));break;case T::Write::Context:f.store32(0x167a6,int32_t(value));break;case T::Write::Mode:f.store8(0x167ba,int8_t(value));break;case T::Write::Part:f.store8(0x17526,int8_t(value));break;case T::Write::Semitone:f.store8(0x1678d,int8_t(value));break;case T::Write::PreviousSemitone:f.store8(0x1678e,int8_t(value));break;}};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StatePanelModes admission differs");
 switch(stage){case S::ReleaseModeKey:f.call(0x5efc);break;
case S::PressModeKey:f.call(0x5edc);break;
case S::RefreshModeKey:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::SelectOctaveDown:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindOctaveDownDelay:f.move(r[6].r,0x190);break;
case S::BindOctaveDownRepeat:f.move(r[6].r,0xc8);break;
case S::BindOctaveDownCallback:f.move(r[5].er,0x4a80);break;
case S::ScheduleOctaveDown:f.call(0x4134);break;
case S::ClearOctaveDownIndex:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::BindOctaveDownIndex:f.move(r[0].rl,0xffffffe2);break;
case S::ReadOctaveDownBase:f.move(r[1].er,task.patch());break;
case S::LocateOctaveDown:r[1].er=h.add32(r[1].er,r[0].er);break;
case S::ReadOctaveDown:f.move(r[6].rh,task.value());break;
case S::DecrementOctaveDown:f.inc(r[6].rh,-1);break;
case S::ClampOctaveDown:f.move(r[6].rh,0x6);break;
case S::ApplyOctaveDown:f.call(0x6ae6);break;
case S::ReadOctaveDownScreen:f.move(r[6].rl,task.screen());break;
case S::ObserveOctaveDownEdit:f.call(0x4efe);break;
case S::SelectOctaveDownEdit:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReturnOctaveDown:f.ret();break;
case S::SelectOctaveUp:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindOctaveUpDelay:f.move(r[6].r,0x190);break;
case S::BindOctaveUpRepeat:f.move(r[6].r,0xc8);break;
case S::BindOctaveUpCallback:f.move(r[5].er,0x4ac2);break;
case S::ScheduleOctaveUp:f.call(0x4134);break;
case S::ClearOctaveUpIndex:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::BindOctaveUpIndex:f.move(r[0].rl,0xffffffe2);break;
case S::ReadOctaveUpBase:f.move(r[1].er,task.patch());break;
case S::LocateOctaveUp:r[1].er=h.add32(r[1].er,r[0].er);break;
case S::ReadOctaveUp:f.move(r[6].rh,task.value());break;
case S::IncrementOctaveUp:f.inc(r[6].rh,1);break;
case S::TestOctaveUpLimit:h.sub8(r[6].rh,0x6);break;
case S::ClampOctaveUp:f.move(r[6].rh,0x0);break;
case S::ApplyOctaveUp:f.call(0x6ae6);break;
case S::ReadOctaveUpScreen:f.move(r[6].rl,task.screen());break;
case S::ObserveOctaveUpEdit:f.call(0x4efe);break;
case S::SelectOctaveUpEdit:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReturnOctaveUp:f.ret();break;
case S::ClearTransposeReference:r[1].er=h.sub32(r[1].er,r[1].er);break;
case S::BindTransposeIndex:f.move(r[1].rl,0xffffffe3);break;
case S::LocateTransposeReference:r[1].er=h.add32(r[1].er,0x168fc);break;
case S::ReturnTransposeReference:f.ret();break;
case S::ReadTransposeDownHardware:f.move(r[0].rl,task.hardware());break;
case S::TestTransposeDownEvent:f.move(r[6].rh,r[6].rh);break;
case S::SelectTransposeDownEvent:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadTransposeToggle:f.move(r[0].rl,task.toggle());break;
case S::FlipTransposeToggle:r[0].rl^=0x1;value(h,r[0].rl);break;
case S::RefreshTransposeToggle:f.call(0x6c3c);break;
case S::PublishTransposeToggle:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::TestTransposeDownContact:f.move(r[6].rh,r[6].rh);break;
case S::ReturnTransposeDownRelease:f.ret();break;
case S::LookupTransposeDown:f.call(0x4afc);break;
case S::BindTransposeDownZero:f.move(r[6].rh,0x0);break;
case S::ReadTransposeDownOther:flag(h,4,(task.flags()&128)==0);break;
case S::ReadTransposeDown:f.move(r[6].rh,task.value());break;
case S::DecrementTransposeDown:f.inc(r[6].rh,-1);break;
case S::TestTransposeDownLimit:h.sub8(r[6].rh,0xfffffffd);break;
case S::ClampTransposeDown:f.move(r[6].rh,0xfffffffd);break;
case S::ReadTransposeUpHardware:f.move(r[0].rl,task.hardware());break;
case S::TestTransposeUpEvent:f.move(r[6].rh,r[6].rh);break;
case S::SelectTransposeUpEvent:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::EnterTransposeUp:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::TestTransposeUpContact:f.move(r[6].rh,r[6].rh);break;
case S::ReturnTransposeUpRelease:f.ret();break;
case S::LookupTransposeUp:f.call(0x4afc);break;
case S::BindTransposeUpZero:f.move(r[6].rh,0x0);break;
case S::ReadTransposeUpOther:flag(h,4,(task.flags()&64)==0);break;
case S::BindTransposeUpLimit:f.move(r[6].rl,0x4);break;
case S::ReadTransposeHardwareType:f.move(r[0].rl,task.type());break;
case S::TestTransposeHardwareType:h.sub8(r[0].rl,0x4);break;
case S::BindTransposeAlternateLimit:f.move(r[6].rl,0x3);break;
case S::ReadTransposeUp:f.move(r[6].rh,task.value());break;
case S::IncrementTransposeUp:f.inc(r[6].rh,1);break;
case S::TestTransposeUpLimit:h.sub8(r[6].rh,r[6].rl);break;
case S::ClampTransposeUp:f.move(r[6].rh,r[6].rl);break;
case S::BindTransposeContext:f.move(r[0].er,0x168fc);break;
case S::ApplyTranspose:f.call(0x6ae6);break;
case S::ClearTransposeContext:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::ConvertTranspose:f.call(0x4c1e);break;
case S::BindTransposeNotice:f.move(r[6].er,0x14f2c);break;
case S::ShowTransposeNotice:f.call(0x55b8);break;
case S::BindTransposeBase:f.move(r[1].er,0x168fc);break;
case S::ReadTransposeValue:f.move(r[0].rl,task.value());break;
case S::ShowTransposeValue:f.call(0xa4c);break;
case S::ReturnTranspose:f.ret();break;
case S::ReadResetMode:f.move(r[0].rl,task.mode());break;
case S::ReadResetPart:f.move(r[0].rh,task.savedPart());break;
case S::SaveResetContext:f.push(r[0].r);break;
case S::BindResetMode:f.move(r[0].rl,0x0);break;
case S::BindResetPart:f.move(r[0].rl,0x0);break;
case S::BindResetCount:f.move(r[0].rh,0x5);break;
case S::SaveResetPart:f.push(r[0].r);break;
case S::ResetPartMidi:f.call(0x3074);break;
case S::ResetPartVoices:f.call(0x9374);break;
case S::ResetPartQueue:f.call(0x2bc8);break;
case S::BindResetMulti:f.move(r[0].rl,0x1);break;
case S::RestoreResetPart:f.pop(r[0].r);break;
case S::NextResetPart:f.inc(r[0].rl,1);break;
case S::DecrementResetCount:f.inc(r[0].rh,-1);break;
case S::RestoreResetContext:f.pop(r[0].r);break;
case S::ReturnReset:f.ret();break;
case S::ReadTransposeType:f.move(r[6].rl,task.type());break;
case S::TestTransposeType:h.sub8(r[6].rl,0x4);break;
case S::IncrementTransposeValue:f.inc(r[6].rh,1);break;
case S::BindSemitoneScale:f.move(r[6].rl,0xc);break;
case S::ScaleTranspose:f.multiplySigned(r[6].rh,r[6].r);break;
case S::ReadOldSemitone:f.move(r[6].rh,task.oldSemitone());break;
case S::ReturnSemitone:f.ret();break;
case S::ReadCurrentSemitone:f.move(r[6].rl,task.semitone());break;
case S::ReadPreviousSemitone:f.move(r[6].rh,task.oldSemitone());break;
case S::CompareSemitones:h.sub8(r[6].rl,r[6].rh);break;
case S::SelectSemitoneReset:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReturnSemitoneCheck:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StatePanelModes clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
