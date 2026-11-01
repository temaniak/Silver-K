#include "ksStateProgramDisplayCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateProgramDisplayPartialTest partialTest=nullptr;using T=StateProgramDisplayTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,79> points{{{0x6094,2},{0x6096,4},{0x609a,6},{0x60a0,2},{0x60a2,2},{0x60a4,2},{0x60a6,6},{0x60ac,2},{0x60ae,2},{0x60b0,4},{0x60b4,6},{0x60ba,2},{0x60bc,4},{0x60c0,2},{0x60c2,4},{0x60c6,2},{0x60c8,6},{0x60ce,4},{0x60d2,2},{0x60d4,4},{0x60d8,2},{0x60da,6},{0x60e0,2},{0x60e2,4},{0x60e6,6},{0x60ec,2},{0x60ee,2},{0x60f0,2},{0x60f2,2},{0x60f4,2},{0x60f6,4},{0x60fa,2},{0x60fc,4},{0x6100,2},{0x6102,4},{0x6106,2},{0x6108,6},{0x610e,2},{0x6110,6},{0x6116,2},{0x6118,6},{0x611e,2},{0x6120,2},{0x6122,2},{0x6124,4},{0x6128,6},{0x612e,2},{0x6130,6},{0x6136,6},{0x613c,2},{0x613e,6},{0x6144,2},{0x6146,2},{0x6148,6},{0x614e,4},{0x6152,2},{0x6154,4},{0x6158,4},{0x615c,4},{0x6160,2},{0x6162,4},{0x6166,6},{0x616c,2},{0x616e,2},{0x6170,2},{0x6172,2},{0x6174,6},{0x617a,2},{0x617c,4},{0x6180,2},{0x6182,4},{0x6186,2},{0x6188,2},{0x618a,2},{0x618c,2},{0x618e,4},{0x6192,2},{0x6194,2},{0x6196,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::Character:return 0x976;default:throw std::runtime_error("Missing StateProgramDisplay exit");}}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateProgramDisplayPartialForTests(StateProgramDisplayPartialTest value){partialTest=value;}
bool StateProgramDisplayCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x6094:kind=T::Kind::Program;break;case 0x6110:kind=T::Kind::Heading;break;case 0x6166:kind=T::Kind::Category;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateProgramDisplay capacity exceeded");slot->emplace(*kind,h.getSP());++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();
 auto accessCost=[&](uint32_t ref,unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){const unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};unsigned store=3;switch(stage){case S::ReadProgramPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadNameSlot:store=accessCost(uint32_t(0x167d7),1,-1);break;case S::ReadProgramBank:store=accessCost(uint32_t(0x167ac),1,-1);break;case S::ReadProgramNumber:store=accessCost(uint32_t(0x167ac),2,-1);break;case S::WriteProgramLatch:store=accessCost(uint32_t(0x16792),1,h.lastwrite);break;case S::ReadHeadingPage:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadHeadingPart:store=accessCost(uint32_t(0x167d6),1,-1);break;case S::ReadHeadingPageAgain:store=accessCost(uint32_t(0x16790),1,-1);break;case S::ReadHeadingMode:store=accessCost(uint32_t(0x167ba),1,-1);break;case S::ReadCategory:store=accessCost(uint32_t(0x167da),1,-1);break;case S::ReadCategoryCharacter:store=accessCost(uint32_t(r[0].er),1,-1);break;default:break;} const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{if(field==T::Read::Program)return uint16_t(h.read16(0x167ac));return uint8_t(h.read8(field==T::Read::Page?0x16790:field==T::Read::Slot?0x167d7:field==T::Read::Bank?0x167ac:field==T::Read::Part?0x167d6:field==T::Read::Mode?0x167ba:field==T::Read::Category?0x167da:ref));};auto write=[&](T::Write,uint8_t v){f.store8(0x16792,int8_t(v));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateProgramDisplay admission differs");
 switch(stage){case S::BindProgramPosition:f.move(r[6].rl,0xffffff80);break;
case S::ShowProgramPosition:f.call(0x976);break;
case S::ReadProgramPage:f.move(r[0].rl,task.page());break;
case S::CompareProgramPage:h.sub8(r[0].rl,0x2);break;
case S::BindMultiName:f.move(r[6].er,0x1733c);break;
case S::ShowMultiName:f.call(0x6110);break;
case S::BindNameQuote:f.move(r[6].rl,0x27);break;
case S::ShowNameQuote:f.call(0x976);break;
case S::ReadNameSlot:f.move(r[6].rl,task.value());break;
case S::AddNameSlot:r[6].rl=h.add8(r[6].rl,0xffffffc1);break;
case S::ShowNameSlot:f.call(0x976);break;
case S::BindNameEnd:f.move(r[6].rl,0xfffffffc);break;
case S::ShowNameEnd:f.call(0x976);break;
case S::BindMultiNotice:f.move(r[6].er,0x14e0b);break;
case S::ShowMultiNotice:f.call(0x9b8);break;
case S::ResolveStoredName:f.call(0x6e42);break;
case S::ShowStoredName:f.call(0x6110);break;
case S::ReadProgramBank:f.move(r[6].rl,task.value());break;
case S::AddProgramBank:r[6].rl=h.add8(r[6].rl,0x30);break;
case S::ShowProgramBank:f.call(0x976);break;
case S::ReadProgramNumber:f.move(r[0].r,task.program());break;
case S::CompareProgramBank:h.sub8(r[0].rh,0x4);break;
case S::CompareFxNumber:h.sub8(r[0].rl,0x31);break;
case S::BindFxFirst:f.move(r[6].rl,0x46);break;
case S::ShowFxFirst:f.call(0x976);break;
case S::BindFxSecond:f.move(r[6].rl,0x58);break;
case S::ShowFxSecond:f.call(0x976);break;
case S::ShowProgramNumber:f.call(0x9dc);break;
case S::ClearProgramLatch:f.move(r[0].rl,0x0);break;
case S::ReturnProgramDisplay:f.ret();break;
case S::ReadHeadingPage:f.move(r[0].rl,task.page());break;
case S::ReadHeadingPart:f.move(r[0].rl,task.value());break;
case S::TestHeadingPart:h.sub8(r[0].rl,0xffffff91);break;
case S::BindHeadingPart:f.move(r[6].rl,r[0].rl);break;
case S::ShowHeadingPart:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadHeadingPageAgain:f.move(r[0].rl,task.page());break;
case S::BindSingleHeading:f.move(r[1].er,0x14de5);break;
case S::ReadHeadingMode:f.move(r[0].rh,task.mode());break;
case S::BindMultiHeading:f.move(r[1].er,0x14e30);break;
case S::TickHeadingPage:f.inc(r[0].rl,-1);break;
case S::BindPerformanceHeading:f.move(r[1].er,0x14dfb);break;
case S::SaveHeadingName:f.push(r[6].er);break;
case S::BindHeadingName:f.move(r[6].er,r[1].er);break;
case S::ShowHeadingName:f.call(0x9b8);break;
case S::RestoreHeadingName:f.pop(r[6].er);break;
case S::ShowHeadingValue:f.call(0x9b8);break;
case S::BindHeadingSpace:f.move(r[6].rl,0x20);break;
case S::ShowHeadingSpace:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ReadCategory:f.move(r[0].rl,task.category());break;
case S::BindCategoryLength:f.move(r[1].rl,0xa);break;
case S::BindCategoryWidth:f.move(r[1].rh,0x14);break;
case S::ScaleCategory:r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[1].rl));h.clockI(12);break;
case S::NormalizeCategory:r[0].er&=0xffff;value(h,r[0].er);break;
case S::LocateCategory:r[0].er=h.add32(r[0].er,0x14f74);break;
case S::BindCategoryPosition:f.move(r[6].rl,0xffffffc0);break;
case S::ShowCategoryPosition:f.call(0x976);break;
case S::ReadCategoryCharacter:f.move(r[6].rl,task.value());r[0].er+=1;h.clockI(2);break;
case S::ShowCategoryCharacter:f.call(0x976);break;
case S::TickCategoryWidth:f.inc(r[1].rh,-1);break;
case S::TickCategoryLength:f.inc(r[1].rl,-1);break;
case S::BindCategorySpace:f.move(r[6].rl,0x20);break;
case S::ShowCategorySpace:f.call(0x976);break;
case S::TickCategoryPadding:f.inc(r[1].rh,-1);break;
case S::ReturnCategory:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateProgramDisplay clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
