#include "ksUnisonCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
UnisonPartialTest partialTest=nullptr;using S=UnisonTask::Stage;using W=UnisonTask::Write;
struct Point {unsigned location,length;};
Point point(const UnisonTask& task){switch(task.stage()){
 case S::PublishNote:return {0x8fcc,6};case S::PublishVelocity:return {0x8fd2,6};case S::DispatchPart:return {0x8fd8,4};
 case S::AwaitPart:case S::ReadPatch:return {0x8fdc,10};case S::ReadMode:return {0x8fe6,8};case S::MaskCount:return {0x8fee,2};case S::AdmitCount:return {0x8ff0,2};
 case S::DispatchPartAgain:return {0x8ff2,4};case S::AwaitPartAgain:case S::ReadUnison:return {0x8ff6,8};case S::KeepUpper:return {0x8ffe,2};
 case S::MergeCount:return {0x9000,2};case S::WriteUnison:return {0x9002,8};case S::RetainPart:return {0x900a,4};case S::DispatchSingle:return {0x900e,2};
 case S::AwaitSingle:case S::ReleasePart:return {0x9010,4};case S::ReadRemaining:return {0x9014,8};case S::Decrement:return {0x901c,2};
 case S::WriteRemaining:return {0x901e,8};case S::MaskRemaining:return {0x9026,2};case S::AdmitMore:return {0x9028,2};case S::Complete:return {0x902a,4};
 }throw std::runtime_error("Invalid native unison stage");}
unsigned outputAddress(const UnisonTask& task,W what){switch(what){case W::Note:return 0x17528;case W::Velocity:return 0x17529;case W::Unison:return 0x174ae + unsigned(task.part())*20+18;}return 0;}
}
void setUnisonPartialForTests(UnisonPartialTest fn){partialTest=fn;}
bool UnisonCalls::step(ControllerCpu& h){using namespace controllerDetail;using C=UnisonTask::Child;
 const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 if(pc==0x8fcc){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native unison capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& v=h.regs;const auto child=task.child();if(child!=C::None){uint8_t part=0;
  if(child!=C::Single){const auto pointer=uint32_t(v[0].er);if(pointer<0x174ae||pointer>=0x174ae + 20*256||(pointer-0x174ae)%20)throw std::runtime_error("Invalid unison part binding");part=uint8_t((pointer-0x174ae)/20);}task.resumeChild(part);}
 const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task).length);
 auto timing=[&](W what){const auto address=outputAddress(task,what);return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};
 const auto cost=task.nextClocks(task.publication()?timing(task.publicationKind()):3u);
 auto read=[&](UnisonTask::Read what)->uint32_t{using R=UnisonTask::Read;switch(what){case R::Note:return uint8_t(v[5].rl);case R::Velocity:return uint8_t(v[5].rh);
  case R::PatchReference:call.patch=uint32_t(h.read32(0x174ae + unsigned(task.part())*20));return 0;case R::Mode:return uint8_t(h.read8(int32_t(call.patch)));
  case R::Unison:return uint8_t(h.read8(0x174ae + unsigned(task.part())*20+18));}return 0;};
 auto write=[&](W what,uint32_t data){h.write8(int8_t(data),outputAddress(task,what));};
 if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Native unison admission mismatch");
 auto dispatch=[&](unsigned target){clocks.call(target);h.pc=h.makepc(clocks.next);if(h.cycles-before!=cost)throw std::runtime_error("Native unison dispatch clock mismatch");};
 switch(stage){case S::PublishNote:value(h,int8_t(task.note()));break;case S::PublishVelocity:value(h,int8_t(task.velocity()));break;
 case S::DispatchPart:case S::DispatchPartAgain:dispatch(0xa66a);return true;
 case S::ReadPatch:v[6].er=int32_t(call.patch);value(h,v[6].er);break;case S::ReadMode:v[1].rl=int8_t(task.mode());value(h,v[1].rl);break;
 case S::MaskCount:v[1].rl=int8_t(task.count());value(h,v[1].rl);break;case S::ReadUnison:case S::KeepUpper:case S::MergeCount:v[1].rh=int8_t(task.unison());value(h,v[1].rh);break;
 case S::WriteUnison:value(h,int8_t(task.unison()));break;case S::RetainPart:clocks.push(v[0].er);call.depth+=4;break;
 case S::DispatchSingle:dispatch(0x902a);return true;case S::ReleasePart:clocks.pop(v[0].er);call.depth-=4;break;
 case S::ReadRemaining:v[1].rl=int8_t(task.remaining());value(h,v[1].rl);break;case S::Decrement:clocks.inc(v[1].rl,-1);break;
 case S::WriteRemaining:value(h,int8_t(task.remaining()));break;case S::MaskRemaining:v[1].rl=int8_t(task.remaining());value(h,v[1].rl);break;default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native unison bus clocks differ");h.pc=h.makepc(point(task).location);
 if(task.complete()){selected->reset();++completed;}return true;
}
}
