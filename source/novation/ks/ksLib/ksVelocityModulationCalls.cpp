#include "ksVelocityModulationCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
VelocityModulationPartialTest partialTest=nullptr;using S=VelocityModulationTask::Stage;
struct Point {unsigned location,length;};
struct Binding {unsigned entry,end,depth;uint16_t offset;std::array<Point,21> points;};
constexpr std::array<Binding,8> bindings{{
 {0x8932,0x896a,58,0x0,{{{0x8932,4},{0x8936,8},{0x8936,8},{0x893e,8},{0x8946,2},{0x8948,2},{0x894a,2},{0x894c,2},{0x894e,2},{0x8950,4},{0x8954,2},{0x8956,2},{0x8958,6},{0x895e,2},{0x8960,2},{0x8962,2},{0x8964,2},{0x0,0},{0x8966,4},{0x896a,0},{0x896a,0}}}},
 {0x89f4,0x8a2c,61,0x0,{{{0x89f4,4},{0x89f8,8},{0x89f8,8},{0x8a00,8},{0x8a08,2},{0x8a0a,2},{0x8a0c,2},{0x8a0e,2},{0x8a10,2},{0x8a12,4},{0x8a16,2},{0x8a18,2},{0x8a1a,6},{0x8a20,2},{0x8a22,2},{0x8a24,2},{0x8a26,2},{0x0,0},{0x8a28,4},{0x8a2c,0},{0x8a2c,0}}}},
 {0x8b0e,0x8b46,66,0x0,{{{0x8b0e,4},{0x8b12,8},{0x8b12,8},{0x8b1a,8},{0x8b22,2},{0x8b24,2},{0x8b26,2},{0x8b28,2},{0x8b2a,2},{0x8b2c,4},{0x8b30,2},{0x8b32,2},{0x8b34,6},{0x8b3a,2},{0x8b3c,2},{0x8b3e,2},{0x8b40,2},{0x0,0},{0x8b42,4},{0x8b46,0},{0x8b46,0}}}},
 {0x8d06,0x8d3e,169,0x0,{{{0x8d06,4},{0x8d0a,8},{0x8d0a,8},{0x8d12,8},{0x8d1a,2},{0x8d1c,2},{0x8d1e,2},{0x8d20,2},{0x8d22,2},{0x8d24,4},{0x8d28,2},{0x8d2a,2},{0x8d2c,6},{0x8d32,2},{0x8d34,2},{0x8d36,2},{0x8d38,2},{0x0,0},{0x8d3a,4},{0x8d3e,0},{0x8d3e,0}}}},
 {0x8e64,0x8e9c,170,0x0,{{{0x8e64,4},{0x8e68,8},{0x8e68,8},{0x8e70,8},{0x8e78,2},{0x8e7a,2},{0x8e7c,2},{0x8e7e,2},{0x8e80,2},{0x8e82,4},{0x8e86,2},{0x8e88,2},{0x8e8a,6},{0x8e90,2},{0x8e92,2},{0x8e94,2},{0x8e96,2},{0x0,0},{0x8e98,4},{0x8e9c,0},{0x8e9c,0}}}},
 {0x9084,0x90c0,58,0x6b,{{{0x9084,4},{0x9088,8},{0x9088,8},{0x9090,8},{0x9098,2},{0x909a,2},{0x909c,2},{0x909e,2},{0x90a0,2},{0x90a2,4},{0x90a6,2},{0x90a8,2},{0x90aa,6},{0x90b0,2},{0x90b2,2},{0x90b4,2},{0x90b6,2},{0x90b8,4},{0x90bc,4},{0x90c0,0},{0x90c0,0}}}},
 {0x90c0,0x90fc,61,0x6f,{{{0x90c0,4},{0x90c4,8},{0x90c4,8},{0x90cc,8},{0x90d4,2},{0x90d6,2},{0x90d8,2},{0x90da,2},{0x90dc,2},{0x90de,4},{0x90e2,2},{0x90e4,2},{0x90e6,6},{0x90ec,2},{0x90ee,2},{0x90f0,2},{0x90f2,2},{0x90f4,4},{0x90f8,4},{0x90fc,0},{0x90fc,0}}}},
 {0x90fc,0x9138,66,0x77,{{{0x90fc,4},{0x9100,8},{0x9100,8},{0x9108,8},{0x9110,2},{0x9112,2},{0x9114,2},{0x9116,2},{0x9118,2},{0x911a,4},{0x911e,2},{0x9120,2},{0x9122,6},{0x9128,2},{0x912a,2},{0x912c,2},{0x912e,2},{0x9130,4},{0x9134,4},{0x9138,0},{0x9138,0}}}},
}};
Point point(const VelocityModulationTask& task,unsigned binding){return bindings[binding].points[unsigned(task.stage())];}
}
void setVelocityModulationPartialForTests(VelocityModulationPartialTest fn){partialTest=fn;}
bool VelocityModulationCalls::candidate(unsigned p){return (p>=0x8932&&p<=0x896a)||(p>=0x89f4&&p<=0x8a2c)||(p>=0x8b0e&&p<=0x8b46)||(p>=0x8d06&&p<=0x8d3e)||(p>=0x8e64&&p<=0x8e9c)||(p>=0x9084&&p<=0x90c0)||(p>=0x90c0&&p<=0x90fc)||(p>=0x90fc&&p<=0x9138);}
void VelocityModulationCalls::retireReturns(ControllerCpu& h){for(auto& c:calls)if(c&&c->token==h.getSP()&&point(c->task,c->binding).location==unsigned(h.getPC())&&(c->task.child()==VelocityModulationTask::Child::Word)){c->task.resumeChild();c.reset();++completed;}}
bool VelocityModulationCalls::step(ControllerCpu& h){using namespace controllerDetail;using C=VelocityModulationTask::Child;
 const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 // Inline blocks have no return instruction of their own. DSP completion is
 // admitted when the original caller can consume its next boundary; no extra
 // fetch or clock is introduced by retiring this native logical context.
 if(selected&&(*selected)->task.child()==C::Word&&point((*selected)->task,(*selected)->binding).location==pc){
  (*selected)->task.resumeChild();selected->reset();selected=nullptr;++completed;
 }
 for(unsigned binding=0;binding<bindings.size();++binding)if(bindings[binding].entry==pc){
  if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native velocity-modulation capacity exceeded");
  const auto& b=bindings[binding];selected->emplace(bool(b.offset),b.offset,h.getSP(),binding);++started;break;
 }
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& v=h.regs;const auto& binding=bindings[call.binding];
 if(task.child()==C::Voice){const auto pointer=uint32_t(v[3].er);if(pointer<0x17560||pointer>=0x17560 + 20*256||(pointer-0x17560)%20)throw std::runtime_error("Invalid velocity-modulation voice binding");task.resumeChild(uint8_t((pointer-0x17560)/20));}
 const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task,call.binding).length);const auto cost=task.nextClocks();
 auto read=[&](VelocityModulationTask::Read what)->uint32_t{if(what==VelocityModulationTask::Read::Velocity)return uint8_t(h.read8(0x17560 + unsigned(task.slot())*20+6));return uint8_t(h.read8(int32_t(uint32_t(v[6].er)+binding.depth)));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native velocity-modulation admission mismatch");
 auto dispatch=[&](unsigned target){clocks.call(target);h.pc=h.makepc(clocks.next);if(h.cycles-before!=cost)throw std::runtime_error("Native velocity-modulation dispatch clocks differ");};
 switch(stage){case S::DispatchVoice:dispatch(0xa656);return true;
 case S::ReadVelocity:case S::InvertVelocity:v[1].rl=int8_t(task.velocity());value(h,v[1].rl);break;case S::ReadDepth:v[1].rh=int8_t(task.depth());value(h,v[1].rh);break;
 case S::BiasDepth:v[1].rh=h.add8(v[1].rh,-64);break;case S::NegateDepth:v[1].rh=h.sub8(0,v[1].rh);break;case S::BiasVelocity:v[1].rl=h.add8(v[1].rl,-127);break;
 case S::Multiply:v[1].r=task.product();flag(h,8,v[1].r<0);flag(h,4,!v[1].r);h.clockI(12);break;case S::Scale:clocks.shift(v[1].r,2,true);break;
 case S::SetUnity:v[1].er=0x7fffff00;value(h,v[1].er);break;case S::BiasHigh:v[1].rh=h.add8(v[1].rh,127);break;
 case S::CaptureWord:v[1].e=v[1].r;value(h,v[1].e);break;case S::ClearMiddle:v[1].rh=h.sub8(v[1].rh,v[1].rh);break;
 case S::SetOffset:v[4].r=int16_t(task.offset());value(h,v[4].r);break;
 case S::DispatchWord:if((uint32_t(v[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native velocity-modulation payload projection differs");dispatch(task.relative()?0x785a:0x2520);return true;
 default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native velocity-modulation bus clocks differ");h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
