#include "ksStateMultiSaveCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateMultiSaveTask;using S=T::Stage;StateMultiSavePartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,43> points{{{0x71a6,2},{0x71a8,6},{0x71ae,6},{0x71b4,4},{0x71b8,2},{0x72e0,6},{0x72e6,8},{0x72ee,4},{0x72f2,2},{0x72f4,2},{0x72f6,2},{0x72f8,2},{0x72fa,2},{0x72fc,2},{0x72fe,4},{0x7302,2},{0x7304,6},{0x730a,2},{0x730c,2},{0x730e,2},{0x7310,4},{0x7314,2},{0x7316,6},{0x731c,4},{0x7320,2},{0x7322,2},{0x7324,2},{0x7326,2},{0x7328,2},{0x732a,4},{0x732e,2},{0x7330,2},{0x7332,2},{0x7334,2},{0x7336,2},{0x7338,6},{0x733e,4},{0x7342,2},{0x7344,4},{0x7348,6},{0x734e,2},{0x7350,2},{0x7352,2}}};Point point(const T& t){if(t.awaitingMessage())return{0x66d2,0};return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStateMultiSavePartialForTests(StateMultiSavePartialTest f){partialTest=f;}
bool StateMultiSaveCalls::retireReturns(ControllerCpu& h){bool retired=false;for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingMessage()&&h.getPC()==0x66d2){c->task.retireMessage();c.reset();++completed;retired=true;}return retired;}
bool StateMultiSaveCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x71a6||pc==0x72e0){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native multi-save capacity exceeded");selected->emplace(pc==0x71a6?T::Entry::Current:T::Entry::Provided,h.getSP(),uint32_t(h.regs[0].er),uint32_t(h.regs[1].er));++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();const unsigned physical=0x167fc+task.byteOffset();const unsigned storeClocks=(h.lastwrite&1)||int(physical)!=(h.lastwrite|1)?3:0;const unsigned programClocks=(call.program&0xffffff)<0x400000&&!(call.program&1)?3:6;const auto cost=task.nextClocks(storeClocks,programClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what)->uint32_t{return observed=what==T::Read::Enabled?uint8_t(h.read8(0x168fc+239)):what==T::Read::Program?uint16_t(h.read16(call.program)):uint8_t(h.read8(call.source+task.pageNumber()*256+task.byteOffset()));};auto write=[&](unsigned offset,uint8_t value){f.store8(0x167fc+offset,value);};if(task.advanceBefore(cost,read,write,storeClocks,programClocks)!=cost)throw std::runtime_error("Native multi-save admission differs");switch(stage){case S::SerializeCurrent:{f.call(0x71ba);break;}
case S::SetCurrentBuffer:{f.move(r[1].er,0x169fc);break;}
case S::SetCurrentProgram:{f.move(r[0].er,0x167aa);break;}
case S::StoreCurrent:{f.call(0x72e0);break;}
case S::ReturnCurrent:{f.ret();break;}
case S::SetGlobal:{f.move(r[2].er,0x168fc);break;}
case S::ReadEnabled:{f.move(r[2].rl,observed);break;}
case S::AdmitEnabled:{h.clockI(2);break;}
case S::ReadProgram:{f.move(r[0].r,observed);break;}
case S::CompareBank:{h.sub8(r[0].rh,0x1);break;}
case S::AdmitBank:{;break;}
case S::CompareProgram:{h.sub8(r[0].rl,0x63);break;}
case S::AdmitProgram:{;break;}
case S::ClearBank:{r[0].r&=0xff;value(h,r[0].r);break;}
case S::SetStride:{f.move(r[0].e,0x400);break;}
case S::ScaleProgram:{r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20);break;}
case S::AddBase:{r[0].er=h.add32(r[0].er,0x845800);break;}
case S::CaptureTarget:{f.move(r[6].er,r[0].er);break;}
case S::ClearTargetLow:{f.move(r[6].rl,0x0);break;}
case S::SetPageCount:{f.move(r[0].rl,0x4);break;}
case S::RetainSource:{f.push(r[1].er);break;}
case S::RetainPage:{f.push(r[0].r);break;}
case S::SetScratch:{f.move(r[0].er,0x167fc);break;}
case S::SetCopyCount:{f.move(r[3].e,0xd1);break;}
case S::ReadByte:{f.move(r[4].rl,observed);r[1].er+=1;h.clockI(2);break;}
case S::WriteByte:{;break;}
case S::AdvanceCopy:{f.inc(r[0].er,1);break;}
case S::DecrementCopy:{f.inc(r[3].e,-1);break;}
case S::AdmitCopy:{;break;}
case S::SetZeroCount:{f.move(r[3].e,0x2f);break;}
case S::ClearByte:{f.move(r[4].rl,0x0);break;}
case S::WriteZero:{;break;}
case S::AdvanceZero:{f.inc(r[0].er,1);break;}
case S::DecrementZero:{f.inc(r[3].e,-1);break;}
case S::AdmitZero:{;break;}
case S::SetScratchSource:{f.move(r[5].er,0x167fc);break;}
case S::ProgramPage:{f.call(0x11ae);break;}
case S::RestorePage:{f.pop(r[0].r);break;}
case S::RestoreSource:{f.pop(r[1].er);break;}
case S::AdvancePageSource:{r[1].er=h.add32(r[1].er,0x100);break;}
case S::DecrementPage:{f.inc(r[0].rl,-1);break;}
case S::AdmitPages:{;break;}
case S::ReturnMulti:{f.ret();break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native multi-save clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
