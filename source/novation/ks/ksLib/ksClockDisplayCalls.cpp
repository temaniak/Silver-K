#include "ksClockDisplayCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
ClockDisplayPartialTest partialTest=nullptr;using T=ClockDisplayTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,31> points{{{0x29a6,4},{0x29aa,4},{0x29ae,4},{0x29b2,2},{0x29b4,4},{0x29b8,6},{0x29be,2},{0x29c0,6},{0x29c6,2},{0x29c8,2},{0x29ca,2},{0x29cc,2},{0x29ce,2},{0x29d0,2},{0x29d2,6},{0x29d8,4},{0x29dc,2},{0x29de,4},{0x29e2,8},{0x29ea,4},{0x29ee,2},{0x29f0,2},{0x29f2,2},{0x29f4,2},{0x29f6,2},{0x29f8,2},{0x29fa,4},{0x29fe,4},{0x2a02,4},{0x2a06,4},{0x2a0a,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
}
void setClockDisplayPartialForTests(ClockDisplayPartialTest value){partialTest=value;}
bool ClockDisplayCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 if(pc==0x29a6){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native clock display capacity exceeded");slot->emplace(T::Kind::Display,h.getSP());++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field)->uint32_t{return field==T::Read::Period?uint32_t(h.read32(0xffee7e)):uint8_t(h.read8(field==T::Read::Active?0xffee96:0xffee97));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native clock display admission differs");
 switch(stage){
case S::SaveDisplayValue:f.push(r[0].er);break;
case S::SaveDisplayIndex:f.push(r[1].er);break;
case S::SaveDisplayText:f.push(r[6].er);break;
case S::BindDisplayPosition:f.move(r[6].rl,0xffffff8d);break;
case S::SetDisplayPosition:f.call(0x976);break;
case S::ReadActive:f.move(r[1].rl,task.index());break;
case S::ReadFlags:f.move(r[0].rl,task.flags());break;
case S::TestFlags:flag(h,4,(uint8_t(r[0].rl)&(1u<<0x0))==0);break;
case S::IncrementActive:f.inc(r[1].rl,1);break;
case S::ScaleTextIndex:f.shift(r[1].rl,2,true);break;
case S::ClearTextAddress:r[6].er=h.sub32(r[6].er,r[6].er);break;
case S::BindTextIndex:f.move(r[6].rl,r[1].rl);break;
case S::LocateText:r[6].er=h.add32(r[6].er,0x2a0c);break;
case S::DisplayText:f.call(0x9b8);break;
case S::BindNumberPosition:f.move(r[6].rl,0x20);break;
case S::SetNumberPosition:f.call(0x976);break;
case S::ReadPeriod:f.move(r[0].er,task.period());break;
case S::BindPeriodWeight:f.move(r[1].r,0xd1b7);break;
case S::DividePeriod:f.divideUnsigned(r[1].r,r[0].er);break;
case S::QuarterFirst:f.shift(r[0].r,2,false);break;
case S::QuarterSecond:f.shift(r[0].r,2,false);break;
case S::HalfNumber:f.shift(r[0].r,1,false);break;
case S::RoundLowByte:f.inc(r[0].rl,1);break;
case S::DisplayNumber:f.call(0xa28);break;
case S::RestoreDisplayText:f.pop(r[6].er);break;
case S::RestoreDisplayIndex:f.pop(r[1].er);break;
case S::RestoreDisplayValue:f.pop(r[0].er);break;
case S::ReturnDisplay:f.ret();break;
 default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native clock display clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
