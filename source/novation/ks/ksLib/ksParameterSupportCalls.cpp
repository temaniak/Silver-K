#include "ksParameterSupportCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {using S=ParameterSupportTask::Stage;using K=ParameterSupportTask::Kind;ParameterSupportPartialTest partialTest=nullptr;struct Point{unsigned location,length;};struct Binding{K kind;unsigned entry,end,offset;std::array<Point,29> points;};
constexpr std::array<Binding,5> bindings{{{K::Rate,0x8898,0x88ae,47,{{{0x8898,8},{0x88a0,2},{0x88a2,2},{0x88a4,4},{0x88a8,2},{0x88aa,4},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0}}}},{K::Sync,0x88ae,0x88d6,140,{{{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x88ae,8},{0x88b6,2},{0x88b8,4},{0x88bc,4},{0x88c0,4},{0x88c4,8},{0x88cc,2},{0x88ce,2},{0x88d0,2},{0x88d2,4},{0x0,0},{0x0,0},{0x0,0}}}},{K::Clock,0x8ac4,0x8ae8,125,{{{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x8ac4,8},{0x8acc,8},{0x8ad4,2},{0x8ad6,2},{0x8ad8,2},{0x8ada,2},{0x8adc,2},{0x8ade,4},{0x8ae2,2},{0x8ae4,4},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0}}}},{K::Rate,0x8bca,0x8be0,69,{{{0x8bca,8},{0x8bd2,2},{0x8bd4,2},{0x8bd6,4},{0x8bda,2},{0x8bdc,4},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0}}}},{K::Exit,0x8fb6,0x8fc0,0,{{{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x8fb6,4},{0x8fba,4},{0x8fbe,2}}}}}};
Point point(const ParameterSupportTask& t,unsigned b){const auto& v=bindings[b];if(t.terminalAwait())return {v.end,0};const auto s=t.stage();return v.points[unsigned(s==S::AwaitCoefficient?S::CopyCoefficient:s==S::AwaitSyncFirst?S::RestoreSync:s==S::AwaitExitFirst?S::DispatchExitSecond:s==S::AwaitExitSecond?S::ReturnExit:s)];}
unsigned depth(const ParameterSupportTask& t){switch(t.stage()){case S::DispatchSyncFirst:case S::AwaitSyncFirst:case S::RestoreSync:return 4;default:return 0;}}}
void setParameterSupportPartialForTests(ParameterSupportPartialTest fn){partialTest=fn;}
bool ParameterSupportCalls::candidate(unsigned p){for(const auto& b:bindings)if(p>=b.entry&&p<=b.end)return true;return false;}
void ParameterSupportCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token-depth(c->task)==h.getSP()&&c->task.terminalAwait()&&point(c->task,c->binding).location==unsigned(h.getPC())){c->task.resumeChild();c.reset();++completed;}}
bool ParameterSupportCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token-depth(c->task)==h.getSP()){selected=&c;break;}
 for(unsigned b=0;b<bindings.size();++b)if(pc==bindings[b].entry){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native parameter-support capacity exceeded");selected->emplace(bindings[b].kind,h.getSP(),b);++started;break;}
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;const auto& binding=bindings[call.binding];if(task.awaitingChild())task.resumeChild(uint32_t(r[0].er));const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task,call.binding).length);uint32_t observed=0;
 auto read=[&](ParameterSupportTask::Read what)->uint32_t{using R=ParameterSupportTask::Read;return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+(what==R::Rate?binding.offset:what==R::Clock?125:what==R::Sync?140:0))));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native parameter-support admission mismatch");
 switch(stage){case S::ReadRate:{f.move(r[1].rh,int8_t(observed));break;}
case S::ClearRateLow:{r[1].rl=h.sub8(r[1].rl,r[1].rl);break;}
case S::DoubleRate:{f.shift(r[1].r,1,true);break;}
case S::SetRateScale:{f.move(r[1].e,0x8102);break;}
case S::MultiplyRate:{r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[1].e));h.clockI(20);break;}
case S::DispatchRate:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native support packet differs");f.call(0x2520);break;}
case S::ReadSync:{f.move(r[1].rh,int8_t(observed));break;}
case S::ClearSyncHigh:{r[1].e=h.sub16(r[1].e,r[1].e);break;}
case S::RetainSync:{f.push(r[1].er);break;}
case S::DispatchSyncFirst:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native support packet differs");f.call(0x2520);break;}
case S::RestoreSync:{f.pop(r[1].er);break;}
case S::ReadSyncFlags:{f.move(r[0].rl,int8_t(observed));break;}
case S::TestSyncFlag:{flag(h,4,(uint8_t(r[0].rl)&(1u<<0x5))==0);break;}
case S::AdmitSyncFlag:{;break;}
case S::OverrideSync:{f.move(r[1].rh,0x3);break;}
case S::DispatchSyncLast:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native support packet differs");f.call(0x2520);break;}
case S::ReadClock:{f.move(r[0].rl,int8_t(observed));break;}
case S::ReadClockFlags:{f.move(r[0].rh,int8_t(observed));break;}
case S::MaskClockFlags:{r[0].rh&=0x7;value(h,r[0].rh);break;}
case S::SubtractFlags:{r[0].rl=h.sub8(r[0].rl,r[0].rh);break;}
case S::OffsetClock:{r[0].rl=h.add8(r[0].rl,0xffffffa2);break;}
case S::SetClockMultiplier:{f.move(r[0].rh,0xffffffaa);break;}
case S::MultiplyClock:{r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;}
case S::DispatchCoefficient:{if(uint16_t(r[0].r)!=task.scaleInput())throw std::runtime_error("Native support coefficient input differs");f.call(0xa69e);break;}
case S::CopyCoefficient:{f.move(r[1].er,r[0].er);break;}
case S::DispatchClock:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native support packet differs");f.call(0x2520);break;}
case S::DispatchExitFirst:{f.call(0x24a0);break;}
case S::DispatchExitSecond:{f.call(0x248e);break;}
case S::ReturnExit:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native parameter-support clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(task.awaitingChild()){h.pc=h.makepc(f.next);return true;}h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
