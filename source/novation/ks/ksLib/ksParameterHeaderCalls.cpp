#include "ksParameterHeaderCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {using S=ParameterHeaderTask::Stage;using K=ParameterHeaderTask::Kind;ParameterHeaderPartialTest partialTest=nullptr;struct Point{unsigned location,length;};struct Binding{K kind;unsigned entry,end;std::array<Point,22> points;};
constexpr std::array<Binding,3> bindings{{{K::Header,0x7de4,0x7e20,{{{0x7de4,6},{0x7dea,6},{0x7df0,2},{0x7df2,2},{0x7df4,2},{0x7df6,2},{0x7df8,2},{0x7dfa,2},{0x7dfc,2},{0x7dfe,2},{0x7e00,4},{0x7e04,8},{0x7e0c,2},{0x7e0e,2},{0x7e10,2},{0x7e12,2},{0x7e14,2},{0x7e16,2},{0x7e18,2},{0x7e1a,2},{0x7e1c,4},{0x0,0}}}},{K::Part,0x8ae8,0x8b00,{{{0x8ae8,6},{0x8aee,6},{0x8af4,2},{0x8af6,2},{0x8af8,2},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x8afa,2},{0x8afc,4},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0}}}},{K::Byte,0x8b00,0x8b0e,{{{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x8b08,2},{0x8b0a,4},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x0,0},{0x8b00,8}}}}}};
Point point(const ParameterHeaderTask& t,unsigned b){if(t.stage()==S::AwaitWord)return t.kind()==K::Header?bindings[b].points[unsigned(S::ReadMode)]:Point{bindings[b].end,0};if(t.stage()==S::AwaitModeWord)return {bindings[b].end,0};return bindings[b].points[unsigned(t.stage())];}}
void setParameterHeaderPartialForTests(ParameterHeaderPartialTest fn){partialTest=fn;}
bool ParameterHeaderCalls::candidate(unsigned p){for(const auto& b:bindings)if(p>=b.entry&&p<=b.end)return true;return false;}
void ParameterHeaderCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingWord()&&point(c->task,c->binding).location==unsigned(h.getPC())&&(c->task.kind()!=K::Header||c->task.stage()==S::AwaitModeWord)){c->task.resumeWord();c.reset();++completed;}}
bool ParameterHeaderCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 for(unsigned b=0;b<bindings.size();++b)if(pc==bindings[b].entry){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native parameter-header capacity exceeded");selected->emplace(bindings[b].kind,h.getSP(),b);++started;break;}
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingWord())task.resumeWord();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task,call.binding).length);uint32_t observed=0;
 auto read=[&](unsigned what)->uint32_t{return observed=what==0?uint8_t(h.read8(0x17526)):what==1?uint8_t(h.read8(0x167ba)):uint8_t(h.read8(int32_t(uint32_t(r[6].er)+(what==2?6:148))));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native parameter-header admission mismatch");
 switch(stage){case S::ReadPart:{f.move(r[1].rh,int8_t(observed));break;}
case S::ReadBase:{f.move(r[0].rl,int8_t(observed));break;}
case S::SubtractPart:{r[1].rh=h.sub8(r[1].rh,r[0].rl);break;}
case S::AdmitNegative:{;break;}
case S::ClampNegative:{f.move(r[1].rh,0x0);break;}
case S::CapturePart:{f.move(r[0].rl,r[1].rh);break;}
case S::DoublePart:{f.shift(r[0].rl,1,true);break;}
case S::AddPart:{r[1].rh=h.add8(r[1].rh,r[0].rl);break;}
case S::OffsetPart:{r[1].rh=h.add8(r[1].rh,0x67);break;}
case S::ClearHigh:{r[1].e=h.sub16(r[1].e,r[1].e);break;}
case S::DispatchWord:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native parameter-header packet differs");f.call(0x2520);break;}
case S::ReadMode:{f.move(r[1].rh,int8_t(observed));break;}
case S::RotateMode:{f.rotate(r[1].rh,2,true,false);break;}
case S::MaskMode:{r[1].rh&=0x1;value(h,r[1].rh);break;}
case S::InvertMode:{r[1].rh^=0x1;value(h,r[1].rh);break;}
case S::CaptureMode:{f.move(r[1].rl,r[1].rh);break;}
case S::DoubleMode:{f.shift(r[1].rl,1,true);break;}
case S::AddMode:{r[1].rh=h.add8(r[1].rh,r[1].rl);break;}
case S::OffsetMode:{r[1].rh=h.add8(r[1].rh,0x3);break;}
case S::ClearModeHigh:{r[1].e=h.sub16(r[1].e,r[1].e);break;}
case S::DispatchModeWord:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native parameter-header packet differs");f.call(0x2520);break;}
case S::ReadByte:{f.move(r[1].rh,int8_t(observed));break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native parameter-header clocks differ");if(stage==S::DispatchWord||stage==S::DispatchModeWord){h.pc=h.makepc(f.next);return true;}h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
