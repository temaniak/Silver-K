#include "ksStateScreenCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateScreenPartialTest partialTest=nullptr;using T=StateScreenTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,117> points{{{0x582a,2},{0x582c,2},{0x582e,6},{0x5834,2},{0x5836,2},{0x5838,2},{0x583a,2},{0x583c,2},{0x583e,2},{0x5840,6},{0x5846,4},{0x584a,6},{0x5850,2},{0x5852,6},{0x5858,2},{0x585a,6},{0x5860,2},{0x5862,2},{0x5864,2},{0x5866,8},{0x586e,8},{0x5876,2},{0x5878,2},{0x587a,2},{0x587c,2},{0x587e,6},{0x5884,4},{0x5888,2},{0x588a,2},{0x588c,2},{0x588e,6},{0x5894,2},{0x5896,2},{0x5898,2},{0x589a,2},{0x589c,4},{0x58a0,2},{0x58a2,2},{0x58a4,4},{0x58a8,2},{0x58aa,2},{0x58ac,2},{0x58ae,6},{0x58b4,2},{0x58b6,4},{0x58ba,6},{0x58c0,8},{0x58c8,2},{0x58ca,2},{0x58cc,2},{0x58ce,6},{0x58d4,4},{0x58d8,2},{0x58da,2},{0x58dc,2},{0x58de,2},{0x58e0,2},{0x58e2,2},{0x58e4,6},{0x58ea,4},{0x58ee,2},{0x58f0,2},{0x58f2,2},{0x58f4,2},{0x58f6,2},{0x58f8,6},{0x58fe,2},{0x5900,2},{0x5902,2},{0x5904,4},{0x5908,2},{0x590a,2},{0x590c,6},{0x5912,2},{0x5914,6},{0x591a,6},{0x5920,4},{0x5924,2},{0x5926,4},{0x592a,2},{0x592c,2},{0x592e,2},{0x5930,2},{0x5932,6},{0x5938,4},{0x593c,6},{0x5942,4},{0x5946,2},{0x5948,6},{0x594e,2},{0x5950,2},{0x5952,2},{0x5954,2},{0x5956,8},{0x595e,8},{0x5966,2},{0x5968,2},{0x596a,4},{0x596e,2},{0x5970,4},{0x5974,2},{0x5976,8},{0x597e,2},{0x5980,4},{0x5984,2},{0x5986,2},{0x5988,2},{0x598a,2},{0x598c,4},{0x5990,2},{0x5992,2},{0x5994,2},{0x5996,4},{0x599a,2},{0x599c,4},{0x59a0,4},{0x59a4,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){return at<0x400000&&!(previous&1)&&at==(unsigned(previous)|1u)?0:3;}
}
void setStateScreenPartialForTests(StateScreenPartialTest value){partialTest=value;}
bool StateScreenCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x582a:kind=T::Kind::Descriptor;break;case 0x58ae:kind=T::Kind::Set;break;case 0x58ce:kind=T::Kind::Next;break;case 0x58e4:kind=T::Kind::Previous;break;case 0x5900:kind=T::Kind::Refresh;break;case 0x590a:kind=T::Kind::Gate;break;case 0x5932:kind=T::Kind::Render;break;case 0x5976:kind=T::Kind::Label;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native screen capacity exceeded");slot->emplace(*kind,h.getSP(),uint8_t(pc==0x58ae?h.regs[6].rl:h.regs[0].rl));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild({uint32_t(r[0].er),uint8_t(r[1].rh),uint8_t(r[1].rl),!(h.ccr&4)});const auto stage=task.stage();
 const unsigned target=stage==S::WriteDynamicCount?task.pointer():stage==S::ClampDynamicPage?task.pointer()+1:stage==S::WriteCurrent?task.pointer():stage==S::WriteScreen?0x16785:0x16787;const auto store=byteClocks(target,h.lastwrite);
 const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field,uint32_t ref)->uint32_t{switch(field){case T::Read::Mode:return uint8_t(h.read8(0x167ba));case T::Read::Hardware:return uint8_t(h.read8(0xffec14));case T::Read::PrimaryPointer:return uint32_t(h.read32(0x101ae + ref));case T::Read::SecondaryPointer:return uint32_t(h.read32(0x101e6 + ref));case T::Read::DescriptorByte:return uint8_t(h.read8(ref));case T::Read::PatchPointer:return uint32_t(h.read32(0x167bc));case T::Read::Variant:return uint8_t(h.read8(ref+226));case T::Read::ResetFlag:return uint8_t(h.read8(0x169f7));case T::Read::Screen:return uint8_t(h.read8(0x16785));case T::Read::UiPage:return uint8_t(h.read8(0x16787));case T::Read::RecordPointer:return uint32_t(h.read32(0x167f0));}return 0;};
 auto write=[&](T::Write field,uint32_t ref,uint32_t v){if(field==T::Write::RecordPointer)f.store32(0x167f0,int32_t(v));else f.store8(field==T::Write::DescriptorByte?ref:field==T::Write::Screen?0x16785:0x16787,int8_t(v));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native screen admission differs");
 switch(stage){
case S::CheckDescriptorIndex:h.sub8(r[0].rl,0xc);break;
case S::ReadDescriptorMode:f.move(r[1].rl,task.mode());break;
case S::IncrementDescriptorIndex:f.inc(r[0].rl,1);break;
case S::DoubleDescriptorIndex:f.shift(r[0].rl,1,true);break;
case S::QuadrupleDescriptorIndex:f.shift(r[0].rl,1,true);break;
case S::ClearDescriptorWord:r[0].r&=0xff;value(h,r[0].r);break;
case S::ClearDescriptorLong:r[0].er&=0xffff;value(h,r[0].er);break;
case S::LocatePrimaryTable:r[0].er=h.add32(r[0].er,0x101ae);break;
case S::ReadPrimaryPointer:f.move(r[0].er,task.pointer());break;
case S::ReadDescriptorHardware:f.move(r[1].rl,task.hardware());break;
case S::CompareHardwareDescriptor:h.sub32(r[0].er,0x10517);break;
case S::RemapHardwareDescriptor:f.move(r[0].er,0x10569);break;
case S::ReadHeader:f.move(r[1].rl,task.header());break;
case S::CompareSentinel:h.sub8(r[1].rl,0xffffffff);break;
case S::ReadPatchPointer:f.move(r[1].er,task.patch());break;
case S::ReadVariant:f.move(r[0].rl,task.variant());break;
case S::RetainVariant:f.move(r[1].rl,r[0].rl);break;
case S::ScaleVariant:f.shift(r[0].rl,2,true);break;
case S::ClearVariantWord:r[0].r&=0xff;value(h,r[0].r);break;
case S::ClearVariantLong:r[0].er&=0xffff;value(h,r[0].er);break;
case S::LocateSecondaryTable:r[0].er=h.add32(r[0].er,0x101e6);break;
case S::ReadSecondaryPointer:f.move(r[0].er,task.pointer());break;
case S::CompareDynamicVariant:h.sub8(r[1].rl,0x6);break;
case S::BindDynamicCount:f.move(r[1].rh,0x3);break;
case S::ReadDynamicMode:f.move(r[1].rl,task.mode());break;
case S::IncrementDynamicCount:f.inc(r[1].rh,1);break;
case S::DecrementDynamicCount:f.inc(r[1].rh,-1);break;
case S::ReadDynamicPage:f.move(r[1].rl,task.page());break;
case S::CompareDynamicPage:h.sub8(r[1].rl,r[1].rh);break;
case S::ReadCount:f.move(r[1].rh,task.count());r[0].er+=1;h.clockI(2);break;
case S::ReadCurrent:f.move(r[1].rl,task.page());break;
case S::ReturnDescriptor:f.ret();break;
case S::BindScreen:f.move(r[0].rl,r[6].rl);break;
case S::LookupSet:f.call(0x582a);break;
case S::BindResetBase:f.move(r[2].er,0x168fc);break;
case S::ReadReset:f.move(r[2].rl,task.reset());break;
case S::ResetPage:f.move(r[1].rl,0x0);break;
case S::ReadNextScreen:f.move(r[0].rl,task.index());break;
case S::LookupNext:f.call(0x582a);break;
case S::IncrementPage:f.inc(r[1].rl,1);break;
case S::ComparePage:h.sub8(r[1].rl,r[1].rh);break;
case S::BindPageLimit:f.move(r[1].rl,r[1].rh);break;
case S::DecrementPageLimit:f.inc(r[1].rl,-1);break;
case S::ReadPreviousScreen:f.move(r[0].rl,task.index());break;
case S::LookupPrevious:f.call(0x582a);break;
case S::DecrementPage:f.inc(r[1].rl,-1);break;
case S::FloorPage:f.move(r[1].rl,0x0);break;
case S::IncrementCurrentAddress:r[0].er=int32_t(uint32_t(r[0].er)+0x1);break;
case S::RenderSelection:f.call(0x5932);break;
case S::RefreshGate:f.call(0x590a);break;
case S::InitializeCalibration:f.call(0x1f84);break;
case S::ReturnRefresh:f.ret();break;
case S::ClearGate:f.move(r[0].rl,0x0);break;
case S::ReadGateHardware:f.move(r[1].rl,task.hardware());break;
case S::ReadGateScreen:f.move(r[1].rh,task.screen());break;
case S::ReadGatePage:f.move(r[1].rl,task.page());break;
case S::CompareGateLocation:h.sub16(r[1].r,0x902);break;
case S::CheckCalibration:f.call(0x4efe);break;
case S::AcceptGate:f.move(r[0].rl,0x1);break;
case S::TestGate:f.move(r[0].rl,r[0].rl);break;
case S::ReturnGate:f.ret();break;
case S::BindHeading:f.move(r[6].er,0x14f5e);break;
case S::DisplayHeading:f.call(0x9b8);break;
case S::ReadRenderScreen:f.move(r[0].rl,task.index());break;
case S::LookupRender:f.call(0x582a);break;
case S::IncrementRecordBase:r[0].er=int32_t(uint32_t(r[0].er)+0x1);break;
case S::BindRecordSize:f.move(r[1].rh,0x14);break;
case S::MultiplyRecordIndex:r[1].r=uint8_t(r[1].r)*uint32_t(uint8_t(r[1].rh));h.clockI(12);break;
case S::ClearRecordIndex:r[1].er&=0xffff;value(h,r[1].er);break;
case S::LocateRecord:r[0].er=h.add32(r[0].er,r[1].er);break;
case S::ReadParameter:f.move(r[1].rl,task.parameter());break;
case S::ClearParameterWord:r[1].r&=0xff;value(h,r[1].r);break;
case S::ClearParameterLong:r[1].er&=0xffff;value(h,r[1].er);break;
case S::BindParameter:f.call(0x5a46);break;
case S::DisplayLabel:f.call(0x5976);break;
case S::ClearParameterBase:f.call(0x5a7c);break;
case S::ReturnRender:f.ret();break;
case S::ReadRecordPointer:f.move(r[0].er,task.pointer());break;
case S::BindLabelPosition:f.move(r[6].rl,0xffffff80);break;
case S::SetLabelPosition:f.call(0x976);break;
case S::BindLabelLength:f.move(r[5].rl,0x10);break;
case S::ReadCharacter:f.move(r[6].rl,task.character());r[0].er+=1;h.clockI(2);break;
case S::CompareCharacter:h.sub8(r[6].rl,0xffffffff);break;
case S::DisplayCharacter:f.call(0x976);break;
case S::DecrementLabelLength:f.inc(r[5].rl,-1);break;
case S::BindSeparator:f.move(r[6].rl,0x20);break;
case S::DisplaySeparator:f.call(0x976);break;
case S::ReadLabelParameter:f.move(r[1].rl,task.parameter());r[0].er+=1;h.clockI(2);break;
case S::ReadFormat:f.move(r[2].rh,task.format());break;
case S::DisplayValue:f.call(0x5a90);break;
case S::ReturnLabel:f.ret();break;
 default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native screen clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
