#include "ksHardwareSetupCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks{namespace{HardwareSetupPartialTest partialTest=nullptr;using T=HardwareSetupTask;using K=T::Kind;using F=T::Field;
struct Point{unsigned location,length;};Point point(const T&t){switch(t.kind()){case K::Pins:{static constexpr std::array<Point,23> values{{{0x456,2},{0x458,2},{0x45a,2},{0x45c,2},{0x45e,2},{0x460,4},{0x464,2},{0x466,2},{0x468,2},{0x46a,4},{0x46e,2},{0x470,4},{0x474,2},{0x476,4},{0x47a,2},{0x47c,2},{0x47e,2},{0x480,4},{0x484,2},{0x486,2},{0x488,2},{0x48a,4},{0x48e,2}}};return values[t.position()];}case K::WaitStates:{static constexpr std::array<Point,9> values{{{0x4d6,2},{0x4d8,4},{0x4dc,2},{0x4de,4},{0x4e2,2},{0x4e4,4},{0x4e8,2},{0x4ea,4},{0x4ee,2}}};return values[t.position()];}case K::Dram:{static constexpr std::array<Point,9> values{{{0x4f0,2},{0x4f2,4},{0x4f6,2},{0x4f8,4},{0x4fc,2},{0x4fe,2},{0x500,2},{0x502,2},{0x504,2}}};return values[t.position()];}case K::Clock:{static constexpr std::array<Point,20> values{{{0x506,6},{0x50c,4},{0x510,2},{0x512,2},{0x514,2},{0x516,2},{0x518,2},{0x51a,6},{0x520,2},{0x522,4},{0x526,2},{0x528,2},{0x52a,2},{0x52c,2},{0x52e,6},{0x534,4},{0x538,2},{0x53a,2},{0x53c,2},{0x53e,2}}};return values[t.position()];}}throw std::runtime_error("Invalid native hardware setup stage");}
unsigned address(F f){switch(f){case F::P3Direction:return 0xffff39u;case F::BusControl:return 0xffffacu;case F::PBControl:return 0xfffffeb1u;case F::InputSelect:return 0xffff62u;case F::PCControl:return 0xfffffeb2u;case F::PEControl:return 0xfffffeb5u;case F::PHControl:return 0xfffffeb9u;case F::P9Direction:return 0xffff45u;case F::PortFDirection:return 0xfffffebeu;case F::SystemControl:return 0xffff6fu;case F::PortGDirection:return 0xfffffebfu;case F::WaitControl:return 0xfffffed1u;case F::Wait0:return 0xfffffed2u;case F::Wait1:return 0xfffffed3u;case F::WaitEnable:return 0xfffffed4u;case F::DramMode:return 0xfffffec8u;case F::DramRefresh:return 0xfffffec9u;case F::P4Direction:return 0xffff3cu;case F::P4Data:return 0xffff3du;case F::TimerPeriod:return 0xffffffd8u;case F::TimerControl:return 0xffffd0u;case F::TimerEnable:return 0xffffd4u;case F::ClockCounter:return 0xffec10u;case F::InterruptControl:return 0xffffe0u;case F::PriorityControl:return 0xfffff1u;case F::ClockMode:return 0xfffffff6u;case F::InterruptMode:return 0xffffc0u;default:throw std::runtime_error("Invalid native hardware setup field");}}
}
void setHardwareSetupPartialForTests(HardwareSetupPartialTest fn){partialTest=fn;}
bool HardwareSetupCalls::step(ControllerCpu&h){using namespace controllerDetail;unsigned pc=unsigned(h.getPC());std::optional<Call>*selected=nullptr;for(auto&c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
const unsigned roots[]={0x456,0x4d6,0x4f0,0x506};for(unsigned i=0;i<4;++i)if(pc==roots[i]){if(selected)throw std::runtime_error("Overlapping native hardware setup");for(auto&c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native hardware setup capacity exceeded");selected->emplace(K(i),h.getSP());++started;break;}
if(!selected||point((*selected)->task).location!=pc){if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
auto&task=(*selected)->task;auto&v=h.regs;const auto phase=task.phase();const auto before=h.cycles;Boundary f(h,pc,point(task).length);auto timing=[&](T::Publication p){unsigned a=address(p.field)&0xffffff,used=0;int previous=h.lastwrite;for(unsigned n=0;n<p.width;++n){const unsigned at=a+n;if(at>=0xffffa0){++used;previous=-1;}else if(at>=0xfffd10){if((previous&1)||at!=(unsigned(previous)|1u))++used;previous=int(at);}else{used+=3;previous=-1;}}return used;};const auto cost=task.nextClocks(phase==T::Phase::Publish?timing(task.publication()):0);
if(task.advanceBefore(cost,[&](T::Publication p){const auto a=address(p.field);if(p.width==1)h.write8(int8_t(p.value),a);else if(p.width==2)h.write16(int16_t(p.value),a);else h.write32(int32_t(p.value),a);},timing)!=cost)throw std::runtime_error("Native hardware setup admission mismatch");
switch(pc){
case 0x456:f.move(v[0].rl,0x25);break;
case 0x458:value(h,v[0].rl);break;
case 0x45a:f.move(v[0].rl,0x18);break;
case 0x45c:value(h,v[0].rl);break;
case 0x45e:f.move(v[0].rl,0xffffffff);break;
case 0x460:value(h,v[0].rl);break;
case 0x464:f.move(v[0].rl,0x30);break;
case 0x466:value(h,v[0].rl);break;
case 0x468:f.move(v[0].rl,0x3f);break;
case 0x46a:value(h,v[0].rl);break;
case 0x46e:f.move(v[0].rl,3);break;
case 0x470:value(h,v[0].rl);break;
case 0x474:f.move(v[0].rl,0x10);break;
case 0x476:value(h,v[0].rl);break;
case 0x47a:f.move(v[0].rl,0);break;
case 0x47c:value(h,v[0].rl);break;
case 0x47e:f.move(v[0].rl,0xffffff8f);break;
case 0x480:value(h,v[0].rl);break;
case 0x484:f.move(v[0].rl,1);break;
case 0x486:value(h,v[0].rl);break;
case 0x488:f.move(v[0].rl,0x1f);break;
case 0x48a:value(h,v[0].rl);break;
case 0x48e:f.ret();break;
case 0x4d6:f.move(v[0].rl,0xffffffdf);break;
case 0x4d8:value(h,v[0].rl);break;
case 0x4dc:f.move(v[0].rl,0x55);break;
case 0x4de:value(h,v[0].rl);break;
case 0x4e2:f.move(v[0].rl,0x55);break;
case 0x4e4:value(h,v[0].rl);break;
case 0x4e8:f.move(v[0].rl,0xffffff80);break;
case 0x4ea:value(h,v[0].rl);break;
case 0x4ee:f.ret();break;
case 0x4f0:f.move(v[0].rl,0x76);break;
case 0x4f2:value(h,v[0].rl);break;
case 0x4f6:f.move(v[0].rl,0x66);break;
case 0x4f8:value(h,v[0].rl);break;
case 0x4fc:f.move(v[0].rl,0x19);break;
case 0x4fe:value(h,v[0].rl);break;
case 0x500:f.move(v[0].rl,0x7f);break;
case 0x502:value(h,v[0].rl);break;
case 0x504:f.ret();break;
case 0x506:f.move(v[0].er,0xffffffd8);break;
case 0x50c:f.move(v[1].r,0x30d4);break;
case 0x510:value(h,v[1].r);break;
case 0x512:f.move(v[0].rl,0x20);break;
case 0x514:value(h,v[0].rl);break;
case 0x516:f.move(v[0].rl,0x1);break;
case 0x518:value(h,v[0].rl);break;
case 0x51a:f.move(v[0].er,0xffec10);break;
case 0x520:v[1].er=h.sub32(v[1].er,v[1].er);break;
case 0x522:value(h,v[1].er);break;
case 0x526:f.move(v[0].rl,0x6);break;
case 0x528:value(h,v[0].rl);break;
case 0x52a:f.move(v[0].rl,0x4);break;
case 0x52c:value(h,v[0].rl);break;
case 0x52e:f.move(v[0].er,0xfffffff6);break;
case 0x534:f.move(v[1].r,0x2);break;
case 0x538:value(h,v[1].r);break;
case 0x53a:f.move(v[0].rl,0x7);break;
case 0x53c:value(h,v[0].rl);break;
case 0x53e:f.ret();break;
default:break;}
if(h.cycles-before!=cost)throw std::runtime_error("Native hardware setup clocks differ");
if(phase==T::Phase::Finish){h.pc=h.makepc(f.next);selected->reset();++completed;}else h.pc=h.makepc(point(task).location);return true;}
}
