#include "ksNoteOnStateCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
NoteOnStatePartialTest partialTest=nullptr;using S=NoteOnStateTask::Stage;using I=NoteOnStateTask::Input;
struct Point {unsigned location,length;};
Point point(const NoteOnStateTask& t){switch(t.stage()){
 case S::RetainPart:return {0x2efc,4};
 case S::RetainIndex:return {0x2f00,4};
 case S::RetainScratch:return {0x2f04,4};
 case S::DispatchActivity:return {0x2f08,4};
 case S::ObserveActivity:return {0x2f0c,2};
 case S::AdmitActivity:return {0x2f0e,2};
 case S::RetainNote:return {0x2f10,2};
 case S::RetainVelocity:return {0x2f12,2};
 case S::DispatchMonoReset:return {0x2f14,4};
 case S::ReleaseVelocity:return {0x2f18,2};
 case S::ReleaseNote:return {0x2f1a,2};
 case S::DispatchPart:return {0x2f1c,4};
 case S::ClearIndex:return {0x2f20,2};
 case S::ReadCount:return {0x2f22,8};
 case S::CompareCount:return {0x2f2a,2};
 case S::AdmitAppend:return {0x2f2c,2};
 case S::CaptureCount:return {0x2f2e,2};
 case S::LocateEntry:return {0x2f30,2};
 case S::BindEntry:return {0x2f32,2};
 case S::StoreEntry:return {0x2f34,8};
 case S::StoreLastEvent:return {0x2f3c,6};
 case S::IncrementCount:return {0x2f42,2};
 case S::StoreHeldCount:return {0x2f44,8};
 case S::StoreDownCount:return {0x2f4c,8};
 case S::DispatchLivePart:return {0x2f54,4};
 case S::ClearVelocityIndex:return {0x2f58,2};
 case S::CaptureVelocityKey:return {0x2f5a,2};
 case S::BindVelocity:return {0x2f5c,2};
 case S::StoreVelocity:return {0x2f5e,8};
 case S::ClearMapIndex:return {0x2f66,2};
 case S::CaptureMapKey:return {0x2f68,2};
 case S::ScaleMapQuarter:return {0x2f6a,2};
 case S::ScaleMapHalf:return {0x2f6c,2};
 case S::BindMap:return {0x2f6e,2};
 case S::LocatePhysical:return {0x2f70,6};
 case S::PublishPhysical:return {0x2f76,4};
 case S::LocateHeld:return {0x2f7a,6};
 case S::PublishHeld:return {0x2f80,4};
 case S::ReleaseScratch:return {0x2f84,4};
 case S::ReleaseIndex:return {0x2f88,4};
 case S::ReleasePart:return {0x2f8c,4};
 case S::Finish:return {0x2f90,2};
 case S::AwaitActivity:return {0x2f0c,2};case S::AwaitMonoReset:return {0x2f18,2};case S::AwaitPart:return {0x2f20,2};case S::AwaitLivePart:return {0x2f58,2};default:break;
 }throw std::runtime_error("Invalid note-on state stage");}
}
void setNoteOnStatePartialForTests(NoteOnStatePartialTest fn){partialTest=fn;}
bool NoteOnStateCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 if(pc==0x2efc){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
  if(!selected)throw std::runtime_error("Native note-on state capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& v=h.regs;
 if(task.awaitingActivity())task.resumeActivity(uint16_t(v[1].r));
 if(task.awaitingMonoReset())task.resumeMonoReset();
 if(task.awaitingPart()){const uint32_t address=uint32_t(v[0].er);if(address<0x160d2||address>=0x160d2 + 256*334||(address-0x160d2)%334)throw std::runtime_error("Invalid note-on part binding");task.resumePart(uint8_t((address-0x160d2)/334));}
 const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task).length);const unsigned base=0x160d2 + unsigned(task.part())*334;
 auto timing=[&](unsigned offset){const auto address=base+offset;return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};
 auto read=[&](I input,unsigned at)->unsigned {switch(input){case I::MidiWord:return uint16_t(v[5].r);case I::Note:return uint8_t(v[5].rl);case I::Velocity:return uint8_t(v[5].rh);default:return uint8_t(h.read8(base+at));}};
 auto publish=[&](unsigned at,unsigned data,unsigned bytes){if(bytes==2)h.write16(int16_t(data),base+at);else if(bytes==1)h.write8(int8_t(data),base+at);else h.write16(int16_t(data),0x15c2e);};
 const auto cost=task.nextClocks(task.bytePublication()?timing(task.publicationOffset()):3u);
 if(task.advanceBefore(cost,read,publish,timing)!=cost)throw std::runtime_error("Native note-on state admission mismatch");
 bool dispatched=false,finished=false;
 switch(stage){case S::RetainPart:clocks.push(v[0].er);call.depth+=4;break;case S::RetainIndex:clocks.push(v[1].er);call.depth+=4;break;
 case S::RetainScratch:clocks.push(v[2].er);call.depth+=4;break;case S::RetainNote:clocks.push(v[5].r);call.depth+=2;break;
 case S::RetainVelocity:clocks.push(v[6].r);call.depth+=2;break;
 case S::DispatchActivity:clocks.call(0x30e4);dispatched=true;break;case S::DispatchMonoReset:clocks.call(0x3f80);dispatched=true;break;
 case S::DispatchPart:case S::DispatchLivePart:clocks.call(0x4000);dispatched=true;break;
 case S::ObserveActivity:value(h,v[1].r);break;
 case S::ReleaseVelocity:clocks.pop(v[6].r);call.depth-=2;break;case S::ReleaseNote:clocks.pop(v[5].r);call.depth-=2;break;
 case S::ClearIndex:case S::ClearVelocityIndex:case S::ClearMapIndex:v[1].er=h.sub32(v[1].er,v[1].er);break;
 case S::ReadCount:v[1].rl=int8_t(task.count());value(h,v[1].rl);break;case S::CompareCount:h.sub8(v[1].rl,0x20);break;
 case S::CaptureCount:v[2].rl=v[1].rl;value(h,v[2].rl);break;case S::LocateEntry:clocks.shift(v[1].rl,1,true);break;
 case S::BindEntry:case S::BindVelocity:case S::BindMap:v[1].er=h.add32(v[1].er,v[0].er);break;
 case S::StoreEntry:case S::StoreLastEvent:value(h,v[5].r);break;
 case S::IncrementCount:clocks.inc(v[2].rl,1);break;case S::StoreHeldCount:case S::StoreDownCount:value(h,v[2].rl);break;
 case S::CaptureVelocityKey:case S::CaptureMapKey:v[1].rl=v[5].rl;value(h,v[1].rl);break;
 case S::StoreVelocity:value(h,v[5].rh);break;case S::ScaleMapQuarter:clocks.shift(v[1].rl,2,false);break;
 case S::ScaleMapHalf:clocks.shift(v[1].rl,1,false);break;
 case S::LocatePhysical:v[1].er=h.add32(v[1].er,0xc0);break;case S::LocateHeld:v[1].er=h.add32(v[1].er,0x10);break;
 case S::ReleaseScratch:clocks.pop(v[2].er);call.depth-=4;break;case S::ReleaseIndex:clocks.pop(v[1].er);call.depth-=4;break;
 case S::ReleasePart:clocks.pop(v[0].er);call.depth-=4;break;
 case S::Finish:clocks.ret();selected->reset();++completed;finished=true;break;default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native note-on state bus clocks differ");h.pc=h.makepc(dispatched||finished?clocks.next:point(task).location);return true;
}
}
