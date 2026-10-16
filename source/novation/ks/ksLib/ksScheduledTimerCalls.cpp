#include "ksScheduledTimerCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace {
ScheduledTimerPartialTest partialTest=nullptr;using S=ScheduledTimerTask::Stage;using K=ScheduledTimerTask::Kind;using F=ScheduledTimerTask::Field;
struct Point {unsigned location,length;};
Point point(const ScheduledTimerTask& t){const unsigned b=0x4016+0x5c*t.slot();switch(t.stage()){
 case S::Clear:return {b,2};case S::StoreClear:return {b+2,6};case S::StoreAction:return {b+10,8};case S::StoreInterval:return {b+18,6};
 case S::DispatchClock:return {b+(t.kind()==K::Arm?24u:50u),4};case S::AwaitClock:return {b+(t.kind()==K::Arm?28u:54u),6};case S::StoreStarted:return {b+28,6};
 case S::RetainInterval:return {b+36,2};case S::ReadInterval:return {b+38,6};case S::AdmitInterval:return {b+44,2};case S::RetainClock:return {b+46,4};
 case S::ReadStarted:return {b+54,6};case S::Elapsed:return {b+60,2};case S::Compare:return {b+62,2};case S::AdmitDeadline:return {b+64,2};
 case S::ZeroInterval:return {b+66,2};case S::StoreExpired:return {b+68,6};case S::ReadAction:return {b+74,8};case S::DispatchAction:return {b+82,2};
 case S::AwaitAction:case S::ReleaseClock:return {b+84,4};case S::ReleaseInterval:return {b+88,2};
 case S::Finish:return {b+(t.kind()==K::Cancel?8u:t.kind()==K::Arm?34u:90u),2};default:break;}
 throw std::runtime_error("Invalid scheduled timer stage");}
}
void setScheduledTimerPartialForTests(ScheduledTimerPartialTest fn){partialTest=fn;}
bool ScheduledTimerCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 if(candidate(pc)){const unsigned local=(pc-0x4016)%0x5c;if(local==0||local==10||local==36){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
  if(!selected)throw std::runtime_error("Native scheduled timer capacity exceeded");selected->emplace(local==0?K::Cancel:local==10?K::Arm:K::Service,(pc-0x4016)/0x5c,h.getSP());++started;}}
 if(!selected||point((*selected)->task).location!=pc){if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& v=h.regs;
 if(task.awaitingClock())task.resumeClock(uint16_t(v[6].r));if(task.awaitingAction())task.resumeAction();
 const auto stage=task.stage();const auto before=h.cycles;Boundary f(h,pc,point(task).length);const unsigned base=0x16758+task.slot()*8;
 auto address=[&](F field){return base+(field==F::Started?0:field==F::Interval?2:4);};
 auto read=[&](F field,bool argument)->uint32_t {if(argument)return field==F::Action?uint32_t(v[5].er):uint16_t(v[6].r);return field==F::Action?h.read32(address(field)):h.read16(address(field));};
 auto write=[&](F field,uint32_t value){if(field==F::Action)h.write32(int32_t(value),address(field));else h.write16(int16_t(value),address(field));};
 const auto cost=task.nextClocks();if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native scheduled timer admission mismatch");
 bool dispatched=false,finished=false;
 switch(stage){case S::Clear:v[0].r=h.sub16(v[0].r,v[0].r);break;case S::StoreClear:value(h,v[0].r);break;
 case S::StoreAction:value(h,v[5].er);break;case S::StoreInterval:case S::StoreStarted:value(h,v[6].r);break;
 case S::DispatchClock:f.call(0x12fa);dispatched=true;break;case S::RetainInterval:f.push(v[5].r);call.depth+=2;break;
 case S::ReadInterval:v[5].r=int16_t(task.interval());value(h,v[5].r);break;case S::RetainClock:f.push(v[6].er);call.depth+=4;break;
 case S::ReadStarted:v[6].e=int16_t(task.startedTime());value(h,v[6].e);break;case S::Elapsed:v[6].r=h.sub16(v[6].r,v[6].e);break;
 case S::Compare:h.sub16(v[6].r,v[5].r);break;case S::ZeroInterval:v[6].e=h.sub16(v[6].e,v[6].e);break;case S::StoreExpired:value(h,v[6].e);break;
 case S::ReadAction:v[6].er=int32_t(task.action().value);value(h,v[6].er);break;case S::DispatchAction:f.call(task.action().value&0xffffff);dispatched=true;break;
 case S::ReleaseClock:f.pop(v[6].er);call.depth-=4;break;case S::ReleaseInterval:f.pop(v[5].r);call.depth-=2;break;
 case S::Finish:f.ret();selected->reset();++completed;finished=true;break;default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native scheduled timer clocks differ");h.pc=h.makepc(dispatched||finished?f.next:point(task).location);return true;
}
}
