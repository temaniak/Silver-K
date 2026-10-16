#include "ksStateModeCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateModeTask;using S=T::Stage;StateModePartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,63> points{{{0x6ec6,4},{0x6eca,4},{0x6ece,2},{0x6ed0,6},{0x6ed6,2},{0x6ed8,2},{0x6eda,6},{0x6ee0,2},{0x6ee2,6},{0x6ee8,6},{0x6eee,6},{0x6ef4,8},{0x6efc,6},{0x6f02,8},{0x6f0a,6},{0x6f10,6},{0x6f16,2},{0x6f18,2},{0x6f1a,6},{0x6f20,6},{0x6f26,4},{0x6f2a,6},{0x6f30,6},{0x6f36,4},{0x6f3a,6},{0x6f40,2},{0x6f42,6},{0x6f48,2},{0x6f4a,2},{0x6f4c,2},{0x6f4e,2},{0x6f50,2},{0x6f52,2},{0x6f54,2},{0x6f56,6},{0x6f5c,6},{0x6f62,2},{0x6f64,6},{0x6f6a,6},{0x6f70,2},{0x6f72,2},{0x6f74,6},{0x6f7a,2},{0x6f7c,6},{0x6f82,6},{0x6f88,6},{0x6f8e,2},{0x6f90,2},{0x6f92,6},{0x6f98,6},{0x6f9e,2},{0x6fa0,2},{0x6fa2,6},{0x6fa8,2},{0x6faa,2},{0x6fac,2},{0x6fae,6},{0x6fb4,6},{0x6fba,6},{0x6fc0,2},{0x6fc2,4},{0x6fc6,4},{0x6fca,2}}};Point point(const T& t){return t.awaitingPart()?Point{0x6f72,2}:points[unsigned(t.stage())];}}
void setStateModePartialForTests(StateModePartialTest f){partialTest=f;}
bool StateModeCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x6ec6){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native state-mode capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingPart())task.resumePart();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{if(what==T::Read::CurrentProgram||what==T::Read::SingleSlot||what==T::Read::SlotProgram)return observed=uint16_t(h.read16(what==T::Read::CurrentProgram?0x167ac:what==T::Read::SingleSlot?0x167ae:0x167ae +task.slotOffset()));return observed=uint8_t(h.read8(what==T::Read::Mode?0x167ba:what==T::Read::PreviousMode?0x167bb:what==T::Read::PartMask?0x167d0:what==T::Read::AuxMask?0x167d1:what==T::Read::Active?0x16798:0x167d4));};
 auto write=[&](T::Write what,uint32_t value){switch(what){case T::Write::CurrentBase:f.store32(0x167bc,value);return;case T::Write::LiveBase:f.store32(0x167c0,value);return;case T::Write::SingleSlot:f.store16(0x167ae,value);return;case T::Write::CurrentProgram:f.store16(0x167ac,value);return;case T::Write::SlotProgram:f.store16(0x167ae +task.slotOffset(),value);return;default:break;}f.store8(what==T::Write::Active?0x16798:what==T::Write::PartMask?0x167d2:what==T::Write::AuxMask?0x167d3:what==T::Write::PreviousMode?0x167bb:0x167d4,value);};
 if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native mode admission differs");switch(stage){case S::RetainAddress:{f.push(r[0].er);break;}
case S::RetainSlot:{f.push(r[1].er);break;}
case S::RetainMode:{f.push(r[2].r);break;}
case S::ReadMode:{f.move(r[2].rl,observed);break;}
case S::AdmitMode:{;break;}
case S::ClearActive:{f.move(r[0].rl,0x0);break;}
case S::WriteZeroActive:{;break;}
case S::SetSingleMask:{f.move(r[0].rl,0x1);break;}
case S::WriteSingleAux:{;break;}
case S::WriteSingleMask:{;break;}
case S::SetSingleBase:{f.move(r[0].er,0x168fc);break;}
case S::WriteSingleBase:{;break;}
case S::SetSingleLive:{f.move(r[0].er,0x16e12);break;}
case S::WriteSingleLive:{;break;}
case S::ReadSinglePreviousMode:{f.move(r[0].rl,observed);break;}
case S::WriteSinglePreviousMode:{;break;}
case S::CompareSingleMode:{h.sub8(r[2].rl,r[0].rl);break;}
case S::AdmitSingleMode:{;break;}
case S::ReadSingleCurrent:{f.move(r[0].r,observed);break;}
case S::WriteSingleSlot:{;break;}
case S::JumpRestoreSingle:{h.clockI(2);break;}
case S::ReadSingleSlot:{f.move(r[0].r,observed);break;}
case S::WriteSingleCurrent:{;break;}
case S::JumpRestoreChanged:{h.clockI(2);break;}
case S::ReadPartMask:{f.move(r[0].rh,observed);break;}
case S::MaskParts:{r[0].rh&=0xf;value(h,r[0].rh);break;}
case S::WritePartMask:{;break;}
case S::ClearActiveIndex:{f.move(r[0].rl,0x0);break;}
case S::ShiftPartMask:{f.shift(r[0].rh,1,false);break;}
case S::AdmitPartBit:{;break;}
case S::IncrementActiveIndex:{f.inc(r[0].rl,1);break;}
case S::CompareActiveBound:{h.sub8(r[0].rl,0x3);break;}
case S::AdmitActiveBound:{;break;}
case S::FinishActiveIndex:{f.inc(r[0].rl,1);break;}
case S::WriteActive:{;break;}
case S::ReadAuxMask:{f.move(r[0].rl,observed);break;}
case S::MaskAux:{r[0].rl&=0xf;value(h,r[0].rl);break;}
case S::WriteAuxMask:{;break;}
case S::ReadBindActive:{f.move(r[0].rl,observed);break;}
case S::BindPart:{f.call(0x6fcc);break;}
case S::ClearSlot:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::ReadSlotActive:{f.move(r[1].rl,observed);break;}
case S::DoubleSlot:{f.shift(r[1].rl,1,true);break;}
case S::AddSlotBase:{r[1].er=h.add32(r[1].er,0x167ae);break;}
case S::ReadPreviousMode:{f.move(r[0].rl,observed);break;}
case S::WritePreviousMode:{;break;}
case S::CompareMode:{h.sub8(r[2].rl,r[0].rl);break;}
case S::AdmitChangedMode:{;break;}
case S::ReadCompareActive:{f.move(r[0].rl,observed);break;}
case S::ReadPreviousActive:{f.move(r[0].rh,observed);break;}
case S::CompareActive:{h.sub8(r[0].rl,r[0].rh);break;}
case S::AdmitChangedActive:{;break;}
case S::ReadCurrentProgram:{f.move(r[0].r,observed);break;}
case S::WriteSlotProgram:{;break;}
case S::JumpRestore:{;break;}
case S::ReadSlotProgram:{f.move(r[0].r,observed);break;}
case S::WriteCurrentProgram:{;break;}
case S::ReadRememberActive:{f.move(r[0].rl,observed);break;}
case S::WriteRememberActive:{;break;}
case S::RestoreMode:{f.pop(r[2].r);break;}
case S::RestoreSlot:{f.pop(r[1].er);break;}
case S::RestoreAddress:{f.pop(r[0].er);break;}
case S::ReturnMode:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native mode clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingPart())call.token=h.getSP();h.pc=h.makepc(task.awaitingPart()?f.next:point(task).location);return true;}}
