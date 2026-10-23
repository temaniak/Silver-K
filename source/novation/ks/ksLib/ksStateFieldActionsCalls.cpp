#include "ksStateFieldActionsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateFieldActionsPartialTest partialTest=nullptr;using T=StateFieldActionsTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,165> points{{{0x4f0c,2},{0x4f0e,6},{0x4f14,8},{0x4f1c,4},{0x4f20,2},{0x4f22,4},{0x4f26,2},{0x4f28,2},{0x4f2a,8},{0x4f32,8},{0x4f3a,8},{0x4f42,2},{0x4f44,2},{0x4f46,4},{0x4f4a,2},{0x4f4c,4},{0x4f50,8},{0x4f58,4},{0x4f5c,2},{0x4f5e,2},{0x4f60,2},{0x4f62,2},{0x4f64,2},{0x4f66,2},{0x4f68,2},{0x4f6a,2},{0x4f6c,2},{0x4f6e,2},{0x4f70,4},{0x4f74,2},{0x4f76,2},{0x4f78,2},{0x4f7a,2},{0x4f7c,2},{0x4f7e,2},{0x4f80,2},{0x4f82,2},{0x4f84,2},{0x4f86,2},{0x4f88,2},{0x4f8a,2},{0x4f8c,6},{0x4f92,4},{0x4f96,2},{0x4f98,6},{0x4f9e,8},{0x4fa6,2},{0x4fa8,8},{0x4fb0,2},{0x4fb2,8},{0x4fba,2},{0x4fbc,2},{0x4fbe,2},{0x4fc0,2},{0x4fc2,2},{0x4fc4,6},{0x4fca,4},{0x4fce,2},{0x4fd0,6},{0x4fd6,2},{0x4fd8,4},{0x4fdc,2},{0x4fde,6},{0x4fe4,8},{0x4fec,2},{0x4fee,2},{0x4ff0,2},{0x4ff2,2},{0x4ff4,2},{0x4ff6,4},{0x4ffa,6},{0x5000,4},{0x5004,2},{0x5006,2},{0x5008,6},{0x500e,4},{0x51d8,4},{0x51dc,4},{0x51e0,8},{0x51e8,2},{0x51ea,2},{0x51ec,2},{0x51ee,4},{0x51f2,2},{0x51f4,4},{0x51f8,4},{0x51fc,2},{0x51fe,4},{0x5202,2},{0x5204,8},{0x520c,8},{0x5214,2},{0x5216,2},{0x5218,2},{0x521a,2},{0x521c,8},{0x5224,8},{0x522c,2},{0x522e,2},{0x5230,2},{0x5232,2},{0x5234,2},{0x5236,2},{0x5238,2},{0x523a,4},{0x523e,2},{0x5240,2},{0x5242,2},{0x5244,2},{0x5246,2},{0x5248,2},{0x524a,2},{0x524c,2},{0x524e,4},{0x5252,4},{0x5256,2},{0x5258,8},{0x5260,8},{0x5268,2},{0x526a,2},{0x526c,2},{0x526e,2},{0x5270,8},{0x5278,2},{0x527a,2},{0x527c,2},{0x527e,2},{0x5280,2},{0x5282,2},{0x5284,2},{0x5286,2},{0x5288,2},{0x528a,2},{0x528c,2},{0x528e,2},{0x5290,4},{0x5294,2},{0x5296,2},{0x5298,2},{0x529a,2},{0x529c,2},{0x529e,2},{0x52a0,2},{0x52a2,2},{0x52a4,2},{0x52a6,4},{0x52aa,2},{0x52e4,2},{0x530e,2},{0x5310,2},{0x5312,2},{0x5314,2},{0x5316,2},{0x5318,4},{0x531c,2},{0x531e,2},{0x5320,2},{0x5322,8},{0x532a,2},{0x532c,2},{0x532e,2},{0x5330,2},{0x5332,2},{0x5334,8},{0x533c,4}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::Refresh:return 0x5340;case T::Exit::Display:return 0x78c;case T::Exit::Apply:return 0x6ae6;default:throw std::runtime_error("Missing StateFieldActions exit");}}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateFieldActionsPartialForTests(StateFieldActionsPartialTest value){partialTest=value;}
bool StateFieldActionsCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x4f0c:kind=T::Kind::Global;break;case 0x4f22:kind=T::Kind::Selector;break;case 0x4f4a:kind=T::Kind::Indexed;break;case 0x5006:kind=T::Kind::Clear;break;case 0x51d8:kind=T::Kind::DirectStep;break;case 0x51f4:kind=T::Kind::IndexedStep;break;case 0x51fe:kind=T::Kind::Bit;break;case 0x5252:kind=T::Kind::Pair;break;case 0x52e4:kind=T::Kind::Return;break;case 0x530e:kind=T::Kind::Current;break;case 0x531e:kind=T::Kind::Cycle;break;case 0x5334:kind=T::Kind::Control;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateFieldActions capacity exceeded");slot->emplace(*kind,h.getSP(),uint32_t(h.regs[0].er),uint32_t(h.regs[1].er),uint16_t(h.regs[6].r),uint16_t(h.regs[5].r),uint16_t(h.regs[2].r),uint8_t(h.regs[2].rh));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint32_t(r[1].er),uint8_t(r[6].rh),uint16_t(r[2].r),!(h.ccr&4));const auto stage=task.stage();
 auto accessCost=[&](uint32_t ref,unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){const unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};unsigned store=3;switch(stage){case S::WriteGlobalContext:store=accessCost(uint32_t(0x167a6),4,h.lastwrite);break;case S::ReadSelectorValue:store=accessCost(uint32_t(r[0].er+1),1,-1);break;case S::ReadSelectorPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::WriteSelectorValue:store=accessCost(uint32_t(r[0].er+222),1,h.lastwrite);break;case S::ReadValueType:store=accessCost(uint32_t(r[0].er+6),1,-1);break;case S::ReadCurrentValue:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::WriteEditedField:store=accessCost(uint32_t(0x16793),1,h.lastwrite);break;case S::ReadDisplayEnabled:store=accessCost(uint32_t(r[6].er+250),1,-1);break;case S::ReadDisplayOverride:store=accessCost(uint32_t(0x167a6),4,-1);break;case S::ReadDisplayPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::WriteDisplayField:store=accessCost(uint32_t(0x16794),1,h.lastwrite);break;case S::WriteDisplayLatch:store=accessCost(uint32_t(0x16792),1,h.lastwrite);break;case S::ReadDisplayGuard:store=accessCost(uint32_t(r[0].er+252),1,-1);break;case S::WriteClearedDisplayLatch:store=accessCost(uint32_t(0x16792),1,h.lastwrite);break;case S::ReadDirectPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadBitSelector:store=accessCost(uint32_t(r[0].er+2),1,-1);break;case S::ReadBitPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadBitIndex:store=accessCost(uint32_t(r[3].er),1,-1);break;case S::ReadBitMapping:store=accessCost(uint32_t(r[2].er+4),1,-1);break;case S::ReadBitControl:store=accessCost(uint32_t(r[0].er+7),1,-1);break;case S::ReadBitValue:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadPairSelector:store=accessCost(uint32_t(r[0].er+2),1,-1);break;case S::ReadPairPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadPairIndex:store=accessCost(uint32_t(r[3].er),1,-1);break;case S::ReadPairMapping:store=accessCost(uint32_t(r[2].er+4),1,-1);break;case S::ReadPairValue:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadCycleValue:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadCycleLimit:store=accessCost(uint32_t(r[0].er+1),1,-1);break;case S::ReadControlFlag:store=accessCost(uint32_t(r[0].er+7),1,-1);break;default:break;} const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{return field==T::Read::Byte?uint8_t(h.read8(ref)):uint32_t(h.read32(field==T::Read::Patch?0x167bc:0x167a6));};auto write=[&](T::Write field,uint32_t ref,uint32_t value){if(field==T::Write::Context)f.store32(0x167a6,int32_t(value));else f.store8(field==T::Write::Byte?ref:field==T::Write::EditedField?0x16793:field==T::Write::DisplayField?0x16794:0x16792,int8_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateFieldActions admission differs");
 switch(stage){case S::SaveGlobalEvent:f.push(r[6].r);break;
case S::BindGlobalContext:f.move(r[1].er,0x168fc);break;
case S::ResolveGlobalField:f.call(0x52fc);break;
case S::SaveSelectorDescriptor:f.push(r[0].er);break;
case S::SaveSelectorIndex:f.push(r[5].r);break;
case S::SaveSelectorEvent:f.push(r[6].r);break;
case S::ReadSelectorValue:f.move(r[6].rl,task.selector());break;
case S::ReadSelectorPatch:f.move(r[0].er,task.patch());break;
case S::RestoreSelectorEvent:f.pop(r[6].r);break;
case S::RestoreSelectorIndex:f.pop(r[5].r);break;
case S::RestoreSelectorDescriptor:f.pop(r[0].er);break;
case S::SaveIndexedEvent:f.push(r[6].r);break;
case S::ResolveIndexedField:f.call(0x52ac);break;
case S::ReadValueType:f.move(r[2].rh,task.type());break;
case S::ResolveValueBounds:f.call(0x5112);break;
case S::TestValueMode:f.move(r[5].rh,r[5].rh);break;
case S::BindValueDelta:f.move(r[6].rl,r[6].rh);break;
case S::ReadCurrentValue:f.move(r[6].rh,task.value());break;
case S::TestValueDelta:f.move(r[6].rl,r[6].rl);break;
case S::CompareZeroTypeOne:h.sub8(r[5].rl,0x1);break;
case S::CompareZeroTypeFourteen:h.sub8(r[5].rl,0xe);break;
case S::DisplayZeroValue:f.call(0x5334);break;
case S::RestoreZeroEvent:f.pop(r[6].r);break;
case S::ReturnZeroValue:f.ret();break;
case S::AddValueDelta:r[6].rh=h.add8(r[6].rh,r[6].rl);break;
case S::TestDeltaSign:f.move(r[6].rl,r[6].rl);break;
case S::CompareValueUpper:h.sub8(r[6].rh,r[2].rh);break;
case S::ClampValueUpper:f.move(r[6].rh,r[2].rh);break;
case S::CompareValueLower:h.sub8(r[6].rh,r[2].rl);break;
case S::ClampValueLower:f.move(r[6].rh,r[2].rl);break;
case S::ApplyEditedField:f.call(0x6ae6);break;
case S::RestoreDisplayIndex:f.pop(r[5].r);break;
case S::BindDisplayContext:f.move(r[6].er,0x168fc);break;
case S::ReadDisplayEnabled:f.move(r[6].rl,task.enabled());break;
case S::ReadDisplayOverride:f.move(r[0].er,task.base());break;
case S::ReadDisplayPatch:f.move(r[0].er,task.base());break;
case S::LocateDisplayOffset:r[1].er=h.sub32(r[1].er,r[0].er);break;
case S::SaveDisplayEvent:f.push(r[6].r);break;
case S::SaveDisplayOffset:f.push(r[1].r);break;
case S::BuildDisplayLabel:f.call(0x5012);break;
case S::RestoreDisplayOffset:f.pop(r[1].r);break;
case S::ShowDisplayValue:f.call(0x5a90);break;
case S::BindDisplayLatch:f.move(r[0].rl,0x1);break;
case S::RestoreDisplayEvent:f.pop(r[6].r);break;
case S::ReadDisplayGate:f.call(0x4efe);break;
case S::BindDisplayGuard:f.move(r[0].er,0x168fc);break;
case S::ReadDisplayGuard:f.move(r[0].rl,task.guard());break;
case S::NormalizeDisplayDelay:r[6].r&=0xff;value(h,r[6].r);break;
case S::ScaleDisplayDelayOne:f.shift(r[6].r,1,true);break;
case S::ScaleDisplayDelayTwo:f.shift(r[6].r,1,true);break;
case S::ScaleDisplayDelayThree:f.shift(r[6].r,1,true);break;
case S::BiasDisplayDelay:r[6].r=h.add16(r[6].r,0xc8);break;
case S::BindDisplayCallback:f.move(r[5].er,0x5006);break;
case S::ScheduleDisplay:f.call(0x4020);break;
case S::ReturnDisplay:f.ret();break;
case S::ClearDisplayLatch:f.move(r[0].rl,0x0);break;
case S::RefreshDisplay:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::DirectAddress:f.call(0x52e6);break;
case S::StepDirect:f.call(0x530e);break;
case S::ReadDirectPatch:f.move(r[0].er,task.patch());break;
case S::LocateDirectOffset:r[1].er=h.sub32(r[1].er,r[0].er);break;
case S::CompareDirectField:h.sub8(r[1].rl,0xffffffe1);break;
case S::ApplyDirectSideEffect:f.call(0xa1fa);break;
case S::ReturnDirect:f.ret();break;
case S::IndexedAddress:f.call(0x52ac);break;
case S::StepIndexed:f.call(0x530e);break;
case S::ReturnIndexed:f.ret();break;
case S::ResolveBitAddress:f.call(0x52e6);break;
case S::ClearBitSelector:r[2].er=h.sub32(r[2].er,r[2].er);break;
case S::ReadBitSelector:f.move(r[2].rl,task.selector());break;
case S::ReadBitPatch:f.move(r[3].er,task.patch());break;
case S::LocateBitSelector:r[3].er=h.add32(r[3].er,r[2].er);break;
case S::ClearBitIndex:r[2].er=h.sub32(r[2].er,r[2].er);break;
case S::ReadBitIndex:f.move(r[2].rl,task.index());break;
case S::LocateBitMapping:r[2].er=h.add32(r[2].er,r[0].er);break;
case S::ReadBitMapping:f.move(r[2].rl,task.bit());break;
case S::ReadBitControl:f.move(r[6].rl,task.control());break;
case S::BindBitRequest:f.move(r[2].rh,r[6].rh);break;
case S::ReadBitValue:f.move(r[6].rh,task.value());break;
case S::TestBitRequest:f.move(r[2].rh,r[2].rh);break;
case S::ToggleBit:r[6].rh^=(1u<<(uint8_t(r[2].rl)&7));break;
case S::SaveBitValue:f.push(r[6].r);break;
case S::SaveBitRequest:f.push(r[2].r);break;
case S::ApplyBit:f.call(0x6ae6);break;
case S::RestoreBitRequest:f.pop(r[2].r);break;
case S::RestoreBitValue:f.pop(r[6].r);break;
case S::TestBitShift:f.move(r[2].rl,r[2].rl);break;
case S::ShiftBitValue:f.shift(r[6].rh,1,false);break;
case S::TickBitShift:f.inc(r[2].rl,-1);break;
case S::MaskBitValue:r[6].rh&=0x1;value(h,r[6].rh);break;
case S::DisplayBit:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ResolvePairAddress:f.call(0x52e6);break;
case S::ClearPairSelector:r[2].er=h.sub32(r[2].er,r[2].er);break;
case S::ReadPairSelector:f.move(r[2].rl,task.selector());break;
case S::ReadPairPatch:f.move(r[3].er,task.patch());break;
case S::LocatePairSelector:r[3].er=h.add32(r[3].er,r[2].er);break;
case S::ClearPairIndex:r[2].er=h.sub32(r[2].er,r[2].er);break;
case S::ReadPairIndex:f.move(r[2].rl,task.index());break;
case S::LocatePairMapping:r[2].er=h.add32(r[2].er,r[0].er);break;
case S::ReadPairMapping:f.move(r[2].rl,task.bit());break;
case S::BindPairRequest:f.move(r[2].rh,r[6].rh);break;
case S::ReadPairValue:f.move(r[6].rh,task.value());break;
case S::SavePairRequest:f.push(r[2].r);break;
case S::TestPairShift:f.move(r[2].rl,r[2].rl);break;
case S::RotatePairRight:f.rotate(r[6].rh,1,false,false);break;
case S::TickPairShift:f.inc(r[2].rl,-1);break;
case S::BindPairUpper:f.move(r[6].rl,r[6].rh);break;
case S::MaskPairUpper:r[6].rl&=0xfffffffc;value(h,r[6].rl);break;
case S::MaskPairValue:r[6].rh&=0x3;value(h,r[6].rh);break;
case S::SavePairValue:f.push(r[6].r);break;
case S::CyclePair:f.call(0x531e);break;
case S::RestorePairValue:f.pop(r[3].r);break;
case S::RestorePairRequest:f.pop(r[2].r);break;
case S::MergePairValue:r[6].rh|=r[3].rl;value(h,r[6].rh);break;
case S::TestPairRestore:f.move(r[2].rl,r[2].rl);break;
case S::RotatePairLeft:f.rotate(r[6].rh,1,true,false);break;
case S::TickPairRestore:f.inc(r[2].rl,-1);break;
case S::TestPairRequest:f.move(r[2].rh,r[2].rh);break;
case S::SelectPairRequest:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReturnPair:f.ret();break;
case S::ReturnAddress:f.ret();break;
case S::RetainCycleRequest:f.move(r[2].rh,r[6].rh);break;
case S::ReadCycleValue:f.move(r[6].rh,task.value());break;
case S::CycleCurrentValue:f.call(0x531e);break;
case S::TestCycleRequest:f.move(r[2].rh,r[2].rh);break;
case S::ApplyCycleValue:f.call(0x6ae6);break;
case S::ReturnCycleValue:f.ret();break;
case S::TestCycleMode:f.move(r[2].rh,r[2].rh);break;
case S::ReadCycleLimit:f.move(r[2].rl,task.limit());break;
case S::IncrementCycleValue:f.inc(r[6].rh,1);break;
case S::CompareCycleLimit:h.sub8(r[6].rh,r[2].rl);break;
case S::ClearCycleValue:f.move(r[6].rh,0x0);break;
case S::ReturnCycle:f.ret();break;
case S::ReadControlFlag:f.move(r[6].rl,task.control());break;
case S::DisplayControlFlag:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateFieldActions clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
