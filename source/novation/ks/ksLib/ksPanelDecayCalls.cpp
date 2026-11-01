#include "ksPanelDecayCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks{namespace{PanelDecayPartialTest partialTest=nullptr;using T=PanelDecayTask;using S=T::Stage;struct Point{unsigned location,length;};Point point(S s){switch(s){case S::RetainCaller:return {0x70e,4};case S::BindFirst:return {0x712,6};case S::ClearFirstChanges:return {0x718,2};case S::SetFirstCount:return {0x71a,4};case S::ReadFirst:return {0x71e,2};case S::AdmitFirst:return {0x720,2};case S::DecayFirst:return {0x722,2};case S::PublishFirst:return {0x724,2};case S::AdmitFirstZero:return {0x726,2};case S::MarkFirstChanged:return {0x728,2};case S::AdvanceFirst:return {0x72a,2};case S::TickFirst:return {0x72c,2};case S::LoopFirst:return {0x72e,2};case S::ReadFirstPending:return {0x730,6};case S::MergeFirstPending:return {0x736,2};case S::PublishFirstPending:return {0x738,6};case S::BindSecond:return {0x73e,6};case S::ClearSecondChanges:return {0x744,2};case S::SetSecondCount:return {0x746,4};case S::ReadSecond:return {0x74a,2};case S::AdmitSecond:return {0x74c,2};case S::DecaySecond:return {0x74e,2};case S::PublishSecond:return {0x750,2};case S::AdmitSecondZero:return {0x752,2};case S::MarkSecondChanged:return {0x754,2};case S::AdvanceSecond:return {0x756,2};case S::TickSecond:return {0x758,2};case S::LoopSecond:return {0x75a,2};case S::ReadSecondPending:return {0x75c,6};case S::MergeSecondPending:return {0x762,2};case S::PublishSecondPending:return {0x764,6};case S::RestoreCaller:return {0x76a,4};case S::ReadHoldoff:return {0x76e,6};case S::AdmitHoldoff:return {0x774,2};case S::TickHoldoff:return {0x776,2};case S::PublishHoldoff:return {0x778,6};case S::AdmitHoldoffExpiry:return {0x77e,2};case S::RetainLatch:return {0x780,2};case S::SetLatch:return {0x782,2};case S::DispatchLatch:return {0x784,4};case S::RestoreLatch:return {0x788,2};case S::Finish:return {0x78a,2};case S::AwaitLatch:return {0x788,2};default:throw std::runtime_error("Invalid panel decay stage");}}
unsigned cell(unsigned bank,unsigned i){return (bank?0xffec24u:0xffec16u)+i;}unsigned pending(unsigned bank){return bank?0xffec29u:0xffec1bu;}}
void setPanelDecayPartialForTests(PanelDecayPartialTest fn){partialTest=fn;}
bool PanelDecayCalls::step(ControllerCpu&h){using namespace controllerDetail;unsigned pc=unsigned(h.getPC());std::optional<Call>*selected=nullptr;for(auto&c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}if(pc==0x70e){if(selected)throw std::runtime_error("Overlapping panel decay caller");for(auto&c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Panel decay capacity exceeded");selected->emplace(h.getSP());++started;}
if(!selected||point((*selected)->task.stage()).location!=pc){if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto&call=**selected;auto&task=call.task;if(task.awaiting())task.resumeChild();auto stage=task.stage();auto before=h.cycles;Boundary f(h,pc,point(stage).length);auto&v=h.regs;auto cost=task.nextClocks();if(task.advanceBefore(cost,[&](T::Read what,unsigned bank,unsigned i){return uint8_t(h.read8(what==T::Read::Cell?cell(bank,i):what==T::Read::Pending?pending(bank):0xffec23));},[&](T::Write what,unsigned bank,unsigned i,uint8_t value){h.write8(int8_t(value),what==T::Write::Cell?cell(bank,i):what==T::Write::Pending?pending(bank):0xffec23);})!=cost)throw std::runtime_error("Panel decay admission mismatch");
switch(stage){
case S::RetainCaller:f.push(v[1].er);call.depth+=4;break;
case S::BindFirst:f.move(v[0].er,0xffec16);break;
case S::ClearFirstChanges:f.move(v[1].rl,0x0);break;
case S::SetFirstCount:f.move(v[1].e,0x4);break;
case S::ReadFirst:f.move(v[1].rh,task.cell());break;
case S::AdmitFirst:f.branch(bool(h.ccr&4),0x72a);break;
case S::DecayFirst:f.shift(v[1].rh,1,false);break;
case S::PublishFirst:value(h,v[1].rh);break;
case S::AdmitFirstZero:f.branch(!(h.ccr&4),0x72a);break;
case S::MarkFirstChanged:v[1].rl|=0x1;value(h,v[1].rl);break;
case S::AdvanceFirst:v[0].er=int32_t(uint32_t(v[0].er)+0x1);break;
case S::TickFirst:f.inc(v[1].e,-1);break;
case S::LoopFirst:f.branch(!(h.ccr&4),0x71e);break;
case S::ReadFirstPending:f.move(v[1].rh,task.pending());break;
case S::MergeFirstPending:v[1].rh|=v[1].rl;value(h,v[1].rh);break;
case S::PublishFirstPending:value(h,v[1].rh);break;
case S::BindSecond:f.move(v[0].er,0xffec24);break;
case S::ClearSecondChanges:f.move(v[1].rl,0x0);break;
case S::SetSecondCount:f.move(v[1].e,0x4);break;
case S::ReadSecond:f.move(v[1].rh,task.cell());break;
case S::AdmitSecond:f.branch(bool(h.ccr&4),0x756);break;
case S::DecaySecond:f.shift(v[1].rh,1,false);break;
case S::PublishSecond:value(h,v[1].rh);break;
case S::AdmitSecondZero:f.branch(!(h.ccr&4),0x756);break;
case S::MarkSecondChanged:v[1].rl|=0x1;value(h,v[1].rl);break;
case S::AdvanceSecond:v[0].er=int32_t(uint32_t(v[0].er)+0x1);break;
case S::TickSecond:f.inc(v[1].e,-1);break;
case S::LoopSecond:f.branch(!(h.ccr&4),0x74a);break;
case S::ReadSecondPending:f.move(v[1].rh,task.pending());break;
case S::MergeSecondPending:v[1].rh|=v[1].rl;value(h,v[1].rh);break;
case S::PublishSecondPending:value(h,v[1].rh);break;
case S::RestoreCaller:f.pop(v[1].er);call.depth-=4;break;
case S::ReadHoldoff:f.move(v[0].rl,task.holdoff());break;
case S::AdmitHoldoff:f.branch(bool(h.ccr&4),0x78a);break;
case S::TickHoldoff:f.inc(v[0].rl,-1);break;
case S::PublishHoldoff:value(h,v[0].rl);break;
case S::AdmitHoldoffExpiry:f.branch(!(h.ccr&4),0x78a);break;
case S::RetainLatch:f.push(v[6].r);call.depth+=2;break;
case S::SetLatch:f.move(v[6].rl,0xc);break;
case S::DispatchLatch:f.call(0x8fc);break;
case S::RestoreLatch:f.pop(v[6].r);call.depth-=2;break;
case S::Finish:f.ret();break;
default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Panel decay clocks differ");if(stage==S::DispatchLatch)h.pc=h.makepc(f.next);else if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;}else h.pc=h.makepc(point(task.stage()).location);return true;}}
