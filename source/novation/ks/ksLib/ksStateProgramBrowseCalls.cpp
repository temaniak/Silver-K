#include "ksStateProgramBrowseCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateProgramBrowsePartialTest partialTest=nullptr;using T=StateProgramBrowseTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,97> points{{{0x5f64,6},{0x5f6a,2},{0x5f6c,2},{0x5f6e,4},{0x5f72,2},{0x5f74,2},{0x5f76,2},{0x5f78,6},{0x5f7e,4},{0x5f82,2},{0x5f84,4},{0x5f88,2},{0x5f8a,6},{0x5f90,8},{0x5f98,2},{0x5f9a,2},{0x5f9c,2},{0x5f9e,2},{0x5fa0,2},{0x5fa2,2},{0x5fa4,2},{0x5fa6,2},{0x5fa8,2},{0x5faa,2},{0x5fac,2},{0x5fae,2},{0x5fb0,2},{0x5fb2,2},{0x5fb4,6},{0x5fba,2},{0x5fbc,2},{0x5fbe,2},{0x5fc0,2},{0x5fc2,4},{0x5fc6,6},{0x5fcc,4},{0x5fd0,2},{0x5fd2,2},{0x5fd4,4},{0x5fd8,4},{0x5fdc,2},{0x5fde,2},{0x5fe0,6},{0x5fe6,2},{0x5fe8,2},{0x5fea,4},{0x5fee,6},{0x5ff4,2},{0x5ff6,2},{0x5ff8,2},{0x5ffa,2},{0x5ffc,6},{0x6002,6},{0x6008,2},{0x600a,6},{0x6010,2},{0x6012,2},{0x6014,4},{0x6018,2},{0x601a,2},{0x601c,2},{0x601e,6},{0x6024,2},{0x6026,2},{0x6028,2},{0x602a,2},{0x602c,2},{0x602e,2},{0x6030,2},{0x6032,2},{0x6034,6},{0x603a,2},{0x603c,6},{0x6042,6},{0x6048,2},{0x604a,6},{0x6050,2},{0x6052,4},{0x6056,6},{0x605c,4},{0x6060,6},{0x6066,2},{0x6068,4},{0x606c,2},{0x606e,4},{0x6072,2},{0x6074,4},{0x6078,2},{0x607a,2},{0x607c,6},{0x6082,2},{0x6084,2},{0x6086,2},{0x6088,2},{0x608a,2},{0x608c,6},{0x6092,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateProgramBrowsePartialForTests(StateProgramBrowsePartialTest value){partialTest=value;}
bool StateProgramBrowseCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x5f64:kind=T::Kind::Category;break;case 0x5fa2:kind=T::Kind::Step;break;case 0x5fb0:kind=T::Kind::Entry;break;case 0x601a:kind=T::Kind::Clear;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateProgramBrowse capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[6].r),uint16_t(h.regs[0].r),uint16_t(h.regs[6].e));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint16_t(r[0].r));const auto stage=task.stage();
 auto accessCost=[&](uint32_t ref,unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){const unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};unsigned store=3;switch(stage){case S::ReadCategoryFilter:store=accessCost(uint32_t(0x167a1),1,-1);break;case S::ReadBrowseCategory:store=accessCost(uint32_t(0x167da),1,-1);break;case S::ReadPageCategory:store=accessCost(uint32_t(r[1].er+176),1,-1);break;case S::ReadProgramLatch:store=accessCost(uint32_t(0x167fb),1,-1);break;case S::ReadProgramDigitLatch:store=accessCost(uint32_t(0x167fb),1,-1);break;case S::ReadProgramSecond:store=accessCost(uint32_t(0x167f9),1,-1);break;case S::WriteProgramNumber:store=accessCost(uint32_t(0x167ad),1,h.lastwrite);break;case S::ReadProgramFirst:store=accessCost(uint32_t(0x167f8),1,-1);break;case S::WriteProgramBank:store=accessCost(uint32_t(0x167ac),1,h.lastwrite);break;case S::WriteDigitsValue:store=accessCost(uint32_t(0x167fb),1,h.lastwrite);break;case S::WriteProgramFirst:store=accessCost(uint32_t(0x167f8),1,h.lastwrite);break;case S::WriteProgramSecondBlank:store=accessCost(uint32_t(0x167f9),1,h.lastwrite);break;case S::WriteProgramThirdBlank:store=accessCost(uint32_t(0x167fa),1,h.lastwrite);break;case S::WriteProgramDigits:store=accessCost(uint32_t(0x167fb),1,h.lastwrite);break;case S::ReadProgramFirstCharacter:store=accessCost(uint32_t(r[5].er),1,-1);break;case S::ReadProgramSecondCharacter:store=accessCost(uint32_t(r[5].er),1,-1);break;case S::ReadProgramThirdCharacter:store=accessCost(uint32_t(r[5].er),1,-1);break;case S::ReadProgramFirstLimit:store=accessCost(uint32_t(0x167f8),1,-1);break;case S::WriteProgramSecond:store=accessCost(uint32_t(0x167f9),1,h.lastwrite);break;default:break;} const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{return uint8_t(h.read8(field==T::Read::Filter?0x167a1:field==T::Read::Category?0x167da:field==T::Read::Latch?0x167fb:field==T::Read::First?0x167f8:field==T::Read::Second?0x167f9:field==T::Read::Third?0x167fa:ref));};auto write=[&](T::Write field,uint8_t value){f.store8(field==T::Write::Bank?0x167ac:field==T::Write::Number?0x167ad:field==T::Write::Latch?0x167fb:field==T::Write::First?0x167f8:field==T::Write::Second?0x167f9:0x167fa,int8_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateProgramBrowse admission differs");
 switch(stage){case S::ReadCategoryFilter:f.move(r[1].rl,task.filter());break;
case S::RetainBrowseDirection:f.move(r[1].r,r[6].r);break;
case S::BindBrowseStep:f.move(r[6].r,0x1);break;
case S::TestBrowseDirection:f.move(r[1].r,r[1].r);break;
case S::NegateBrowseStep:r[6].r=h.sub16(0,r[6].r);break;
case S::ReadBrowseCategory:f.move(r[2].rl,task.category());break;
case S::BindBrowseAttempts:f.move(r[2].e,0x258);break;
case S::StepCategory:f.call(0x5fa2);break;
case S::BindCategoryPage:f.move(r[1].r,0x100);break;
case S::ScaleCategoryPage:r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[0].r));h.clockI(20);break;
case S::LocateCategoryPage:r[1].er=h.add32(r[1].er,0x820000);break;
case S::ReadPageCategory:f.move(r[1].rl,task.observed());break;
case S::ComparePageCategory:h.sub8(r[1].rl,r[2].rl);break;
case S::TickBrowseAttempts:f.inc(r[2].e,-1);break;
case S::ReturnCategoryBrowse:f.ret();break;
case S::AddBrowseStep:r[0].r=h.add16(r[0].r,r[6].r);break;
case S::AddBrowseLimit:r[0].r=h.add16(r[0].r,r[6].e);break;
case S::CompareBrowseLimit:h.sub16(r[0].r,r[6].e);break;
case S::SubtractBrowseLimit:r[0].r=h.sub16(r[0].r,r[6].e);break;
case S::ReturnBrowseStep:f.ret();break;
case S::TestProgramDelta:f.move(r[6].rh,r[6].rh);break;
case S::ReadProgramLatch:f.move(r[0].rl,task.latch());break;
case S::ClearProgramLatch:f.call(0x601a);break;
case S::BindLatchedResult:f.move(r[0].rl,0x1);break;
case S::ReturnLatchedResult:f.ret();break;
case S::BindProgramTotal:f.move(r[6].e,0x258);break;
case S::BindProgramTarget:f.move(r[0].er,0x167ac);break;
case S::ResolveProgramValue:f.call(0x6198);break;
case S::BindProgramDelta:f.move(r[6].rl,r[6].rh);break;
case S::SignProgramDelta:r[6].r=int8_t(r[6].r);value(h,r[6].r);break;
case S::BrowseCategory:f.call(0x5f64);break;
case S::ApplyProgramValue:f.call(0x61c2);break;
case S::BindProgramChanged:f.move(r[0].rl,0x3);break;
case S::ReturnProgramChanged:f.ret();break;
case S::ReadProgramDigitLatch:f.move(r[0].rl,task.latch());break;
case S::CompareProgramDigitLatch:h.sub8(r[0].rl,0x1);break;
case S::SelectProgramSecond:h.clockI(2);break;
case S::ReadProgramSecond:f.move(r[0].rl,task.observed());break;
case S::MaskProgramSecond:r[0].rl&=0xf;value(h,r[0].rl);break;
case S::BindProgramStride:f.move(r[0].rh,0xa);break;
case S::ScaleProgramSecond:r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;
case S::AddProgramThird:r[0].rl=h.add8(r[0].rl,r[6].rl);break;
case S::ReadProgramFirst:f.move(r[0].rl,task.observed());break;
case S::MaskProgramFirst:r[0].rl&=0xf;value(h,r[0].rl);break;
case S::ClearProgramDigits:f.call(0x601a);break;
case S::BindEnteredResult:f.move(r[0].rl,0x3);break;
case S::NotifyProgramEntry:f.call(0x6ec6);break;
case S::ReturnProgramEntry:f.ret();break;
case S::SaveClearDigits:f.push(r[0].r);break;
case S::ClearDigitsValue:r[0].rl=h.sub8(r[0].rl,r[0].rl);break;
case S::RestoreClearDigits:f.pop(r[0].r);break;
case S::ReturnClearDigits:f.ret();break;
case S::TestProgramFirst:f.move(r[6].rl,r[6].rl);break;
case S::BindProgramFirstLimit:f.move(r[6].rh,0x8);break;
case S::CompareProgramFirst:h.sub8(r[6].rl,r[6].rh);break;
case S::AddProgramFirstAscii:r[6].rl=h.add8(r[6].rl,0x30);break;
case S::BindProgramBlank:f.move(r[6].rl,0x5f);break;
case S::TickProgramDigits:f.inc(r[0].rl,1);break;
case S::BindProgramEntryPosition:f.move(r[6].rl,0xffffff80);break;
case S::ShowProgramEntryPosition:f.call(0x976);break;
case S::BindProgramEntryHeading:f.move(r[6].er,0x14dd4);break;
case S::ShowProgramEntryHeading:f.call(0x6110);break;
case S::BindProgramDigits:f.move(r[5].er,0x167f8);break;
case S::ReadProgramFirstCharacter:f.move(r[6].rl,task.observed());r[5].er+=1;h.clockI(2);break;
case S::ShowProgramFirstCharacter:f.call(0x976);break;
case S::ReadProgramSecondCharacter:f.move(r[6].rl,task.observed());r[5].er+=1;h.clockI(2);break;
case S::ShowProgramSecondCharacter:f.call(0x976);break;
case S::ReadProgramThirdCharacter:f.move(r[6].rl,task.observed());r[5].er+=1;h.clockI(2);break;
case S::ShowProgramThirdCharacter:f.call(0x976);break;
case S::ClearProgramEntryResult:f.move(r[0].rl,0x0);break;
case S::ReturnProgramEntryResult:f.ret();break;
case S::ReadProgramFirstLimit:f.move(r[0].rl,task.observed());break;
case S::CompareProgramFirstLimit:h.sub8(r[0].rl,0x34);break;
case S::CompareProgramSecondLimit:h.sub8(r[6].rl,0x4);break;
case S::AddProgramSecondAscii:r[6].rl=h.add8(r[6].rl,0x30);break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateProgramBrowse clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
