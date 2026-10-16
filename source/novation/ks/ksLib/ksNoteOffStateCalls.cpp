#include "ksNoteOffStateCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
NoteOffStatePartialTest partialTest=nullptr;
using T=NoteOffStateTask;using S=T::Stage;
struct Point{unsigned location,length;};
Point point(const T& t){switch(t.stage()){
 case S::RetainPart:return {0x2f92,4};case S::RetainMap:return {0x2f96,4};case S::RetainPedal:return {0x2f9a,2};case S::ClearIndex:return {0x2f9c,2};case S::CaptureNote:return {0x2f9e,2};case S::IndexQuarter:return {0x2fa0,2};case S::IndexEighth:return {0x2fa2,2};case S::RetainIndex:return {0x2fa4,4};
 case S::DispatchPart:return {0x2fa8,4};case S::AwaitPart:case S::ReadPedal:return {0x2fac,8};case S::ObservePedal:return {0x2fb4,2};case S::AdmitPedal:return {0x2fb6,2};case S::BindHeld:return {0x2fb8,2};case S::LocateHeld:return {0x2fba,6};case S::ClearHeld:return {0x2fc0,4};case S::DispatchCompact:return {0x2fc4,2};
 case S::AwaitCompact:case S::ReadMode:return {0x2fc6,8};case S::ObserveMode:return {0x2fce,2};case S::AdmitMode:return {0x2fd0,2};case S::DispatchHeld:return {0x2fd2,4};case S::AwaitHeld:case S::AdmitHeld:return {0x2fd6,2};case S::RetainUpper:return {0x2fd8,2};case S::RetainVoice:return {0x2fda,4};case S::RetainData:return {0x2fde,4};case S::RetainNote:return {0x2fe2,4};case S::RetainPatch:return {0x2fe6,4};case S::DispatchRelease:return {0x2fea,4};case S::AwaitRelease:case S::DispatchMonoReset:return {0x2fee,4};
 case S::AwaitMonoReset:case S::ReleasePatch:return {0x2ff2,4};case S::ReleaseNote:return {0x2ff6,4};case S::ReleaseData:return {0x2ffa,4};case S::ReleaseVoice:return {0x2ffe,4};case S::ReleaseUpper:return {0x3002,2};case S::ReleaseIndex:return {0x3004,4};case S::DispatchPhysicalPart:return {0x3008,4};case S::AwaitPhysicalPart:case S::BindPhysical:return {0x300c,2};case S::LocatePhysical:return {0x300e,6};case S::ClearPhysical:return {0x3014,4};case S::ReleasePedal:return {0x3018,2};case S::ReleaseMap:return {0x301a,4};case S::ReleasePart:return {0x301e,4};case S::Finish:return {0x3022,2};
 default:throw std::runtime_error("Invalid native Note Off state stage");
}}
uint8_t partBinding(uint32_t address){if(address<0x160d2||address>=0x160d2 + 256*334||(address-0x160d2)%334)throw std::runtime_error("Invalid Note Off part binding");return uint8_t((address-0x160d2)/334);}
}
void setNoteOffStatePartialForTests(NoteOffStatePartialTest fn){partialTest=fn;}
bool NoteOffStateCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;
 std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 if(pc==0x2f92){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native Note Off state capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;
 if(task.awaitingPart())task.resumePart(partBinding(uint32_t(v[0].er)));if(task.awaitingCompact())task.resumeCompact(partBinding(uint32_t(v[0].er)));
 if(task.awaitingHeld())task.resumeHeld(uint16_t(v[1].r)!=0,partBinding(uint32_t(v[0].er)));if(task.awaitingRelease())task.resumeRelease();if(task.awaitingMonoReset())task.resumeMonoReset();if(task.awaitingPhysicalPart())task.resumePhysicalPart(partBinding(uint32_t(v[0].er)));
 const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const unsigned base=0x160d2 + unsigned(task.part())*334;
 auto timing=[&](unsigned offset){const unsigned address=base+offset;return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=task.nextClocks(task.publication()?timing(task.publicationOffset()):3);
 auto read=[&](T::Read field,unsigned offset)->unsigned{if(field==T::Read::Note)return uint8_t(v[5].rl);return uint8_t(h.read8(base+offset));};
 auto write=[&](T::Write,unsigned offset,unsigned data){h.write8(int8_t(data),base+offset);};
 if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Native Note Off state deadline mismatch");
 switch(stage){
 case S::RetainPart:clocks.push(v[0].er);call.depth+=4;break;case S::RetainMap:clocks.push(v[1].er);call.depth+=4;break;case S::RetainPedal:clocks.push(v[2].r);call.depth+=2;break;
 case S::ClearIndex:v[1].er=h.sub32(v[1].er,v[1].er);break;case S::CaptureNote:v[1].rl=int8_t(task.noteIndex());value(h,v[1].rl);break;
 case S::IndexQuarter:clocks.shift(v[1].rl,2,false);break;case S::IndexEighth:clocks.shift(v[1].rl,1,false);break;case S::RetainIndex:clocks.push(v[1].er);call.depth+=4;break;
 case S::DispatchPart:case S::DispatchPhysicalPart:case S::DispatchCompact:case S::DispatchHeld:case S::DispatchRelease:case S::DispatchMonoReset:{
  const unsigned target=stage==S::DispatchPart||stage==S::DispatchPhysicalPart?0x4000:stage==S::DispatchCompact?0x3024:stage==S::DispatchHeld?0x30fe:stage==S::DispatchRelease?0x9374:0xa8e6;
  clocks.call(target);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native Note Off dispatch clocks differ");return true;}
 case S::ReadPedal:v[2].rl=int8_t(task.pedal());value(h,v[2].rl);break;case S::ObservePedal:flag(h,4,!(task.pedal()&16));break;
 case S::BindHeld:v[1].er=h.add32(v[1].er,int32_t(base));break;case S::LocateHeld:v[1].er=h.add32(v[1].er,0xd0);break;
 case S::ReadMode:v[1].rl=int8_t(task.mode());value(h,v[1].rl);break;case S::ObserveMode:flag(h,4,!(task.mode()&64));break;
 case S::RetainUpper:clocks.push(v[2].e);call.depth+=2;break;case S::RetainVoice:clocks.push(v[3].er);call.depth+=4;break;case S::RetainData:clocks.push(v[4].er);call.depth+=4;break;case S::RetainNote:clocks.push(v[5].er);call.depth+=4;break;case S::RetainPatch:clocks.push(v[6].er);call.depth+=4;break;
 case S::ReleasePatch:clocks.pop(v[6].er);call.depth-=4;break;case S::ReleaseNote:clocks.pop(v[5].er);call.depth-=4;break;case S::ReleaseData:clocks.pop(v[4].er);call.depth-=4;break;case S::ReleaseVoice:clocks.pop(v[3].er);call.depth-=4;break;case S::ReleaseUpper:clocks.pop(v[2].e);call.depth-=2;break;
 case S::ReleaseIndex:clocks.pop(v[1].er);call.depth-=4;break;case S::BindPhysical:v[1].er=h.add32(v[1].er,int32_t(base));break;case S::LocatePhysical:v[1].er=h.add32(v[1].er,0xc0);break;
 case S::ReleasePedal:clocks.pop(v[2].r);call.depth-=2;break;case S::ReleaseMap:clocks.pop(v[1].er);call.depth-=4;break;case S::ReleasePart:clocks.pop(v[0].er);call.depth-=4;break;
 case S::Finish:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native Note Off return clocks differ");selected->reset();++completed;return true;
 default:break;
 }
 if(h.cycles-begin!=cost)throw std::runtime_error("Native Note Off bus clocks differ");h.pc=h.makepc(point(task).location);return true;
}
}
