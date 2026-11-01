#include "ksParameterTimeCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {using S=ParameterTimeTask::Stage;ParameterTimePartialTest partialTest=nullptr;struct Point{unsigned location,length;};
struct Binding{unsigned entry,end,offset,base;uint8_t multiplier,threshold;bool optional;std::array<Point,18> points;};
constexpr std::array<Binding,9> bindings{{{0x896a,0x8996,59,0x15134,107,117,false,{{{0x0,0},{0x896a,8},{0x0,0},{0x8972,2},{0x8974,2},{0x8976,2},{0x8978,2},{0x897a,2},{0x897c,2},{0x897e,2},{0x8980,2},{0x8982,2},{0x8984,6},{0x898a,2},{0x898c,4},{0x0,0},{0x8990,2},{0x8992,4}}}},{0x8996,0x89c2,60,0x15136,107,127,false,{{{0x0,0},{0x8996,8},{0x0,0},{0x899e,2},{0x89a0,2},{0x89a2,2},{0x89a4,2},{0x89a6,2},{0x89a8,2},{0x89aa,2},{0x89ac,2},{0x89ae,2},{0x89b0,6},{0x89b6,2},{0x89b8,4},{0x0,0},{0x89bc,2},{0x89be,4}}}},{0x89c2,0x89f4,173,0x1513a,80,115,true,{{{0x89c2,2},{0x89c4,8},{0x89cc,2},{0x89ce,2},{0x89d0,2},{0x89d2,2},{0x89d4,2},{0x89d6,2},{0x89d8,2},{0x89da,2},{0x89dc,2},{0x89de,2},{0x89e0,6},{0x89e6,2},{0x89e8,4},{0x89ec,2},{0x89ee,2},{0x89f0,4}}}},{0x8a2c,0x8a58,62,0x15134,107,117,false,{{{0x0,0},{0x8a2c,8},{0x0,0},{0x8a34,2},{0x8a36,2},{0x8a38,2},{0x8a3a,2},{0x8a3c,2},{0x8a3e,2},{0x8a40,2},{0x8a42,2},{0x8a44,2},{0x8a46,6},{0x8a4c,2},{0x8a4e,4},{0x0,0},{0x8a52,2},{0x8a54,4}}}},{0x8a58,0x8a84,63,0x15136,107,127,false,{{{0x0,0},{0x8a58,8},{0x0,0},{0x8a60,2},{0x8a62,2},{0x8a64,2},{0x8a66,2},{0x8a68,2},{0x8a6a,2},{0x8a6c,2},{0x8a6e,2},{0x8a70,2},{0x8a72,6},{0x8a78,2},{0x8a7a,4},{0x0,0},{0x8a7e,2},{0x8a80,4}}}},{0x8a84,0x8ab0,65,0x15136,107,127,false,{{{0x0,0},{0x8a84,8},{0x0,0},{0x8a8c,2},{0x8a8e,2},{0x8a90,2},{0x8a92,2},{0x8a94,2},{0x8a96,2},{0x8a98,2},{0x8a9a,2},{0x8a9c,2},{0x8a9e,6},{0x8aa4,2},{0x8aa6,4},{0x0,0},{0x8aaa,2},{0x8aac,4}}}},{0x8b46,0x8b72,67,0x15134,107,117,false,{{{0x0,0},{0x8b46,8},{0x0,0},{0x8b4e,2},{0x8b50,2},{0x8b52,2},{0x8b54,2},{0x8b56,2},{0x8b58,2},{0x8b5a,2},{0x8b5c,2},{0x8b5e,2},{0x8b60,6},{0x8b66,2},{0x8b68,4},{0x0,0},{0x8b6c,2},{0x8b6e,4}}}},{0x8b72,0x8b9e,68,0x15136,107,127,false,{{{0x0,0},{0x8b72,8},{0x0,0},{0x8b7a,2},{0x8b7c,2},{0x8b7e,2},{0x8b80,2},{0x8b82,2},{0x8b84,2},{0x8b86,2},{0x8b88,2},{0x8b8a,2},{0x8b8c,6},{0x8b92,2},{0x8b94,4},{0x0,0},{0x8b98,2},{0x8b9a,4}}}},{0x8b9e,0x8bca,70,0x15136,107,127,false,{{{0x0,0},{0x8b9e,8},{0x0,0},{0x8ba6,2},{0x8ba8,2},{0x8baa,2},{0x8bac,2},{0x8bae,2},{0x8bb0,2},{0x8bb2,2},{0x8bb4,2},{0x8bb6,2},{0x8bb8,6},{0x8bbe,2},{0x8bc0,4},{0x0,0},{0x8bc4,2},{0x8bc6,4}}}}}};
Point point(const ParameterTimeTask& task,unsigned b){const auto& v=bindings[b];if(task.awaitingWord())return {v.end,0};
 if(task.awaitingCoefficient())return v.points[unsigned(v.optional?S::ShiftCoefficient:S::CaptureCoefficient)];return v.points[unsigned(task.stage())];}
}
void setParameterTimePartialForTests(ParameterTimePartialTest fn){partialTest=fn;}
bool ParameterTimeCalls::candidate(unsigned p){for(const auto& b:bindings)if(p>=b.entry&&p<=b.end)return true;return false;}
void ParameterTimeCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingWord()&&point(c->task,c->binding).location==unsigned(h.getPC())){c->task.resumeWord();c.reset();++completed;}}
bool ParameterTimeCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}

 for(unsigned b=0;b<bindings.size();++b)if(pc==bindings[b].entry){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native parameter-time capacity exceeded");const auto& v=bindings[b];selected->emplace(v.multiplier,v.threshold,v.optional,h.getSP(),b);++started;break;}
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;const auto& binding=bindings[call.binding];if(task.awaitingCoefficient())task.resumeCoefficient(uint32_t(r[0].er));
 const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task,call.binding).length);uint32_t observed=0;
 auto read=[&](bool base)->uint32_t{return observed=base?uint16_t(h.read16(binding.base)):uint8_t(h.read8(int32_t(uint32_t(r[6].er)+binding.offset)));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native parameter-time admission mismatch");
 switch(stage){case S::ReadValue:{f.move(r[0].rl,int8_t(observed));break;}
case S::InvertValue:{r[0].rl^=0x7f;value(h,r[0].rl);break;}
case S::CaptureValue:{f.move(r[2].rh,r[0].rl);break;}
case S::SetMultiplier:{f.move(r[0].rh,int8_t(binding.multiplier));break;}
case S::Multiply:{r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;}
case S::SetThreshold:{f.move(r[2].rl,int8_t(binding.threshold));break;}
case S::SubtractThreshold:{r[2].rh=h.sub8(r[2].rh,r[2].rl);break;}
case S::AdmitCorrection:{;break;}
case S::ClearCorrection:{r[2].rl=h.sub8(r[2].rl,r[2].rl);break;}
case S::AddCorrection:{r[0].r=h.add16(r[0].r,r[2].r);break;}
case S::ReadBase:{f.move(r[0].e,int16_t(observed));break;}
case S::AddBase:{r[0].r=h.add16(r[0].r,r[0].e);break;}
case S::DispatchCoefficient:{if(uint16_t(r[0].r)!=task.scaleInput())throw std::runtime_error("Native parameter-time coefficient input differs");f.call(0xa69e);break;}
case S::CaptureCoefficient:{f.move(r[1].er,r[0].er);break;}
case S::DispatchWord:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native parameter-time packet projection differs");f.call(0x2520);break;}
case S::ClearOutput:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::AdmitZero:{;break;}
case S::ShiftCoefficient:{f.shift(r[0].er,1,false);break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native parameter-time clocks differ");
 if(stage==S::DispatchWord||stage==S::DispatchCoefficient){h.pc=h.makepc(f.next);return true;}
 h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
