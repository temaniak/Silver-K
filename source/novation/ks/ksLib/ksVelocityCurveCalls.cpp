#include "ksVelocityCurveCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
VelocityCurvePartialTest partialTest=nullptr;
struct Point {unsigned location,length;};
Point point(const VelocityCurveTask& task){
 using S=VelocityCurveTask::Stage;using E=VelocityCurveTask::End;
 switch(task.stage()){
 case S::CaptureBase:return {0xa5fc,2};case S::PowerTick:return {0xa5fe,2};case S::PowerGate:return {0xa600,2};
 case S::PowerSeed:return {0xa602,2};case S::PowerProduct:return {0xa604,2};case S::PowerScale:return {0xa606,2};case S::PowerRepeat:return {0xa608,2};
 case S::InverseSeed:return {0xa60a,2};case S::Invert:return {0xa60c,2};case S::TestCurve:return {0xa60e,2};case S::CurveGate:return {0xa610,2};
 case S::TickOne:return {0xa614,2};case S::GateOne:return {0xa616,2};case S::UseInverse:return {0xa618,2};
 case S::TickTwo:return {0xa61c,2};case S::GateTwo:return {0xa61e,2};case S::SquareSeed:return {0xa620,2};case S::SquareProduct:return {0xa622,2};case S::SquareScale:return {0xa624,2};
 case S::TickThree:return {0xa628,2};case S::GateThree:return {0xa62a,2};case S::MixedProduct:return {0xa62c,2};case S::MixedScale:return {0xa62e,2};
 case S::TickFour:return {0xa632,2};case S::NormalizeCurve:return {0xa634,2};case S::NormalizeWide:return {0xa636,2};case S::ResolveThreshold:return {0xa638,6};
 case S::ReadThreshold:return {0xa63e,2};case S::TestParity:return {0xa640,2};case S::ParityGate:return {0xa642,2};case S::EvenCompare:return {0xa644,2};case S::EvenGate:return {0xa646,2};
 case S::OddCompare:return {0xa64a,2};case S::OddGate:return {0xa64c,2};case S::Suppress:return {0xa650,2};
 case S::Complete:switch(task.ending()){case E::Normal:return {0xa612,2};case E::Inverted:return {0xa61a,2};case E::Square:return {0xa626,2};case E::Mixed:return {0xa630,2};
 case E::EvenKept:return {0xa648,2};case E::OddKept:return {0xa64e,2};case E::Suppressed:return {0xa652,2};case E::Empty:return {0xa654,2};}break;
 }throw std::runtime_error("Invalid native velocity curve stage");
}
}
void setVelocityCurvePartialForTests(VelocityCurvePartialTest fn){partialTest=fn;}
bool VelocityCurveCalls::step(ControllerCpu& h){
 using namespace controllerDetail;using S=VelocityCurveTask::Stage;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0xa5fc||pc==0xa654){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
  if(!selected)throw std::runtime_error("Native velocity curve capacity exceeded");
  selected->emplace(pc==0xa654?VelocityCurveTask::empty():VelocityCurveTask(uint8_t(h.regs[1].rh),uint8_t(h.regs[2].rl),uint8_t(h.regs[2].rh)),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& task=(*selected)->task;const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task).length);auto& v=h.regs;
 if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
 const int8_t oldPower=int8_t(task.power()),oldCurve=int8_t(task.curve());const auto oldProduct=task.product();
 const auto cost=task.nextClocks();const auto progress=task.advanceBefore(cost,[&](unsigned index){return h.read8(0x1534e + index);});
 if(progress.clocks!=cost||progress.boundaries!=1)throw std::runtime_error("Native velocity curve admission mismatch");
 auto decrement=[&](int8_t& output,int8_t old,uint8_t next){const int result=int(old)-1;output=int8_t(next);value(h,result);flag(h,2,(uint32_t(old)&~uint32_t(result))&0x80);};
 switch(stage){
 case S::CaptureBase:v[0].rl=int8_t(task.base());value(h,v[0].rl);break;
 case S::PowerTick:decrement(v[2].rh,oldPower,task.power());break;
 case S::PowerSeed:case S::InverseSeed:case S::SquareSeed:v[1].rl=int8_t(task.velocity());value(h,v[1].rl);break;
 case S::PowerProduct:case S::SquareProduct:case S::MixedProduct:v[1].r=int16_t(task.product());h.clockI(12);break;
 case S::PowerScale:case S::SquareScale:case S::MixedScale:v[1].r=int16_t(task.product());flag(h,1,oldProduct&0x8000);value(h,int32_t(uint32_t(task.product())<<16));break;
 case S::Invert:v[1].rl=int8_t(task.inverse());value(h,v[1].rl);break;
 case S::TestCurve:value(h,int8_t(task.curve()));break;
 case S::TickOne:case S::TickTwo:case S::TickThree:case S::TickFour:decrement(v[2].rl,oldCurve,task.curve());break;
 case S::UseInverse:case S::Suppress:v[1].rh=int8_t(task.velocity());value(h,v[1].rh);break;
 case S::NormalizeCurve:v[2].r=int16_t(task.curve());value(h,v[2].r);break;
 case S::NormalizeWide:v[2].er=task.curve();value(h,v[2].er);break;
 case S::ResolveThreshold:v[2].er=h.add32(task.curve(),0x1534e);break;
 case S::ReadThreshold:v[2].rl=int8_t(task.threshold());value(h,v[2].rl);break;
 case S::TestParity:flag(h,4,!(task.threshold()&1));break;
 case S::EvenCompare:case S::OddCompare:h.sub8(int8_t(task.base()),int8_t(task.threshold()));break;default:break;
 }
 if(h.cycles-before!=cost)throw std::runtime_error("Native velocity curve bus clock mismatch");
 h.pc=h.makepc(point(task).location);return true;
}
}
