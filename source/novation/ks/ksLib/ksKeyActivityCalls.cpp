#include "ksKeyActivityCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
KeyActivityPartialTest partialTest=nullptr;
using S=KeyActivityTask::Stage;using K=KeyActivityTask::Kind;
struct Point {unsigned location,length;};
Point point(const KeyActivityTask& t){const unsigned b=t.kind()==K::Held?0x30fe:0x30e4;
 switch(t.stage()){
 case S::DispatchPart:return {b,4};case S::AwaitPart:case S::BindMap:return {b+4,2};case S::LocateMap:return {b+6,6};
 case S::SetRemaining:return {b+12,4};case S::ReadWord:return {b+16,2};case S::AdmitWord:return {b+18,2};
 case S::Decrement:return {b+20,2};case S::AdmitMore:return {b+22,2};case S::Finish:return {b+24,2};default:break;
 }throw std::runtime_error("Invalid key activity stage");}
}
void setKeyActivityPartialForTests(KeyActivityPartialTest fn){partialTest=fn;}
bool KeyActivityCalls::step(ControllerCpu& h){using namespace controllerDetail;
 const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0x30e4||pc==0x30fe){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
  if(!selected)throw std::runtime_error("Native key activity capacity exceeded");selected->emplace(pc==0x30fe?K::Held:K::Physical,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& task=(*selected)->task;auto& v=h.regs;
 if(task.awaitingPart()){const uint32_t address=uint32_t(v[0].er);if(address<0x160d2||address>=0x160d2 + 256*334||(address-0x160d2)%334)throw std::runtime_error("Invalid key activity part binding");task.resumePart(uint8_t((address-0x160d2)/334));}
 const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task).length);
 const auto cost=task.nextClocks();const unsigned base=0x160d2 + unsigned(task.part())*334;
 auto read=[&](unsigned offset){return uint16_t(h.read16(base+offset));};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native key activity admission mismatch");
 switch(stage){case S::DispatchPart:clocks.call(0x4000);h.pc=h.makepc(clocks.next);break;
 case S::BindMap:v[2].er=int32_t(base);value(h,v[2].er);break;
 case S::LocateMap:v[2].er=h.add32(v[2].er,task.kind()==K::Held?0xd0:0xc0);break;
 case S::SetRemaining:v[1].e=8;value(h,v[1].e);break;
 case S::ReadWord:v[1].r=int16_t(task.result());value(h,v[1].r);v[2].er+=2;h.clockI(2);break;
 case S::Decrement:clocks.inc(v[1].e,-1);break;
 case S::Finish:clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;break;
 default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native key activity clocks differ");
 if(stage!=S::DispatchPart&&stage!=S::Finish)h.pc=h.makepc(point(task).location);return true;
}
}
