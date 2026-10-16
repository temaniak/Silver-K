#include "ksPedalCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {PedalPartialTest partialTest=nullptr;using T=PedalTask;using S=T::Stage;
struct Point{unsigned location,length;};
Point point(const T& t){switch(t.stage()){
 case S::CompareController:return {0x352c,2};case S::AdmitController:return {0x352e,2};case S::ClearDesired:return {0x3530,2};case S::CompareValue:return {0x3532,2};case S::AdmitValue:return {0x3534,2};case S::Hold:return {0x3536,2};case S::DispatchPart:return {0x3538,4};case S::AwaitPart:case S::ReadPrevious:return {0x353c,8};case S::PublishCurrent:return {0x3544,8};case S::PublishMirror:return {0x354c,8};case S::RetainPart:return {0x3554,4};case S::CapturePatchPart:return {0x3558,2};case S::DispatchControls:return {0x355a,4};case S::AwaitControls:case S::ReadPatch:return {0x355e,10};case S::RestorePart:return {0x3568,4};case S::ReadFlags:return {0x356c,8};case S::ObserveBypass:return {0x3574,2};case S::AdmitBypass:return {0x3576,2};case S::Changes:return {0x3578,2};case S::AdmitChanges:return {0x357a,2};case S::ReleasedBits:return {0x357c,2};case S::ObserveRelease:return {0x357e,2};case S::AdmitRelease:return {0x3580,2};case S::DispatchRelease:return {0x3582,4};case S::AwaitRelease:case S::Finish:return {0x3586,2};case S::FinishBypass:return {0x3588,2};case S::Handoff:return {0x358a,0};default:throw std::runtime_error("Invalid native sustain stage");}}
uint8_t bind(uint32_t a,unsigned base,unsigned stride){if(a<base||a>=base+256*stride||(a-base)%stride)throw std::runtime_error("Invalid native sustain part binding");return uint8_t((a-base)/stride);}
}
void setPedalPartialForTests(PedalPartialTest fn){partialTest=fn;}
bool PedalCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;
 std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 if(pc==0x352c||(pc==0x3530&&(!selected||point((*selected)->task).location!=pc))){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native sustain capacity exceeded");selected->emplace(pc==0x352c,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;if(task.awaitingPart())task.resumePart(bind(uint32_t(v[0].er),0x160d2,334));if(task.awaitingControls())task.resumeControls(bind(uint32_t(v[0].er),0x174ae,20));if(task.awaitingRelease())task.resumeRelease();
 const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const unsigned base=0x160d2 + unsigned(task.part())*334,controls=0x174ae + unsigned(task.controlPart())*20;
 auto timing=[&](unsigned offset){const unsigned address=base+offset;return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=task.nextClocks(task.publication()?timing(task.publicationOffset()):3);
 auto read=[&](T::Read field)->uint32_t{switch(field){case T::Read::Controller:return uint8_t(v[0].rh);case T::Read::Value:return uint8_t(v[0].rl);case T::Read::Previous:return uint8_t(h.read8(base+309));case T::Read::PatchPart:return uint8_t(v[2].rl);case T::Read::PatchReference:return uint32_t(h.read32(controls));default:return uint8_t(h.read8(task.patchReference()+88));}};
 auto write=[&](unsigned offset,unsigned data){h.write8(int8_t(data),base+offset);};
 if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Native sustain deadline mismatch");
 switch(stage){case S::CompareController:h.sub8(v[0].rh,64);break;case S::ClearDesired:v[3].rh=0;value(h,v[3].rh);break;case S::CompareValue:h.sub8(v[0].rl,64);break;
 case S::Hold:v[3].rh|=16;break;case S::DispatchPart:case S::DispatchControls:case S::DispatchRelease:clocks.call(stage==S::DispatchPart?0x4000:stage==S::DispatchControls?0xa670:0x9400);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native sustain dispatch clocks differ");return true;
 case S::ReadPrevious:v[3].rl=int8_t(task.previous());value(h,v[3].rl);break;case S::PublishCurrent:case S::PublishMirror:value(h,int8_t(task.desired()));break;
 case S::RetainPart:clocks.push(v[0].er);call.depth+=4;break;case S::CapturePatchPart:v[0].rl=int8_t(task.patchPart());value(h,v[0].rl);break;
 case S::ReadPatch:v[1].er=int32_t(task.patchReference());value(h,v[1].er);break;case S::RestorePart:clocks.pop(v[0].er);call.depth-=4;break;
 case S::ReadFlags:v[2].rl=int8_t(task.flags());value(h,v[2].rl);break;case S::ObserveBypass:flag(h,4,!(task.flags()&4));break;
 case S::Changes:v[3].rh=int8_t(task.changed());value(h,v[3].rh);break;case S::ReleasedBits:v[3].rl=int8_t(task.released());value(h,v[3].rl);break;case S::ObserveRelease:flag(h,4,!(task.released()&16));break;
 case S::Finish:case S::FinishBypass:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native sustain return clocks differ");selected->reset();++completed;return true;default:break;
 }
 if(h.cycles-begin!=cost)throw std::runtime_error("Native sustain bus clocks differ");if(task.handoff()){h.pc=h.makepc(0x358a);selected->reset();++handedOff;}else h.pc=h.makepc(point(task).location);return true;
}
}
