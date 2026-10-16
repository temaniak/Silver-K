#include "ksParameterVolumeCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {using S=ParameterVolumeTask::Stage;ParameterVolumePartialTest partialTest=nullptr;struct Point{unsigned location,length;};struct Binding{unsigned entry,end,offset;std::array<Point,15> points;};
constexpr std::array<Binding,1> bindings{{{0x8712,0x8744,44,{{{0x8712,8},{0x871a,8},{0x8722,2},{0x8724,2},{0x8726,2},{0x8728,6},{0x872e,2},{0x8730,4},{0x8734,2},{0x8736,2},{0x8738,2},{0x873a,2},{0x873c,2},{0x873e,2},{0x8740,4}}}}}};
Point point(const ParameterVolumeTask& t,unsigned b){if(t.stage()==S::AwaitVolume)return bindings[b].points[8];if(t.stage()==S::AwaitShape)return {bindings[b].end,0};return bindings[b].points[unsigned(t.stage())];}}
void setParameterVolumePartialForTests(ParameterVolumePartialTest fn){partialTest=fn;}
bool ParameterVolumeCalls::candidate(unsigned p){for(const auto& b:bindings)if(p>=b.entry&&p<=b.end)return true;return false;}
void ParameterVolumeCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.stage()==S::AwaitShape&&point(c->task,c->binding).location==unsigned(h.getPC())){c->task.resumeWord();c.reset();++completed;}}
bool ParameterVolumeCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 for(unsigned b=0;b<bindings.size();++b)if(pc==bindings[b].entry){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native curve capacity exceeded");selected->emplace(h.getSP(),b);++started;break;}
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;const auto& binding=bindings[call.binding];if(task.awaitingWord())task.resumeWord();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task,call.binding).length);uint32_t observed=0;
 auto read=[&](bool level)->uint32_t{return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+(level?44:45))));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native curve admission mismatch");
 switch(stage){case S::ReadAmount:{f.move(r[0].rl,int8_t(observed));break;}
case S::ReadLevel:{f.move(r[0].rh,int8_t(observed));break;}
case S::CaptureLevel:{f.move(r[2].rl,r[0].rh);break;}
case S::MultiplyVolume:{r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;}
case S::DoubleVolume:{f.shift(r[0].r,1,true);break;}
case S::SetUnity:{f.move(r[1].er,0x7fff0000);break;}
case S::SubtractVolume:{r[1].e=h.sub16(r[1].e,r[0].r);break;}
case S::DispatchVolume:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native volume packet differs");f.call(0x2520);break;}
case S::RestoreLevel:{f.move(r[1].rh,r[2].rl);break;}
case S::InvertLevel:{r[1].rh^=0x7f;value(h,r[1].rh);break;}
case S::SetShapeScale:{f.move(r[1].rl,0xffffffb5);break;}
case S::MultiplyShape:{r[1].r=uint8_t(r[1].r)*uint32_t(uint8_t(r[1].rh));h.clockI(12);break;}
case S::CaptureShape:{f.move(r[1].e,r[1].r);break;}
case S::ClearMiddle:{r[1].rh=h.sub8(r[1].rh,r[1].rh);break;}
case S::DispatchShape:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native volume packet differs");f.call(0x2520);break;}default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native curve clocks differ");if((stage==S::DispatchVolume||stage==S::DispatchShape)){h.pc=h.makepc(f.next);return true;}h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
