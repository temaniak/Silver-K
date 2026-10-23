#include "ksStateBankNavigationCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateBankNavigationPartialTest partialTest=nullptr;using T=StateBankNavigationTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,201> points{{{0x5340,2},{0x5342,6},{0x5348,2},{0x534a,4},{0x534e,6},{0x5354,4},{0x5358,2},{0x535a,6},{0x5360,4},{0x5364,6},{0x536a,4},{0x536e,6},{0x5374,2},{0x5376,6},{0x537c,2},{0x537e,6},{0x5384,2},{0x5386,2},{0x5388,4},{0x538c,4},{0x5390,6},{0x5396,2},{0x5398,4},{0x539c,2},{0x539e,4},{0x53a2,4},{0x53a6,2},{0x53a8,2},{0x53aa,2},{0x53ac,6},{0x53b2,2},{0x53b4,4},{0x53b8,2},{0x53ba,2},{0x53bc,2},{0x53be,6},{0x53c4,2},{0x53c6,2},{0x53c8,2},{0x53ca,2},{0x53cc,2},{0x53ce,2},{0x53d0,6},{0x53d6,2},{0x53d8,2},{0x53da,6},{0x53e0,2},{0x53e2,2},{0x53e4,4},{0x53e8,6},{0x53ee,2},{0x53f0,2},{0x53f2,2},{0x53f4,2},{0x53f6,6},{0x53fc,6},{0x5402,2},{0x5404,6},{0x540a,4},{0x540e,2},{0x5410,2},{0x5412,2},{0x5414,2},{0x5416,2},{0x5418,2},{0x541a,2},{0x541c,2},{0x541e,6},{0x5424,2},{0x5426,6},{0x542c,6},{0x5432,2},{0x5434,6},{0x543a,2},{0x543c,4},{0x5440,6},{0x5446,4},{0x544a,6},{0x5450,2},{0x5452,4},{0x5456,2},{0x5458,4},{0x545c,2},{0x545e,4},{0x5462,2},{0x5464,2},{0x5466,2},{0x5468,6},{0x546e,2},{0x5470,2},{0x5472,4},{0x5476,6},{0x547c,2},{0x547e,2},{0x5480,6},{0x5486,2},{0x5488,2},{0x548a,4},{0x548e,2},{0x5490,4},{0x5494,6},{0x549a,2},{0x549c,4},{0x54a0,2},{0x54a2,4},{0x54a6,2},{0x54a8,4},{0x54ac,2},{0x54ae,6},{0x54b4,2},{0x54b6,4},{0x54ba,6},{0x54c0,4},{0x54c4,2},{0x54c6,6},{0x54cc,2},{0x54ce,6},{0x54d4,2},{0x54d6,6},{0x54dc,2},{0x54de,2},{0x54e0,6},{0x54e6,4},{0x54ea,2},{0x54ec,4},{0x54f0,4},{0x54f4,4},{0x54f8,2},{0x54fa,4},{0x54fe,6},{0x5504,2},{0x5506,2},{0x5508,6},{0x550e,2},{0x5510,4},{0x5514,2},{0x5516,2},{0x5518,2},{0x551a,2},{0x551c,4},{0x5520,2},{0x5522,4},{0x5526,2},{0x5528,2},{0x552a,2},{0x552c,4},{0x5530,2},{0x5532,4},{0x5536,2},{0x5538,4},{0x553c,2},{0x553e,4},{0x5542,2},{0x5544,2},{0x5546,2},{0x5548,2},{0x554a,2},{0x554c,4},{0x5550,2},{0x5552,2},{0x5554,2},{0x5556,6},{0x555c,2},{0x555e,2},{0x5560,2},{0x5562,2},{0x5564,2},{0x5566,2},{0x5568,2},{0x556a,2},{0x556c,2},{0x556e,2},{0x5570,2},{0x5572,2},{0x5574,2},{0x5576,6},{0x557c,2},{0x557e,8},{0x5586,2},{0x5588,6},{0x558e,6},{0x5594,4},{0x5598,4},{0x559c,2},{0x559e,2},{0x55a0,6},{0x55a6,2},{0x55a8,8},{0x55b0,6},{0x55b6,2},{0x55b8,4},{0x55bc,2},{0x55be,6},{0x55c4,4},{0x55c8,6},{0x55ce,4},{0x55d2,2},{0x55d4,2},{0x55d6,6},{0x55dc,4},{0x55e0,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::Special:return 0x73e2;case T::Exit::EditLatch:return 0x4ea4;case T::Exit::Edit:return 0x5932;case T::Exit::Refresh:return 0x5340;case T::Exit::Character:return 0x976;default:throw std::runtime_error("Missing StateBankNavigation exit");}}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateBankNavigationPartialForTests(StateBankNavigationPartialTest value){partialTest=value;}
bool StateBankNavigationCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x5340:kind=T::Kind::Refresh;break;case 0x53a8:kind=T::Kind::BankEntry;break;case 0x5470:kind=T::Kind::Bank;break;case 0x54ce:kind=T::Kind::Heading;break;case 0x54fe:kind=T::Kind::Parts;break;case 0x5552:kind=T::Kind::Step;break;case 0x559c:kind=T::Kind::Return;break;case 0x559e:kind=T::Kind::Resolve;break;case 0x55b8:kind=T::Kind::Notice;break;case 0x55d4:kind=T::Kind::ClearNotice;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateBankNavigation capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[6].r));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint32_t(r[1].er));const auto stage=task.stage();
 auto accessCost=[&](uint32_t ref,unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){const unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};unsigned store=3;switch(stage){case S::WriteRefreshFlag:store=accessCost(uint32_t(0x1679c),1,h.lastwrite);break;case S::ReadRefreshSpecial:store=accessCost(uint32_t(0x1679e),1,-1);break;case S::ReadRefreshLatch:store=accessCost(uint32_t(0x16792),1,-1);break;case S::ReadRefreshEdit:store=accessCost(uint32_t(0x16784),1,-1);break;case S::ReadRefreshMode:store=accessCost(uint32_t(0x167ba),1,-1);break;case S::ReadRefreshPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadRefreshPart:store=accessCost(uint32_t(0x16799),1,-1);break;case S::ReadPerformancePage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadBankLatch:store=accessCost(uint32_t(0x167fb),1,-1);break;case S::ReadBankNumber:store=accessCost(uint32_t(0x167ab),1,-1);break;case S::WriteBankNumber:store=accessCost(uint32_t(0x167ab),1,h.lastwrite);break;case S::ReadDigitLatch:store=accessCost(uint32_t(0x167fb),1,-1);break;case S::ReadSecondDigit:store=accessCost(uint32_t(0x167f9),1,-1);break;case S::WriteEnteredNumber:store=accessCost(uint32_t(0x167ab),1,h.lastwrite);break;case S::ReadFirstDigit:store=accessCost(uint32_t(0x167f8),1,-1);break;case S::WriteEnteredBank:store=accessCost(uint32_t(0x167aa),1,h.lastwrite);break;case S::WriteFirstDigit:store=accessCost(uint32_t(0x167f8),1,h.lastwrite);break;case S::WriteSecondBlank:store=accessCost(uint32_t(0x167f9),1,h.lastwrite);break;case S::WriteThirdBlank:store=accessCost(uint32_t(0x167fa),1,h.lastwrite);break;case S::WriteDigitLatch:store=accessCost(uint32_t(0x167fb),1,h.lastwrite);break;case S::ReadFirstCharacter:store=accessCost(uint32_t(r[5].er),1,-1);break;case S::ReadSecondCharacter:store=accessCost(uint32_t(r[5].er),1,-1);break;case S::ReadThirdCharacter:store=accessCost(uint32_t(r[5].er),1,-1);break;case S::WriteSecondDigit:store=accessCost(uint32_t(0x167f9),1,h.lastwrite);break;case S::ReadBankPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadNameSlot:store=accessCost(uint32_t(0x167d7),1,-1);break;case S::ReadBankDigit:store=accessCost(uint32_t(0x167aa),1,-1);break;case S::ReadBankNumberDisplay:store=accessCost(uint32_t(0x167ab),1,-1);break;case S::WriteBankDisplayLatch:store=accessCost(uint32_t(0x16792),1,h.lastwrite);break;case S::ReadHeadingPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadPartMask:store=accessCost(uint32_t(0x167d3),1,-1);break;case S::ReadPartBank:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadPartNumber:store=accessCost(uint32_t(r[1].er),1,-1);break;case S::ReadSelectedPart:store=accessCost(uint32_t(0x16799),1,-1);break;case S::WriteSelectedPart:store=accessCost(uint32_t(0x16799),1,h.lastwrite);break;case S::ReadPartEnabled:store=accessCost(uint32_t(r[1].er+65957),1,-1);break;case S::WriteCurrentPart:store=accessCost(uint32_t(0x167b8),1,h.lastwrite);break;case S::WriteRequestedPart:store=accessCost(uint32_t(0x167d0),1,h.lastwrite);break;case S::ReadPartIndex:store=accessCost(uint32_t(0x16799),1,-1);break;case S::ReadPartMarker:store=accessCost(uint32_t(r[1].er+65956),1,-1);break;case S::WritePartMarker:store=accessCost(uint32_t(0x167d6),1,h.lastwrite);break;case S::WriteNoticeFlag:store=accessCost(uint32_t(0x16791),1,h.lastwrite);break;case S::WriteClearedNotice:store=accessCost(uint32_t(0x16791),1,h.lastwrite);break;default:break;} const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{switch(field){case T::Read::Refresh:return uint8_t(h.read8(0x1679c));case T::Read::Special:return uint8_t(h.read8(0x1679e));case T::Read::EditLatch:return uint8_t(h.read8(0x16792));case T::Read::Edit:return uint8_t(h.read8(0x16784));case T::Read::Mode:return uint8_t(h.read8(0x167ba));case T::Read::Page:return uint8_t(h.read8(0x16790));case T::Read::Part:return uint8_t(h.read8(0x16799));case T::Read::DigitLatch:return uint8_t(h.read8(0x167fb));case T::Read::Number:return uint8_t(h.read8(0x167ab));case T::Read::Bank:return uint8_t(h.read8(0x167aa));case T::Read::FirstDigit:return uint8_t(h.read8(0x167f8));case T::Read::SecondDigit:return uint8_t(h.read8(0x167f9));case T::Read::ThirdDigit:return uint8_t(h.read8(0x167fa));case T::Read::Slot:return uint8_t(h.read8(0x167d7));case T::Read::PartMask:return uint8_t(h.read8(0x167d3));case T::Read::PartValue:case T::Read::Enabled:return uint8_t(h.read8(ref));}return 0;};auto write=[&](T::Write field,uint32_t,uint8_t value){switch(field){case T::Write::Refresh:f.store8(0x1679c,int8_t(value));break;case T::Write::EditLatch:f.store8(0x16792,int8_t(value));break;case T::Write::Part:f.store8(0x16799,int8_t(value));break;case T::Write::DigitLatch:f.store8(0x167fb,int8_t(value));break;case T::Write::Number:f.store8(0x167ab,int8_t(value));break;case T::Write::Bank:f.store8(0x167aa,int8_t(value));break;case T::Write::FirstDigit:f.store8(0x167f8,int8_t(value));break;case T::Write::SecondDigit:f.store8(0x167f9,int8_t(value));break;case T::Write::ThirdDigit:f.store8(0x167fa,int8_t(value));break;case T::Write::CurrentPart:f.store8(0x167b8,int8_t(value));break;case T::Write::RequestedPart:f.store8(0x167d0,int8_t(value));break;case T::Write::Marker:f.store8(0x167d6,int8_t(value));break;case T::Write::Notice:f.store8(0x16791,int8_t(value));break;}};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateBankNavigation admission differs");
 switch(stage){case S::BindRefreshFlag:f.move(r[0].rl,0x1);break;
case S::BindRefreshPosition:f.move(r[6].rl,0xfffffffb);break;
case S::ShowRefreshPosition:f.call(0x976);break;
case S::ReadRefreshSpecial:f.move(r[0].rl,task.value());break;
case S::SelectRefreshSpecial:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ClearRefreshDelta:f.move(r[6].rh,0x0);break;
case S::ReadRefreshLatch:f.move(r[0].rl,task.value());break;
case S::SelectRefreshLatch:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadRefreshEdit:f.move(r[0].rl,task.value());break;
case S::SelectRefreshEdit:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadRefreshMode:f.move(r[0].rl,task.value());break;
case S::ReadRefreshPage:f.move(r[0].rl,task.page());break;
case S::ReadRefreshPart:f.move(r[0].rl,task.value());break;
case S::BindPerformancePosition:f.move(r[6].rl,0xfffffffc);break;
case S::ShowPerformancePosition:f.call(0x976);break;
case S::ShowBankScreen:f.call(0x5470);break;
case S::ReadPerformancePage:f.move(r[0].rl,task.page());break;
case S::ShowPerformanceParts:f.call(0x54fe);break;
case S::ReturnPerformance:f.ret();break;
case S::ShowProgramCategory:f.call(0x6166);break;
case S::ShowProgramScreen:f.call(0x6094);break;
case S::ReturnProgramScreen:f.ret();break;
case S::TestBankDelta:f.move(r[6].rh,r[6].rh);break;
case S::ReadBankLatch:f.move(r[0].rl,task.latch());break;
case S::ClearBankLatch:f.call(0x601a);break;
case S::ReturnBankLatch:f.move(r[0].rl,0x1);break;
case S::ReturnBankLatchValue:f.ret();break;
case S::BindBankLimit:f.move(r[6].rl,0x64);break;
case S::ReadBankNumber:f.move(r[0].rl,task.value());break;
case S::AddBankDelta:r[0].rl=h.add8(r[0].rl,r[6].rh);break;
case S::AddBankLimit:r[0].rl=h.add8(r[0].rl,r[6].rl);break;
case S::CompareBankLimit:h.sub8(r[0].rl,r[6].rl);break;
case S::SubtractBankLimit:r[0].rl=h.sub8(r[0].rl,r[6].rl);break;
case S::BindBankChanged:f.move(r[0].rl,0x3);break;
case S::ReturnBankChanged:f.ret();break;
case S::ReadDigitLatch:f.move(r[0].rl,task.latch());break;
case S::CompareDigitLatch:h.sub8(r[0].rl,0x1);break;
case S::SelectSecondDigit:h.clockI(2);break;
case S::ReadSecondDigit:f.move(r[0].rl,task.value());break;
case S::MaskSecondDigit:r[0].rl&=0xf;value(h,r[0].rl);break;
case S::BindDigitStride:f.move(r[0].rh,0xa);break;
case S::ScaleSecondDigit:r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;
case S::AddThirdDigit:r[0].rl=h.add8(r[0].rl,r[6].rl);break;
case S::ReadFirstDigit:f.move(r[0].rl,task.value());break;
case S::MaskFirstDigit:r[0].rl&=0xf;value(h,r[0].rl);break;
case S::ClearDigitLatch:f.call(0x601a);break;
case S::BindEntryChanged:f.move(r[0].rl,0x3);break;
case S::ReturnEntryChanged:f.ret();break;
case S::TestFirstDigit:f.move(r[6].rl,r[6].rl);break;
case S::BindFirstDigitLimit:f.move(r[6].rh,0x1);break;
case S::CompareFirstDigit:h.sub8(r[6].rl,r[6].rh);break;
case S::AddFirstDigitAscii:r[6].rl=h.add8(r[6].rl,0x30);break;
case S::BindBlankDigit:f.move(r[6].rl,0x5f);break;
case S::TickDigitLatch:f.inc(r[0].rl,1);break;
case S::BindEntryPosition:f.move(r[6].rl,0xffffff80);break;
case S::ShowEntryPosition:f.call(0x976);break;
case S::BindEntryHeading:f.move(r[6].er,0x14e58);break;
case S::ShowEntryHeading:f.call(0x54ce);break;
case S::BindEntryDigits:f.move(r[5].er,0x167f8);break;
case S::ReadFirstCharacter:f.move(r[6].rl,task.value());r[5].er+=1;h.clockI(2);break;
case S::ShowFirstCharacter:f.call(0x976);break;
case S::ReadSecondCharacter:f.move(r[6].rl,task.value());r[5].er+=1;h.clockI(2);break;
case S::ShowSecondCharacter:f.call(0x976);break;
case S::ReadThirdCharacter:f.move(r[6].rl,task.value());r[5].er+=1;h.clockI(2);break;
case S::ShowThirdCharacter:f.call(0x976);break;
case S::ClearEntryResult:f.move(r[0].rl,0x0);break;
case S::ReturnEntryResult:f.ret();break;
case S::AddSecondDigitAscii:r[6].rl=h.add8(r[6].rl,0x30);break;
case S::BindBankPosition:f.move(r[6].rl,0xffffff80);break;
case S::ShowBankPosition:f.call(0x976);break;
case S::ReadBankPage:f.move(r[0].rl,task.page());break;
case S::CompareBankPage:h.sub8(r[0].rl,0x2);break;
case S::BindMultiName:f.move(r[6].er,0x1733c);break;
case S::ShowMultiName:f.call(0x54ce);break;
case S::BindNameQuote:f.move(r[6].rl,0x27);break;
case S::ShowNameQuote:f.call(0x976);break;
case S::BindNameSpace:f.move(r[6].rl,0x20);break;
case S::ShowNameSpace:f.call(0x976);break;
case S::ReadNameSlot:f.move(r[6].rl,task.value());break;
case S::AddNameSlot:r[6].rl=h.add8(r[6].rl,0xffffffc1);break;
case S::ShowNameSlot:f.call(0x976);break;
case S::BindNameEnd:f.move(r[6].rl,0xfffffffc);break;
case S::ShowNameEnd:f.call(0x976);break;
case S::ResolveBankName:f.call(0x6e52);break;
case S::ShowBankName:f.call(0x54ce);break;
case S::ReadBankDigit:f.move(r[6].rl,task.value());break;
case S::AddBankDigit:r[6].rl=h.add8(r[6].rl,0x30);break;
case S::ShowBankDigit:f.call(0x976);break;
case S::ReadBankNumberDisplay:f.move(r[0].rl,task.value());break;
case S::ShowBankNumber:f.call(0x9dc);break;
case S::ClearBankDisplayLatch:f.move(r[0].rl,0x0);break;
case S::ReturnBankDisplay:f.ret();break;
case S::ReadHeadingPage:f.move(r[0].rl,task.page());break;
case S::BindSingleHeading:f.move(r[1].er,0x14e69);break;
case S::TickHeadingPage:f.inc(r[0].rl,-1);break;
case S::BindPerformanceHeading:f.move(r[1].er,0x14e7f);break;
case S::SaveHeadingValue:f.push(r[6].er);break;
case S::BindHeadingValue:f.move(r[6].er,r[1].er);break;
case S::ShowHeadingValue:f.call(0x9b8);break;
case S::RestoreHeadingValue:f.pop(r[6].er);break;
case S::ShowHeadingText:f.call(0x9b8);break;
case S::BindHeadingSpace:f.move(r[6].rl,0x20);break;
case S::ShowHeadingSpace:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::BindPartsTable:f.move(r[1].er,0x167b0);break;
case S::BindPartPosition:f.move(r[6].rl,0xffffffc1);break;
case S::BindPartCount:f.move(r[2].rl,0x4);break;
case S::ReadPartMask:f.move(r[2].rh,task.mask());break;
case S::SavePartPosition:f.push(r[6].r);break;
case S::ShowPartPosition:f.call(0x976);break;
case S::ShiftPartMask:f.shift(r[2].rh,1,false);break;
case S::ReadPartBank:f.move(r[6].rl,task.value());r[1].er+=1;h.clockI(2);break;
case S::AddPartBank:r[6].rl=h.add8(r[6].rl,0x30);break;
case S::ShowPartBank:f.call(0x976);break;
case S::ReadPartNumber:f.move(r[0].rl,task.value());r[1].er+=1;h.clockI(2);break;
case S::ShowPartNumber:f.call(0x9dc);break;
case S::SkipPartNumber:r[1].er=int32_t(uint32_t(r[1].er)+0x2);break;
case S::BindMissingBank:f.move(r[6].rl,0x2d);break;
case S::ShowMissingBank:f.call(0x976);break;
case S::BindMissingTens:f.move(r[6].rl,0x2d);break;
case S::ShowMissingTens:f.call(0x976);break;
case S::BindMissingUnits:f.move(r[6].rl,0x2d);break;
case S::ShowMissingUnits:f.call(0x976);break;
case S::BindPartSeparator:f.move(r[6].rl,0x20);break;
case S::ShowPartSeparator:f.call(0x976);break;
case S::RestorePartPosition:f.pop(r[6].r);break;
case S::NextPartPosition:r[6].rl=h.add8(r[6].rl,0x5);break;
case S::TickPartCount:f.inc(r[2].rl,-1);break;
case S::BindPartsEnd:f.move(r[6].rl,0x20);break;
case S::ShowPartsEnd:f.call(0x976);break;
case S::ReturnParts:f.ret();break;
case S::TestPartDelta:f.move(r[6].rl,r[6].rl);break;
case S::ReadSelectedPart:f.move(r[0].rl,task.value());break;
case S::BindFirstPart:f.move(r[0].rl,0x1);break;
case S::TestPartDirection:f.move(r[6].rl,r[6].rl);break;
case S::BindLastPart:f.move(r[0].rl,0x4);break;
case S::AddPartDelta:r[0].rl=h.add8(r[0].rl,r[6].rl);break;
case S::BindZeroPart:f.move(r[0].rl,0x4);break;
case S::ComparePartLimit:h.sub8(r[0].rl,0x4);break;
case S::ClampPart:f.move(r[0].rl,0x1);break;
case S::ResolveSelectedPart:f.call(0x559e);break;
case S::ReadPartEnabled:f.move(r[0].rl,task.value());break;
case S::ApplyPartRouting:f.call(0x6ec6);break;
case S::RefreshPart:h.clockI(2);break;
case S::ReturnPart:f.ret();break;
case S::ClearPartIndex:r[1].er=h.sub32(r[1].er,r[1].er);break;
case S::ReadPartIndex:f.move(r[1].rl,task.value());break;
case S::DoublePartIndex:f.shift(r[1].rl,1,true);break;
case S::ReadPartMarker:f.move(r[0].rl,task.value());break;
case S::ReturnPartMarker:f.ret();break;
case S::ShowNotice:f.call(0x9b8);break;
case S::BindNoticeFlag:f.move(r[6].rl,0x1);break;
case S::BindNoticeDelay:f.move(r[6].r,0x2ee);break;
case S::BindNoticeCallback:f.move(r[5].er,0x55d4);break;
case S::ScheduleNotice:f.call(0x407c);break;
case S::ReturnNotice:f.ret();break;
case S::ClearNoticeFlag:f.move(r[0].rl,0x0);break;
case S::RefreshNotice:f.call(0x5340);break;
case S::ReturnClearedNotice:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateBankNavigation clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
