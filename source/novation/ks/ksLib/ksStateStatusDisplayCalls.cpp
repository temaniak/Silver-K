#include "ksStateStatusDisplayCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateStatusDisplayPartialTest partialTest=nullptr;using T=StateStatusDisplayTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,157> points{{{0x55e2,6},{0x55e8,6},{0x55ee,2},{0x55f0,6},{0x55f6,2},{0x55f8,6},{0x55fe,2},{0x5600,2},{0x5602,6},{0x5608,2},{0x560a,6},{0x5610,4},{0x5614,2},{0x5616,4},{0x561a,2},{0x561c,6},{0x5622,2},{0x5624,2},{0x5626,6},{0x562c,2},{0x562e,2},{0x5630,4},{0x5634,2},{0x5636,2},{0x5638,2},{0x563a,2},{0x563c,2},{0x563e,4},{0x5642,4},{0x5646,4},{0x564a,2},{0x564c,2},{0x564e,4},{0x5652,2},{0x5654,2},{0x5656,2},{0x5658,2},{0x565a,4},{0x565e,4},{0x5662,2},{0x5664,2},{0x5666,4},{0x566a,6},{0x5670,4},{0x5674,2},{0x5676,6},{0x567c,2},{0x567e,2},{0x5680,4},{0x5684,2},{0x5686,4},{0x568a,2},{0x568c,6},{0x5692,2},{0x5694,2},{0x5696,6},{0x569c,2},{0x569e,2},{0x56a0,2},{0x56a2,2},{0x56a4,4},{0x56a8,2},{0x56aa,6},{0x56b0,2},{0x56b2,2},{0x56b4,4},{0x56b8,2},{0x56ba,2},{0x56bc,6},{0x56c2,4},{0x56c6,2},{0x56c8,6},{0x56ce,2},{0x56d0,2},{0x56d2,2},{0x56d4,2},{0x56d6,8},{0x56de,6},{0x56e4,8},{0x56ec,4},{0x56f0,2},{0x56f2,4},{0x56f6,2},{0x56f8,6},{0x56fe,8},{0x5706,2},{0x5708,4},{0x570c,8},{0x5714,2},{0x5716,6},{0x571c,2},{0x571e,6},{0x5724,2},{0x5726,2},{0x5728,4},{0x572c,8},{0x5734,4},{0x5738,2},{0x573a,2},{0x573c,2},{0x573e,2},{0x5740,2},{0x5742,2},{0x5744,4},{0x5748,4},{0x574c,2},{0x574e,2},{0x5750,4},{0x5754,8},{0x575c,4},{0x5760,2},{0x5762,2},{0x5764,2},{0x5766,2},{0x5768,2},{0x576a,2},{0x576c,2},{0x576e,6},{0x5774,6},{0x577a,2},{0x577c,2},{0x577e,2},{0x5780,2},{0x5782,2},{0x5784,2},{0x5786,4},{0x578a,2},{0x578c,2},{0x578e,2},{0x5790,2},{0x5792,2},{0x5794,2},{0x5796,2},{0x5798,2},{0x579a,4},{0x579e,8},{0x57a6,6},{0x57ac,6},{0x57b2,4},{0x57b6,2},{0x57b8,2},{0x57ba,2},{0x57bc,2},{0x57be,2},{0x57c0,2},{0x57c2,2},{0x57c4,2},{0x57c6,2},{0x57c8,2},{0x57ca,2},{0x57cc,2},{0x57ce,2},{0x57d0,2},{0x57d2,4},{0x57d6,6},{0x57dc,4},{0x57e0,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::Compare:return 0x571e;default:throw std::runtime_error("Missing StateStatusDisplay exit");}}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateStatusDisplayPartialForTests(StateStatusDisplayPartialTest value){partialTest=value;}
bool StateStatusDisplayCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x55e2:kind=T::Kind::Tick;break;case 0x5676:kind=T::Kind::Part;break;case 0x571e:kind=T::Kind::Compare;break;case 0x5666:kind=T::Kind::Schedule;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateStatusDisplay capacity exceeded");slot->emplace(*kind,h.getSP());++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(!(h.ccr&4),uint16_t(r[4].r),uint32_t(r[0].er));const auto stage=task.stage();
 auto accessCost=[&](uint32_t ref,unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){const unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};unsigned store=3;switch(stage){case S::ReadNoticeBusy:store=accessCost(uint32_t(0x16791),1,-1);break;case S::ReadPageBusy:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadLatchBusy:store=accessCost(uint32_t(0x16792),1,-1);break;case S::ReadUiBusy:store=accessCost(uint32_t(0x1679f),1,-1);break;case S::ReadSpecialBusy:store=accessCost(uint32_t(0x1679e),1,-1);break;case S::ReadSpecialCharacter:store=accessCost(uint32_t(0x17494),1,-1);break;case S::ReadStatusScreen:store=accessCost(uint32_t(0x16785),1,-1);break;case S::ReadStatusPage:store=accessCost(uint32_t(0x16787),1,-1);break;case S::ReadPartMode:store=accessCost(uint32_t(0x167ba),1,-1);break;case S::ReadScanPart:store=accessCost(uint32_t(0x167ee),1,-1);break;case S::WriteScanPart:store=accessCost(uint32_t(0x167ee),1,h.lastwrite);break;case S::ReadActiveMask:store=accessCost(uint32_t(0x167d3),1,-1);break;case S::ReadInactivePartMarker:store=accessCost(uint32_t(0x167d6),1,-1);break;case S::ReadSavedProgram:store=accessCost(uint32_t(0x167ac),2,-1);break;case S::ReadScanProgram:store=accessCost(uint32_t(r[1].er+92080),2,-1);break;case S::WriteScanProgram:store=accessCost(uint32_t(0x167ac),2,h.lastwrite);break;case S::ReadSavedPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::WriteScanPatch:store=accessCost(uint32_t(0x167bc),4,h.lastwrite);break;case S::WriteSavedPatch:store=accessCost(uint32_t(0x167bc),4,h.lastwrite);break;case S::WriteSavedProgram:store=accessCost(uint32_t(0x167ac),2,h.lastwrite);break;case S::ReadCompareProgram:store=accessCost(uint32_t(0x167ac),2,-1);break;case S::ReadStandardPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadStandardSource:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::ReadStandardTarget:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadSplitPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadPrefixSource:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::ReadPrefixTarget:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadHeaderSource:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::ReadHeaderTarget:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadSuffixSource:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::ReadSuffixTarget:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadFxPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadFxSource:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::ReadFxTarget:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadTailSource:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::ReadTailTarget:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadCompareMarker:store=accessCost(uint32_t(0x167d6),1,-1);break;default:break;} const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{if(field==T::Read::Patch)return uint32_t(h.read32(0x167bc));if(field==T::Read::Program||field==T::Read::PartProgram)return uint16_t(h.read16(field==T::Read::Program?0x167ac:ref));return uint8_t(h.read8(field==T::Read::Notice?0x16791:field==T::Read::Page?0x16790:field==T::Read::Latch?0x16792:field==T::Read::Ui?0x1679f:field==T::Read::Special?0x1679e:field==T::Read::SpecialCharacter?0x17494:field==T::Read::Screen?0x16785:field==T::Read::EditPage?0x16787:field==T::Read::Mode?0x167ba:field==T::Read::ScanPart?0x167ee:field==T::Read::Mask?0x167d3:field==T::Read::Marker?0x167d6:ref));};auto write=[&](T::Write field,uint32_t value){if(field==T::Write::ScanPart)f.store8(0x167ee,int8_t(value));else if(field==T::Write::Program)f.store16(0x167ac,int16_t(value));else f.store32(0x167bc,int32_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateStatusDisplay admission differs");
 switch(stage){case S::ReadNoticeBusy:f.move(r[0].rl,task.busy());break;
case S::ReadPageBusy:f.move(r[0].rh,task.value());break;
case S::JoinPageBusy:r[0].rl|=r[0].rh;value(h,r[0].rl);break;
case S::ReadLatchBusy:f.move(r[0].rh,task.value());break;
case S::JoinLatchBusy:r[0].rl|=r[0].rh;value(h,r[0].rl);break;
case S::ReadUiBusy:f.move(r[0].rh,task.value());break;
case S::JoinUiBusy:r[0].rl|=r[0].rh;value(h,r[0].rl);break;
case S::ReadSpecialBusy:f.move(r[0].rl,task.value());break;
case S::ReadSpecialCharacter:f.move(r[6].rl,task.value());break;
case S::ShowSpecialCharacter:f.call(0x976);break;
case S::ReadStatusGate:f.call(0x4efe);break;
case S::ReadStatusScreen:f.move(r[0].rl,task.value());break;
case S::CompareStatusScreen:h.sub8(r[0].rl,0x0);break;
case S::ReadStatusPage:f.move(r[0].rl,task.page());break;
case S::CompareMeterPage:h.sub8(r[0].rl,0x5);break;
case S::ShowMeter:f.call(0x29a6);break;
case S::CompareInputPage:h.sub8(r[0].rl,0xb);break;
case S::CompareOutputPage:h.sub8(r[0].rl,0xc);break;
case S::BindMeterInput:f.move(r[4].r,0x8059);break;
case S::ReadMeterInput:f.call(0x2568);break;
case S::BindMeterRange:f.move(r[0].r,0x300);break;
case S::MultiplyMeterInput:r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[4].r));h.clockI(20);break;
case S::BindMeterHigh:f.move(r[0].r,r[0].e);break;
case S::BiasMeterHigh:r[0].r=h.sub16(r[0].r,0x2cd);break;
case S::ClearMeterFloor:f.move(r[0].rl,0x0);break;
case S::HalveMeterValue:f.shift(r[0].rl,1,false);break;
case S::BindMeterValue:f.move(r[6].rl,r[0].rl);break;
case S::ShowMeterValue:f.call(0x2262);break;
case S::BindFastPeriod:f.move(r[6].r,0x1e);break;
case S::ShowPartStatus:f.call(0x5676);break;
case S::BindNormalPeriod:f.move(r[6].r,0x64);break;
case S::BindStatusCallback:f.move(r[5].er,0x55e2);break;
case S::ScheduleStatus:f.call(0x40d8);break;
case S::ReturnStatus:f.ret();break;
case S::ReadPartMode:f.move(r[0].rl,task.value());break;
case S::BindSinglePosition:f.move(r[6].rl,0xffffff90);break;
case S::ShowSinglePosition:f.call(0x976);break;
case S::BindSingleMarker:f.move(r[6].rl,0x20);break;
case S::JoinSingleMarker:h.clockI(2);break;
case S::BindScanAttempts:f.move(r[1].rl,0x3);break;
case S::ReadScanPart:f.move(r[0].rl,task.index());break;
case S::NextScanPart:f.inc(r[0].rl,1);break;
case S::MaskScanPart:r[0].rl&=0x3;value(h,r[0].rl);break;
case S::BindScanPosition:f.move(r[6].rl,r[0].rl);break;
case S::BindScanStride:f.move(r[6].rh,0x5);break;
case S::ScaleScanPosition:r[6].r=uint8_t(r[6].r)*uint32_t(uint8_t(r[6].rh));h.clockI(12);break;
case S::BiasScanPosition:r[6].rl=h.add8(r[6].rl,0xffffffc0);break;
case S::ShowScanPosition:f.call(0x976);break;
case S::BindInactiveMarker:f.move(r[6].rl,0x20);break;
case S::ReadActiveMask:f.move(r[0].rh,task.mask());break;
case S::TestActiveMask:flag(h,4,(uint8_t(r[0].rh)&(1u<<(uint8_t(r[0].rl)&7)))==0);break;
case S::ShowInactiveMarker:f.call(0x976);break;
case S::TickScanAttempts:f.inc(r[1].rl,-1);break;
case S::ReadInactivePartMarker:f.move(r[6].rl,task.value());break;
case S::ShowInactivePartMarker:f.call(0x976);break;
case S::ReturnInactive:f.ret();break;
case S::ReadSavedProgram:f.move(r[1].r,task.savedProgram());break;
case S::SaveSavedProgram:f.push(r[1].r);break;
case S::ClearScanIndex:r[1].er=h.sub32(r[1].er,r[1].er);break;
case S::BindScanIndex:f.move(r[1].rl,r[0].rl);break;
case S::DoubleScanIndex:f.shift(r[1].rl,1,true);break;
case S::ReadScanProgram:f.move(r[1].r,task.program());break;
case S::ReadSavedPatch:f.move(r[1].er,task.savedPatch());break;
case S::SaveSavedPatch:f.push(r[1].er);break;
case S::NormalizeScanIndex:r[0].r&=0xff;value(h,r[0].r);break;
case S::BindScanPatchSize:f.move(r[0].e,0x100);break;
case S::ScaleScanPatch:r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20);break;
case S::LocateScanPatch:r[0].er=h.add32(r[0].er,0x169fc);break;
case S::CompareScanPatch:f.call(0x571e);break;
case S::RestoreSavedPatch:f.pop(r[0].er);break;
case S::RestoreSavedProgram:f.pop(r[1].r);break;
case S::ReturnActive:f.ret();break;
case S::ReadCompareProgram:f.move(r[2].r,task.program());break;
case S::CompareStandardBank:h.sub8(r[2].rh,0x4);break;
case S::ResolveStandardPatch:f.call(0x63a4);break;
case S::ReadStandardPatch:f.move(r[1].er,task.target());break;
case S::BindStandardLength:f.move(r[3].e,0xb3);break;
case S::ReadStandardSource:f.move(r[3].rl,task.sourceByte());r[0].er+=1;h.clockI(2);break;
case S::ReadStandardTarget:f.move(r[3].rh,task.targetByte());r[1].er+=1;h.clockI(2);break;
case S::CompareStandardByte:h.sub8(r[3].rh,r[3].rl);break;
case S::TickStandardLength:f.inc(r[3].e,-1);break;
case S::SelectStandardMatch:h.clockI(2);break;
case S::JoinStandardMismatch:h.clockI(2);break;
case S::CompareFxProgram:h.sub8(r[2].rl,0x31);break;
case S::ResolveSplitPatch:f.call(0x63a4);break;
case S::ReadSplitPatch:f.move(r[1].er,task.target());break;
case S::BindPrefixLength:f.move(r[3].e,0x54);break;
case S::ReadPrefixSource:f.move(r[3].rl,task.sourceByte());r[0].er+=1;h.clockI(2);break;
case S::ReadPrefixTarget:f.move(r[3].rh,task.targetByte());r[1].er+=1;h.clockI(2);break;
case S::ComparePrefixByte:h.sub8(r[3].rh,r[3].rl);break;
case S::TickPrefixLength:f.inc(r[3].e,-1);break;
case S::SkipSourcePrefix:r[0].er=h.add32(r[0].er,0x29);break;
case S::SkipTargetPrefix:r[1].er=h.add32(r[1].er,0x29);break;
case S::ReadHeaderSource:f.move(r[3].rl,task.sourceByte());break;
case S::ReadHeaderTarget:f.move(r[3].rh,task.targetByte());break;
case S::CompareHeaderByte:h.sub8(r[3].rl,r[3].rh);break;
case S::SkipSourceHeader:r[0].er=int32_t(uint32_t(r[0].er)+0x2);break;
case S::SkipTargetHeader:r[1].er=int32_t(uint32_t(r[1].er)+0x2);break;
case S::BindSuffixLength:f.move(r[3].e,0x52);break;
case S::ReadSuffixSource:f.move(r[3].rl,task.sourceByte());r[0].er+=1;h.clockI(2);break;
case S::ReadSuffixTarget:f.move(r[3].rh,task.targetByte());r[1].er+=1;h.clockI(2);break;
case S::CompareSuffixByte:h.sub8(r[3].rh,r[3].rl);break;
case S::TickSuffixLength:f.inc(r[3].e,-1);break;
case S::ResolveFxPatch:f.call(0x63a4);break;
case S::ReadFxPatch:f.move(r[1].er,task.target());break;
case S::SkipSourceFx:r[0].er=h.add32(r[0].er,0x54);break;
case S::SkipTargetFx:r[1].er=h.add32(r[1].er,0x54);break;
case S::BindFxLength:f.move(r[3].e,0x29);break;
case S::ReadFxSource:f.move(r[3].rl,task.sourceByte());r[0].er+=1;h.clockI(2);break;
case S::ReadFxTarget:f.move(r[3].rh,task.targetByte());r[1].er+=1;h.clockI(2);break;
case S::CompareFxByte:h.sub8(r[3].rh,r[3].rl);break;
case S::TickFxLength:f.inc(r[3].e,-1);break;
case S::SkipTargetTail:r[1].er=int32_t(uint32_t(r[1].er)+0x1);break;
case S::SkipSourceTail:r[0].er=int32_t(uint32_t(r[0].er)+0x1);break;
case S::ReadTailSource:f.move(r[3].rl,task.sourceByte());break;
case S::ReadTailTarget:f.move(r[3].rh,task.targetByte());break;
case S::CompareTailByte:h.sub8(r[3].rl,r[3].rh);break;
case S::BindChangedMarker:f.move(r[6].rl,0x2a);break;
case S::ShowChangedMarker:f.call(0x976);break;
case S::ReadCompareMarker:f.move(r[6].rl,task.value());break;
case S::ShowCompareMarker:f.call(0x976);break;
case S::ReturnCompare:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateStatusDisplay clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
