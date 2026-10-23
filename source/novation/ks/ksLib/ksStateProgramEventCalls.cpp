#include "ksStateProgramEventCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateProgramEventPartialTest partialTest=nullptr;using T=StateProgramEventTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,49> points{{{0x61ec,2},{0x61ee,4},{0x61f2,4},{0x61f6,2},{0x61f8,2},{0x61fa,2},{0x61fc,2},{0x61fe,2},{0x6200,2},{0x6202,6},{0x6208,2},{0x620a,6},{0x6210,2},{0x6212,2},{0x6214,2},{0x6216,4},{0x621a,4},{0x621e,2},{0x6220,2},{0x6222,6},{0x6228,4},{0x622c,6},{0x6232,2},{0x6234,2},{0x6236,2},{0x6238,2},{0x623a,2},{0x623c,2},{0x623e,2},{0x6240,2},{0x6242,2},{0x6244,2},{0x6246,2},{0x6248,4},{0x624c,6},{0x6252,6},{0x6258,2},{0x625a,6},{0x6260,2},{0x6262,6},{0x6268,6},{0x626e,4},{0x6272,2},{0x6274,6},{0x627a,2},{0x627c,6},{0x6282,4},{0x6286,4},{0x628a,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned exitAddress(T::Exit e){switch(e){case T::Exit::Multi:return 0x62b2;default:throw std::runtime_error("Missing StateProgramEvent exit");}}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateProgramEventPartialForTests(StateProgramEventPartialTest value){partialTest=value;}
bool StateProgramEventCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x61ec:kind=T::Kind::Event;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateProgramEvent capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[5].r));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint32_t(r[0].er));const auto stage=task.stage();
 auto accessCost=[&](uint32_t ref,unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){const unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};unsigned store=3;switch(stage){case S::ReadEventBank:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::ReadPreviousMode:store=accessCost(uint32_t(0x167ba),1,-1);break;case S::WriteSingleMode:store=accessCost(uint32_t(0x167ba),1,h.lastwrite);break;case S::WriteActiveValue:store=accessCost(uint32_t(0x17526),1,h.lastwrite);break;case S::WriteTargetBank:store=accessCost(uint32_t(r[0].er),1,h.lastwrite);break;case S::WriteTargetIndex:store=accessCost(uint32_t(r[0].er+1),1,h.lastwrite);break;case S::WriteCurrentBank:store=accessCost(uint32_t(0x167ac),1,h.lastwrite);break;case S::WriteCurrentIndex:store=accessCost(uint32_t(0x167ad),1,h.lastwrite);break;case S::ReadSavedPart:store=accessCost(uint32_t(0x16798),1,-1);break;case S::ReadActivePart:store=accessCost(uint32_t(0x17526),1,-1);break;case S::WriteActivePart:store=accessCost(uint32_t(0x16798),1,h.lastwrite);break;case S::WriteSavedPart:store=accessCost(uint32_t(0x16798),1,h.lastwrite);break;case S::WriteReloadFlag:store=accessCost(uint32_t(0x167bb),1,h.lastwrite);break;default:break;} const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{return uint8_t(h.read8(field==T::Read::Target?ref:field==T::Read::Mode?0x167ba:field==T::Read::Part?0x16798:0x17526));};auto write=[&](T::Write field,uint32_t ref,uint8_t value){f.store8(field==T::Write::Target?ref:field==T::Write::Mode?0x167ba:field==T::Write::Active?0x17526:field==T::Write::CurrentBank?0x167ac:field==T::Write::CurrentIndex?0x167ad:field==T::Write::Part?0x16798:0x167bb,int8_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateProgramEvent admission differs");
 switch(stage){case S::TestMultiEvent:flag(h,4,(uint8_t(r[5].rh)&(1u<<0x6))==0);break;
case S::SelectMultiEvent:h.clockI(2);if(task.complete())f.next=exitAddress(task.exit());break;
case S::ResolveEventTarget:f.call(0x628c);break;
case S::ReadEventBank:f.move(r[1].rl,task.previousBank());break;
case S::TestEventSign:f.move(r[5].rh,r[5].rh);break;
case S::TestSingleEvent:flag(h,4,(uint8_t(r[5].rh)&(1u<<0x5))==0);break;
case S::MaskSingleBank:r[5].rh&=0x1f;value(h,r[5].rh);break;
case S::ReadPreviousMode:f.move(r[0].rh,task.mode());break;
case S::ClearSingleMode:f.move(r[0].rl,0x0);break;
case S::ComparePreviousMode:h.sub8(r[0].rh,r[0].rl);break;
case S::SaveSingleEvent:f.push(r[5].r);break;
case S::ResetSingleVoices:f.call(0x466e);break;
case S::ResetSingleUi:f.call(0x48dc);break;
case S::RestoreSingleEvent:f.pop(r[5].r);break;
case S::ClearActiveValue:f.move(r[0].rl,0x0);break;
case S::RefreshSingleRouting:f.call(0x6ec6);break;
case S::BindSingleTarget:f.move(r[0].er,0x167ae);break;
case S::BindEventBank:f.move(r[1].rl,r[5].rh);break;
case S::BindProgramLimit:f.move(r[1].rh,0x64);break;
case S::CompareProgramBank:h.sub8(r[1].rl,0x4);break;
case S::CompareAlternateBank:h.sub8(r[1].rl,0x8);break;
case S::BindAlternateLimit:f.move(r[1].rh,0x32);break;
case S::CompareProgramIndex:h.sub8(r[5].rl,r[1].rh);break;
case S::NotifyProgramChange:f.call(0x629e);break;
case S::ReadSavedPart:f.move(r[0].rl,task.savedPart());break;
case S::SaveSavedPart:f.push(r[0].r);break;
case S::ReadActivePart:f.move(r[0].rl,task.activePart());break;
case S::LoadProgram:f.call(0x6382);break;
case S::RestoreSavedPart:f.pop(r[0].r);break;
case S::BindReloadFlag:f.move(r[0].rl,0xffffffff);break;
case S::RefreshRouting:f.call(0x6ec6);break;
case S::RefreshScreen:f.call(0x5340);break;
case S::ReturnProgramEvent:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateProgramEvent clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
