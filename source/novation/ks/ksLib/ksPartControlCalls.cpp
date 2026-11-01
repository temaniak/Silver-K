#include "ksPartControlCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
PartControlPartialTest partialTest=nullptr;
struct Point {unsigned location,length;};
Point point(const PartLookupTask& task){
 using S=PartLookupTask::Stage;using L=PartLookupTask::Layout;const auto layout=task.layout();
 const bool p=layout==L::Parameters,v=layout==L::Voice;
 switch(task.stage()){
 case S::ReadIndex:return {p?0x4000u:v?0xa656u:0xa66au,6};case S::NormalizeIndex:return {0x4006,2};
 case S::Stride:return {p?0x4008u:v?0xa65cu:0xa670u,p?4u:2u};
 case S::Product:return {p?0x400cu:v?0xa65eu:0xa672u,2};case S::NormalizeProduct:return {v?0xa660u:0xa674u,2};
 case S::Resolve:return {p?0x400eu:v?0xa662u:0xa676u,6};case S::Complete:return {p?0x4014u:v?0xa668u:0xa67cu,2};
 }throw std::runtime_error("Invalid native part lookup stage");
}
Point point(const PartMirrorTask& task){using S=PartMirrorTask::Stage;
 switch(task.stage()){case S::Normalize:return {0xa67e,2};case S::Count:return {0xa680,4};case S::Offset:return {0xa684,2};
 case S::Source:return {0xa686,6};case S::Destination:return {0xa68c,6};case S::Read:return {0xa692,2};case S::Write:return {0xa694,2};
 case S::Advance:return {0xa696,2};case S::Tick:return {0xa698,2};case S::Gate:return {0xa69a,2};case S::Complete:return {0xa69c,2};}
 throw std::runtime_error("Invalid native part mirror stage");
}
}
void setPartControlPartialForTests(PartControlPartialTest fn){partialTest=fn;}
bool PartControlCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto partial=[&]{if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;};
 if(pc>=0xa67e&&pc<=0xa69c){
  using S=PartMirrorTask::Stage;std::optional<Mirror>* selected=nullptr;for(auto& c:mirrors)if(c&&c->token==h.getSP()){selected=&c;break;}
  if(pc==0xa67e){if(!selected)for(auto& c:mirrors)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native part mirror capacity exceeded");selected->emplace(uint8_t(h.regs[0].rl),h.getSP());++mirrorStarted;}
  if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
  auto& task=(*selected)->task;const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);auto& r=h.regs;
  if(stage==S::Complete){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++mirrorCompleted;return true;}
  const auto publication=[&](unsigned at){const unsigned address=0x17512+at;return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};
  const auto cost=task.nextClocks(publication(task.position()));const auto progress=task.advanceBefore(cost,[&](unsigned part,unsigned at){return h.read8(0x174ae + 20*part+at);},[&](unsigned at,uint8_t value){h.write8(int8_t(value),0x17512 + at);},publication);
  if(progress!=cost)throw std::runtime_error("Native part mirror duration mismatch");
  switch(stage){case S::Normalize:r[0].r=int16_t(task.part());value(h,r[0].r);break;
  case S::Count:r[2].e=20;value(h,20);break;case S::Offset:r[0].er=int32_t(task.part()*20);h.clockI(20);break;
  case S::Source:r[0].er=h.add32(int32_t(task.part()*20),0x174ae);break;case S::Destination:r[1].er=0x17512;value(h,r[1].er);break;
  case S::Read:r[2].rl=int8_t(task.byte());value(h,r[2].rl);r[0].er=int32_t(0x174ae + 20*task.part()+task.position()+1);h.clockI(2);break;
  case S::Write:value(h,int8_t(task.byte()));break;case S::Advance:r[1].er=int32_t(0x17512 + task.position());value(h,r[1].er);flag(h,2,false);break;
  case S::Tick:r[2].e=int16_t(task.remaining());value(h,r[2].e);flag(h,2,false);break;default:break;}
  if(h.cycles-begin!=cost)throw std::runtime_error("Native part mirror bus admission mismatch");
  h.pc=h.makepc(point(task).location);return true;
 }
 using L=PartLookupTask::Layout;using S=PartLookupTask::Stage;std::optional<Lookup>* selected=nullptr;
 for(auto& c:lookups)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0x4000||pc==0xa656||pc==0xa66a||(pc==0xa670&&(!selected||point((*selected)->task).location!=pc))){
  if(!selected)for(auto& c:lookups)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native part lookup capacity exceeded");
  auto task=pc==0xa670?PartLookupTask::controlsFor(uint8_t(h.regs[0].rl)):PartLookupTask(pc==0x4000?L::Parameters:pc==0xa656?L::Voice:L::Controls);
  selected->emplace(task,h.getSP());++lookupStarted;
 }
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
 auto& task=(*selected)->task;const auto stage=task.stage();const auto layout=task.layout();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);auto& output=h.regs[layout==L::Voice?3:0];
 if(stage==S::Complete){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++lookupCompleted;return true;}
 const auto cost=task.nextClocks();if(task.advanceBefore(cost,[&](L which){return h.read8(which==L::Voice?0x174a7:0x17526);})!=cost)throw std::runtime_error("Native part lookup duration mismatch");
 switch(stage){case S::ReadIndex:output.rl=int8_t(task.index());value(h,output.rl);break;case S::NormalizeIndex:output.r=int16_t(task.index());value(h,output.r);break;
 case S::Stride:if(layout==L::Parameters){output.e=int16_t(task.stride());value(h,output.e);}else{output.rh=int8_t(task.stride());value(h,output.rh);}break;
 case S::Product:if(layout==L::Parameters){output.er=int32_t(task.offset());h.clockI(20);}else{output.r=int16_t(task.offset());h.clockI(12);}break;
 case S::NormalizeProduct:output.er=int32_t(task.offset());value(h,output.er);break;
 case S::Resolve:output.er=h.add32(int32_t(task.offset()),layout==L::Parameters?0x160d2:layout==L::Voice?0x17560:0x174ae);break;default:break;}
 if(h.cycles-begin!=cost)throw std::runtime_error("Native part lookup bus admission mismatch");
 h.pc=h.makepc(point(task).location);return true;
}
}
