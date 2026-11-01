#include "ksStateValueBoundsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateValueBoundsPartialTest partialTest=nullptr;using T=StateValueBoundsTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,71> points{{{0x5112,4},{0x5116,2},{0x5118,4},{0x511c,2},{0x511e,2},{0x5120,4},{0x5124,2},{0x5126,4},{0x512a,2},{0x512c,2},{0x512e,4},{0x5132,2},{0x5134,2},{0x5136,4},{0x513a,2},{0x513c,2},{0x513e,4},{0x5142,2},{0x5144,4},{0x5148,2},{0x514a,2},{0x514c,4},{0x5150,2},{0x5152,2},{0x5154,2},{0x5156,4},{0x515a,2},{0x515c,2},{0x515e,8},{0x5166,8},{0x516e,2},{0x5170,2},{0x5172,2},{0x5174,4},{0x5178,2},{0x517a,2},{0x517c,2},{0x517e,8},{0x5186,8},{0x518e,2},{0x5190,4},{0x5194,2},{0x5196,2},{0x5198,4},{0x519c,2},{0x519e,2},{0x51a0,2},{0x51a2,4},{0x51a6,2},{0x51a8,2},{0x51aa,2},{0x51ac,2},{0x51ae,2},{0x51b0,2},{0x51b2,2},{0x51b4,4},{0x51b8,2},{0x51ba,2},{0x51bc,2},{0x51be,2},{0x51c0,2},{0x51c2,2},{0x51c4,2},{0x51c6,4},{0x51ca,2},{0x51cc,2},{0x51ce,2},{0x51d0,2},{0x51d2,2},{0x51d4,2},{0x51d6,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateValueBoundsPartialForTests(StateValueBoundsPartialTest value){partialTest=value;}
bool StateValueBoundsCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x5112:kind=T::Kind::Resolve;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateValueBounds capacity exceeded");slot->emplace(*kind,h.getSP(),uint8_t(h.regs[2].rh),uint8_t(h.regs[6].rh),bool(h.regs[5].rh),uint32_t(h.regs[1].er));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();
 auto accessCost=[&](uint32_t ref,unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){const unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};unsigned store=3;switch(stage){case S::ReadOscillatorPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadOscillator:store=accessCost(uint32_t(r[3].er+224),1,-1);break;case S::ReadModulationPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadModulation:store=accessCost(uint32_t(r[3].er+139),1,-1);break;default:break;} const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{return field==T::Read::Patch?uint32_t(h.read32(0x167bc)):uint8_t(h.read8(ref));};auto write=[](int,int){};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateValueBounds admission differs");
 switch(stage){case S::BindDefaultBounds:f.move(r[3].r,0x7f00);break;
case S::TestValueType:f.move(r[2].rh,r[2].rh);break;
case S::SelectDefaultBounds:h.clockI(2);break;
case S::TestBipolarOne:h.sub8(r[2].rh,0x1);break;
case S::BindBipolarOneBounds:f.move(r[3].r,0x4c34);break;
case S::TestBipolarOneMode:f.move(r[5].rh,r[5].rh);break;
case S::SelectBipolarOneMode:h.clockI(2);break;
case S::BindBipolarOneScale:f.move(r[2].rl,0x31);break;
case S::BiasBipolarOne:r[6].rh=h.add8(r[6].rh,0xffffffc3);break;
case S::MultiplyBipolarOne:f.multiplySigned(r[6].rh,r[2].r);break;
case S::CenterBipolarOne:r[2].rh=h.add8(r[2].rh,0x40);break;
case S::BindBipolarOne:f.move(r[6].rh,r[2].rh);break;
case S::JoinBipolarOne:h.clockI(2);break;
case S::TestBipolarTwo:h.sub8(r[2].rh,0x2);break;
case S::BindBipolarTwoBounds:f.move(r[3].r,0x720e);break;
case S::TestBipolarTwoMode:f.move(r[5].rh,r[5].rh);break;
case S::SelectBipolarTwoMode:h.clockI(2);break;
case S::BindBipolarTwoScale:f.move(r[2].rl,0x65);break;
case S::BiasBipolarTwo:r[6].rh=h.add8(r[6].rh,0xffffffc1);break;
case S::MultiplyBipolarTwo:f.multiplySigned(r[6].rh,r[2].r);break;
case S::DoubleBipolarTwo:f.shift(r[2].r,1,true);break;
case S::CenterBipolarTwo:r[2].rh=h.add8(r[2].rh,0x40);break;
case S::BindBipolarTwo:f.move(r[6].rh,r[2].rh);break;
case S::JoinBipolarTwo:h.clockI(2);break;
case S::TestOscillator:h.sub8(r[2].rh,0x3);break;
case S::ReadOscillatorPatch:f.move(r[3].er,task.patch());break;
case S::ReadOscillator:f.move(r[2].rl,task.index());break;
case S::NormalizeOscillatorWord:r[2].r&=0xff;value(h,r[2].r);break;
case S::NormalizeOscillatorLong:r[2].er&=0xffff;value(h,r[2].er);break;
case S::LocateOscillator:r[1].er=h.add32(r[1].er,r[2].er);break;
case S::BindOscillatorBounds:f.move(r[3].r,0x7f00);break;
case S::TestModulation:h.sub8(r[2].rh,0x13);break;
case S::ReadModulationPatch:f.move(r[3].er,task.patch());break;
case S::ReadModulation:f.move(r[2].rl,task.index());break;
case S::NormalizeModulation:r[2].r&=0xff;value(h,r[2].r);break;
case S::BindModulationStride:f.move(r[2].e,0x68);break;
case S::MultiplyModulation:r[2].er=uint16_t(r[2].er)*uint32_t(uint16_t(r[2].e));h.clockI(20);break;
case S::LocateModulation:r[1].er=h.add32(r[1].er,r[2].er);break;
case S::BindModulationBounds:f.move(r[3].r,0x7f00);break;
case S::TestTwenty:h.sub8(r[2].rh,0xa);break;
case S::BindTwentyBounds:f.move(r[3].r,0x1300);break;
case S::TestTwentyMode:f.move(r[5].rh,r[5].rh);break;
case S::BindTwentyScale:f.move(r[6].rl,0x28);break;
case S::MultiplyTwenty:r[6].r=uint8_t(r[6].r)*uint32_t(uint8_t(r[6].rh));h.clockI(12);break;
case S::TestTwentyFour:h.sub8(r[2].rh,0x15);break;
case S::BindTwentyFourBounds:f.move(r[3].r,0x1700);break;
case S::TestTwentyFourMode:f.move(r[5].rh,r[5].rh);break;
case S::BindTwentyFourScale:f.move(r[6].rl,0x30);break;
case S::MultiplyTwentyFour:r[6].r=uint8_t(r[6].r)*uint32_t(uint8_t(r[6].rh));h.clockI(12);break;
case S::TestThirtyTwo:h.sub8(r[2].rh,0x16);break;
case S::BindThirtyTwoBounds:f.move(r[3].r,0x1f00);break;
case S::TestThirtyTwoMode:f.move(r[5].rh,r[5].rh);break;
case S::BindThirtyTwoScale:f.move(r[6].rl,0x40);break;
case S::MultiplyThirtyTwo:r[6].r=uint8_t(r[6].r)*uint32_t(uint8_t(r[6].rh));h.clockI(12);break;
case S::PublishBounds:f.move(r[2].r,r[3].r);break;
case S::ReturnBounds:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateValueBounds clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
