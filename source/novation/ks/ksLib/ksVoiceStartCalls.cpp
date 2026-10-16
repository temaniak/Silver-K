#include "ksVoiceStartCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
VoiceStartPartialTest partialTest=nullptr;using S=VoiceStartTask::Stage;using K=VoiceStartTask::Kind;
struct Point {unsigned location,length;};
Point point(const VoiceStartTask& task){switch(task.stage()){
 case S::DispatchAllocation:return {0x902a,4};case S::AwaitAllocation:case S::ReadVelocity:return {0x902e,8};case S::AdmitVelocity:return {0x9036,4};
 case S::DispatchTracking:return {0x903a,4};case S::AwaitTracking:case S::ReadAllocation:return {0x903e,6};case S::CompareAllocation:return {0x9044,2};case S::AdmitAllocation:return {0x9046,2};
 case S::PrimeOffset:return {0x9048,4};case S::DispatchPrime:return {0x904c,4};case S::AwaitPrime:case S::DispatchParameters:return {0x9050,4};
 case S::AwaitParameters:case S::ReadFlags:return {task.fullGate()?0x927eu:0x9054u,8};case S::ShapeFlags:return {task.fullGate()?0x9286u:0x905cu,2};
 case S::ReadMiddle:return {task.fullGate()?0x9288u:0x905eu,8};case S::ShapeMiddle:return {task.fullGate()?0x9290u:0x9066u,2};
 case S::RotateMiddle:return {0x9292,2};case S::CombineFlags:return {0x9294,2};case S::ClearMiddle:return {0x9296,2};
 case S::CaptureHigh:return {task.fullGate()?0x9298u:0x9068u,2};case S::ReadLow:return {task.fullGate()?0x929au:0x906au,task.fullGate()?8u:2u};
 case S::MaskLow:return {0x92a2,2};case S::FixedLow:return {0x92a4,2};case S::ReadDetune:return {task.fullGate()?0x92a6u:0x906cu,8};
 case S::BiasDetune:return {task.fullGate()?0x92aeu:0x9074u,2};case S::AdmitDetune:return {task.fullGate()?0x92b0u:0x9076u,2};case S::EnableShift:return {task.fullGate()?0x92b2u:0x9078u,2};
 case S::GateOffset:return {task.fullGate()?0x92b4u:0x907au,4};case S::DispatchGate:return {task.fullGate()?0x92b8u:0x907eu,4};
 case S::AwaitGate:case S::Finish:return {task.suppressed()?0x92beu:task.fullGate()?0x92bcu:0x9082u,2};case S::Complete:return {0x9084,0};
 }throw std::runtime_error("Invalid native voice start stage");}
}
void setVoiceStartPartialForTests(VoiceStartPartialTest fn){partialTest=fn;}
bool VoiceStartCalls::step(ControllerCpu& h){using namespace controllerDetail;using C=VoiceStartTask::Child;
 const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0x902a||((pc==0x9054||pc==0x927e)&&!selected)){
  if(pc==0x902a&&selected)throw std::runtime_error("Native voice start has overlapping caller");
  if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native voice start capacity exceeded");
  selected->emplace(pc==0x902a?K::Voice:pc==0x9054?K::ResumeGate:K::FullGate,h.getSP());++started;
 }
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& task=(**selected).task;auto& v=h.regs;const auto child=task.child();if(child!=C::None){uint8_t slot=0;
  if(child==C::Allocation){const auto pointer=uint32_t(v[3].er);if(pointer<0x17560||pointer>=0x17560 + 20*256||(pointer-0x17560)%20)throw std::runtime_error("Invalid native start voice binding");slot=uint8_t((pointer-0x17560)/20);}task.resumeChild(slot);}
 const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task).length);const auto cost=task.nextClocks();
 auto read=[&](VoiceStartTask::Read what)->uint32_t{using R=VoiceStartTask::Read;const uint32_t patch=uint32_t(v[6].er);
  switch(what){case R::Velocity:return uint8_t(h.read8(0x17560 + unsigned(task.slot())*20+6));case R::Allocation:return uint8_t(h.read8(0x1752a));
  case R::Flags:return uint8_t(h.read8(int32_t(patch+79)));case R::Middle:return uint8_t(h.read8(int32_t(patch+(task.fullGate()?78:147))));
  case R::Low:return uint8_t(h.read8(int32_t(patch+80)));case R::Detune:return uint8_t(h.read8(int32_t(patch+4)));}return 0;};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native voice start admission mismatch");
 auto dispatch=[&](unsigned target){clocks.call(target);h.pc=h.makepc(clocks.next);if(h.cycles-before!=cost)throw std::runtime_error("Native voice start dispatch clocks differ");};
 switch(stage){case S::DispatchAllocation:dispatch(0xa338);return true;case S::ReadVelocity:v[0].rl=int8_t(task.velocity());value(h,v[0].rl);break;
 case S::AdmitVelocity:h.clockI(2);break;case S::DispatchTracking:dispatch(0xa1b0);return true;
 case S::ReadAllocation:v[0].rl=int8_t(task.allocation());value(h,v[0].rl);break;case S::CompareAllocation:h.sub8(v[0].rl,1);break;
 case S::PrimeOffset:v[4].r=int16_t(task.primeOffset());value(h,v[4].r);break;case S::DispatchPrime:dispatch(0x77dc);return true;case S::DispatchParameters:dispatch(0x7de4);return true;
 case S::ReadFlags:case S::ShapeFlags:case S::CombineFlags:v[1].rh=int8_t(task.flags());value(h,v[1].rh);break;
 case S::ReadMiddle:v[1].rl=int8_t(task.middle());value(h,v[1].rl);break;
 case S::ShapeMiddle:if(task.fullGate()){v[1].rl=int8_t(task.middle());value(h,v[1].rl);}else clocks.shift(v[1].rl,1,true);break;
 case S::RotateMiddle:clocks.rotateRight(v[1].rl,2);break;case S::ClearMiddle:v[1].rl=0;value(h,v[1].rl);break;
 case S::CaptureHigh:v[1].e=v[1].r;value(h,v[1].e);break;
 case S::ReadLow:case S::MaskLow:case S::FixedLow:case S::EnableShift:v[1].rh=int8_t(task.low());value(h,v[1].rh);break;
 case S::ReadDetune:v[0].rl=int8_t(task.detune());value(h,v[0].rl);break;case S::BiasDetune:v[0].rl=h.add8(v[0].rl,-64);break;
 case S::GateOffset:v[4].r=int16_t(task.gateOffset());value(h,v[4].r);break;
 case S::DispatchGate:if((uint32_t(v[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native voice gate payload projection differs");dispatch(0x785a);return true;
 case S::Finish:clocks.ret();if(h.cycles-before!=cost)throw std::runtime_error("Native voice start return clocks differ");h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;
 default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native voice start bus clocks differ");h.pc=h.makepc(point(task).location);
 if(task.complete()){if(!task.handoff())throw std::runtime_error("Invalid native voice start handoff");selected->reset();++completed;++handoffs;}return true;
}
}
