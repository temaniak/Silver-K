#include "ksParameterFilterCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {using S=ParameterFilterTask::Stage;ParameterFilterPartialTest partialTest=nullptr;struct Point{unsigned location,length;};struct Binding{unsigned entry,end,offset;std::array<Point,23> points;};
constexpr std::array<Binding,1> bindings{{{0x8744,0x8798,150,{{{0x8744,4},{0x8748,8},{0x8750,2},{0x8752,8},{0x875a,2},{0x875c,4},{0x8760,2},{0x8762,8},{0x876a,2},{0x876c,2},{0x876e,2},{0x8770,2},{0x8772,2},{0x8774,4},{0x8778,2},{0x877a,4},{0x877e,2},{0x8780,4},{0x8784,2},{0x8786,6},{0x878c,6},{0x8792,2},{0x8794,4}}}}}};
Point point(const ParameterFilterTask& t,unsigned b){return t.awaitingWord()?Point{bindings[b].end,0}:bindings[b].points[t.awaitingVoice()?1:unsigned(t.stage())];}}
void setParameterFilterPartialForTests(ParameterFilterPartialTest fn){partialTest=fn;}
bool ParameterFilterCalls::candidate(unsigned p){for(const auto& b:bindings)if(p>=b.entry&&p<=b.end)return true;return false;}
void ParameterFilterCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingWord()&&point(c->task,c->binding).location==unsigned(h.getPC())){c->task.resumeWord();c.reset();++completed;}}
bool ParameterFilterCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 for(unsigned b=0;b<bindings.size();++b)if(pc==bindings[b].entry){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native voice-scale capacity exceeded");selected->emplace(h.getSP(),b);++started;break;}
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;const auto& binding=bindings[call.binding];
 if(task.awaitingVoice()){const auto pointer=uint32_t(r[3].er);if(pointer<0x17560||pointer>=0x17560+20*256||(pointer-0x17560)%20)throw std::runtime_error("Invalid voice-scale handle");task.resumeVoice(uint8_t((pointer-0x17560)/20));}
 const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task,call.binding).length);uint32_t observed=0;
 auto read=[&](ParameterFilterTask::Read what)->uint32_t{using R=ParameterFilterTask::Read;return observed=what==R::Velocity?uint8_t(h.read8(0x17560+task.slot()*20+6)):what==R::Depth?uint8_t(h.read8(int32_t(uint32_t(r[6].er)+150))):what==R::Cutoff?uint8_t(h.read8(int32_t(uint32_t(r[6].er)+46))):uint16_t(h.read16(0x15132));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native voice-scale admission mismatch");
 switch(stage){case S::DispatchVoice:{f.call(0xa656);break;}
case S::ReadVelocity:{f.move(r[0].rl,int8_t(observed));break;}
case S::BiasVelocity:{r[0].rl=h.add8(r[0].rl,0xffffffc0);break;}
case S::ReadDepth:{f.move(r[0].rh,int8_t(observed));break;}
case S::BiasDepth:{r[0].rh=h.add8(r[0].rh,0xffffffc0);break;}
case S::MultiplyVelocity:{f.multiplySigned(r[0].rh,r[0].r);break;}
case S::DoubleModulation:{f.shift(r[0].r,1,true);break;}
case S::ReadCutoff:{f.move(r[1].rh,int8_t(observed));break;}
case S::ClearLow:{r[1].rl=h.sub8(r[1].rl,r[1].rl);break;}
case S::HalfCutoff:{f.shift(r[1].r,1,false);break;}
case S::AddModulation:{r[1].r=h.add16(r[1].r,r[0].r);break;}
case S::AdmitSign:{;break;}
case S::ClampZero:{r[1].r=h.sub16(r[1].r,r[1].r);break;}
case S::CompareMaximum:{h.sub16(r[1].r,0x3f80);break;}
case S::AdmitMaximum:{;break;}
case S::ClampMaximum:{f.move(r[1].r,0x3f80);break;}
case S::DoubleCutoff:{f.shift(r[1].r,1,true);break;}
case S::SetScale:{f.move(r[1].e,0xaaab);break;}
case S::MultiplyCutoff:{r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[1].e));h.clockI(20);break;}
case S::SubtractOrigin:{r[1].er=h.sub32(r[1].er,0x48001400);break;}
case S::ReadStatic:{f.move(r[0].e,int16_t(observed));break;}
case S::AddStatic:{r[1].e=h.add16(r[1].e,r[0].e);break;}
case S::DispatchWord:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native filter packet differs");f.call(0x2520);break;}default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native voice-scale clocks differ");if(stage==S::DispatchWord||stage==S::DispatchVoice){h.pc=h.makepc(f.next);return true;}h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
