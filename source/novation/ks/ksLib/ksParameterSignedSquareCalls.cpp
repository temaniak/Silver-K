#include "ksParameterSignedSquareCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {using S=ParameterSignedSquareTask::Stage;ParameterSignedSquarePartialTest partialTest=nullptr;struct Point{unsigned location,length;};struct Binding{unsigned entry,end,offset;std::array<Point,13> points;};
constexpr std::array<Binding,2> bindings{{{0x88ea,0x890e,57,{{{0x88ea,8},{0x88f2,2},{0x88f4,2},{0x88f6,4},{0x88fa,2},{0x88fc,2},{0x88fe,2},{0x8900,2},{0x8902,2},{0x8904,2},{0x8906,2},{0x8908,2},{0x890a,4}}}},{0x890e,0x8932,172,{{{0x890e,8},{0x8916,2},{0x8918,2},{0x891a,4},{0x891e,2},{0x8920,2},{0x8922,2},{0x8924,2},{0x8926,2},{0x8928,2},{0x892a,2},{0x892c,2},{0x892e,4}}}}}};
Point point(const ParameterSignedSquareTask& t,unsigned b){return t.awaitingWord()?Point{bindings[b].end,0}:bindings[b].points[unsigned(t.stage())];}}
void setParameterSignedSquarePartialForTests(ParameterSignedSquarePartialTest fn){partialTest=fn;}
bool ParameterSignedSquareCalls::candidate(unsigned p){for(const auto& b:bindings)if(p>=b.entry&&p<=b.end)return true;return false;}
void ParameterSignedSquareCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingWord()&&point(c->task,c->binding).location==unsigned(h.getPC())){c->task.resumeWord();c.reset();++completed;}}
bool ParameterSignedSquareCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 for(unsigned b=0;b<bindings.size();++b)if(pc==bindings[b].entry){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native curve capacity exceeded");selected->emplace(h.getSP(),b);++started;break;}
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;const auto& binding=bindings[call.binding];const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task,call.binding).length);uint32_t observed=0;
 auto read=[&]()->uint32_t{return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+binding.offset)));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native curve admission mismatch");
 switch(stage){case S::Read:{f.move(r[1].rl,int8_t(observed));break;}
case S::Bias:{r[1].rl=h.add8(r[1].rl,0xffffffc0);break;}
case S::CaptureSign:{f.move(r[0].rl,r[1].rl);break;}
case S::Square:{f.multiplySigned(r[1].rl,r[1].r);break;}
case S::AdmitValue:{f.move(r[0].rl,r[0].rl);break;}
case S::AdmitSign:{;break;}
case S::Negate:{r[1].r=h.sub16(0,r[1].r);break;}
case S::Scale0:{f.shift(r[1].r,1,true);break;}
case S::Scale1:{f.shift(r[1].r,1,true);break;}
case S::Scale2:{f.shift(r[1].r,1,true);break;}
case S::CaptureWord:{f.move(r[1].e,r[1].r);break;}
case S::ClearMiddle:{r[1].rh=h.sub8(r[1].rh,r[1].rh);break;}
case S::Dispatch:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native signed-square packet differs");f.call(0x2520);break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native curve clocks differ");if(stage==S::Dispatch){h.pc=h.makepc(f.next);return true;}h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
