#include "ksKeyResetCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
KeyResetPartialTest partialTest=nullptr;
using S=KeyResetTask::Stage;using K=KeyResetTask::Kind;
struct Point {unsigned location,length;};
Point point(const KeyResetTask& task){const unsigned base=task.kind()==K::Held?0x3074:0x30b8;
 switch(task.stage()){
 case S::RetainPart:return {base,4};case S::RetainBitmap:return {base+4,4};case S::RetainCount:return {base+8,2};
 case S::DispatchPart:return {base+10,4};case S::AwaitPart:case S::BindBitmap:return {base+14,2};
 case S::LocateEnd:return {base+16,6};case S::SetCount:return {base+22,4};case S::ClearByte:return {base+26,2};
 case S::Decrement:return {base+28,2};case S::AdmitMore:return {base+30,2};
 case S::ClearOrdered:return {0x3094,8};case S::ClearPhysical:return {0x309c,8};case S::ClearCursor:return {0x30a4,8};
 case S::ReleaseCount:return {task.kind()==K::Held?0x30acu:0x30d8u,2};
 case S::ReleaseBitmap:return {task.kind()==K::Held?0x30aeu:0x30dau,4};case S::ReleasePart:return {task.kind()==K::Held?0x30b2u:0x30deu,4};
 case S::Complete:return {task.kind()==K::Held?0x30b6u:0x30e2u,2};
 }throw std::runtime_error("Invalid key reset stage");}
}
void setKeyResetPartialForTests(KeyResetPartialTest fn){partialTest=fn;}
bool KeyResetCalls::step(ControllerCpu& h){using namespace controllerDetail;
 const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 if(pc==0x3074||pc==0x30b8){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
  if(!selected)throw std::runtime_error("Native key reset capacity exceeded");selected->emplace(pc==0x3074?K::Held:K::Down,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& v=h.regs;
 if(task.awaitingPart()){const uint32_t address=uint32_t(v[0].er);if(address<0x160d2||address>=0x160d2 + 256*334||(address-0x160d2)%334)throw std::runtime_error("Invalid key reset part binding");task.resumePart(uint8_t((address-0x160d2)/334));}
 const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task).length);
 if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
 const unsigned base=0x160d2 + unsigned(task.part())*334;
 auto timing=[&](unsigned offset){const auto address=base+offset;return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};
 const auto cost=task.nextClocks(task.publication()?timing(task.publicationOffset()):3u);
 auto write=[&](unsigned offset,unsigned data){h.write8(int8_t(data),base+offset);};
 if(task.advanceBefore(cost,write,timing)!=cost)throw std::runtime_error("Native key reset admission mismatch");
 switch(stage){case S::RetainPart:clocks.push(v[0].er);call.depth+=4;break;case S::RetainBitmap:clocks.push(v[1].er);call.depth+=4;break;
 case S::RetainCount:clocks.push(v[2].r);call.depth+=2;break;case S::DispatchPart:clocks.call(0x4000);h.pc=h.makepc(clocks.next);
  if(h.cycles-before!=cost)throw std::runtime_error("Native key reset dispatch clocks differ");return true;
 case S::BindBitmap:v[1].er=int32_t(base);value(h,v[1].er);break;
 case S::LocateEnd:v[1].er=h.add32(v[1].er,task.kind()==K::Held?0xe0:0xd0);break;
 case S::SetCount:v[2].r=16;value(h,v[2].r);break;
 case S::ClearByte:--v[1].er;value(h,v[2].rh);h.clockI(2);break;
 case S::Decrement:clocks.inc(v[2].rl,-1);break;
 case S::ClearOrdered:case S::ClearPhysical:case S::ClearCursor:value(h,v[2].rh);break;
 case S::ReleaseCount:clocks.pop(v[2].r);call.depth-=2;break;case S::ReleaseBitmap:clocks.pop(v[1].er);call.depth-=4;break;
 case S::ReleasePart:clocks.pop(v[0].er);call.depth-=4;break;default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native key reset bus clocks differ");h.pc=h.makepc(point(task).location);return true;
}
}
