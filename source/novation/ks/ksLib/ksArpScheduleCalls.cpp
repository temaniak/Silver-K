#include "ksArpScheduleCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace {ArpSchedulePartialTest partialTest=nullptr;using T=ArpScheduleTask;using S=T::Stage;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){
case S::ReadMask:return {0x370e,6};
case S::PublishMask:return {0x3714,6};
case S::ClearResult:return {0x371a,2};
case S::ClearIndex:return {0x371c,2};
case S::SinglePart:return {0x371e,2};
case S::ReadMode:return {0x3720,6};
case S::AdmitMode:return {0x3726,2};
case S::FirstMultiPart:return {0x3728,2};
case S::MultiParts:return {0x372a,2};
case S::PublishPart:return {0x372c,6};
case S::PublishWork:return {0x3732,6};
case S::RetainParts:return {0x3738,2};
case S::RetainResult:return {0x373a,2};
case S::DispatchPart:return {0x373c,2};
case S::AwaitPart:return {0x373e,2};
case S::ReleaseResult:return {0x373e,2};
case S::ReleaseParts:return {0x3740,2};
case S::ReadWork:return {0x3742,6};
case S::AdmitWork:return {0x3748,2};
case S::CaptureIndex:return {0x374a,2};
case S::ReadCurrentMode:return {0x374c,6};
case S::LocateResult:return {0x3752,2};
case S::MarkResult:return {0x3754,2};
case S::ReadCurrentMask:return {0x3756,6};
case S::ShiftMask:return {0x375c,2};
case S::PublishShiftedMask:return {0x375e,6};
case S::TickPart:return {0x3764,2};
case S::TickParts:return {0x3766,2};
case S::AdmitParts:return {0x3768,2};
case S::DispatchDrain:return {0x376a,4};
case S::AwaitDrain:return {0x376e,2};
case S::Finish:return {0x376e,2};
default:throw std::runtime_error("Invalid arp schedule stage");}}unsigned address(T::Write w){return w==T::Write::Mask?0x160d0:w==T::Write::Part?0x17526:0x160d1;}}
void setArpSchedulePartialForTests(ArpSchedulePartialTest fn){partialTest=fn;}
bool ArpScheduleCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}if(pc==0x370e){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Arp schedule capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& t=call.task;if(t.awaitingChild())t.resumeChild();const auto stage=t.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(t).length);auto timing=[&](T::Write w){const auto at=address(w);return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=t.nextClocks(t.publication()?timing(t.publicationKind()):3);
 auto read=[&](T::Read r)->unsigned{return uint8_t(h.read8(r==T::Read::Mask?0x167d3:r==T::Read::Mode||r==T::Read::CurrentMode?0x167ba:r==T::Read::Work?0x160d1:0x160d0));};auto write=[&](T::Write w,unsigned data){h.write8(int8_t(data),address(w));};if(t.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Arp schedule deadline mismatch");
 switch(stage){case S::ReadMask:v[0].rl=int8_t(t.mask());value(h,v[0].rl);break;case S::PublishMask:value(h,int8_t(t.mask()));break;case S::ClearResult:v[2].r=h.sub16(v[2].r,v[2].r);break;case S::ClearIndex:v[0].rl=0;value(h,v[0].rl);break;case S::SinglePart:v[0].rh=1;value(h,v[0].rh);break;case S::ReadMode:v[1].rl=int8_t(t.mode());value(h,v[1].rl);break;case S::FirstMultiPart:v[0].rl=1;value(h,v[0].rl);break;case S::MultiParts:v[0].rh=4;value(h,v[0].rh);break;case S::PublishPart:value(h,int8_t(t.index()));break;case S::PublishWork:value(h,int8_t(0));break;
 case S::RetainParts:clocks.push(v[0].r);call.depth+=2;break;case S::RetainResult:clocks.push(v[2].r);call.depth+=2;break;case S::ReleaseResult:clocks.pop(v[2].r);call.depth-=2;break;case S::ReleaseParts:clocks.pop(v[0].r);call.depth-=2;break;
 case S::ReadWork:v[1].rl=int8_t(t.work());value(h,v[1].rl);break;case S::CaptureIndex:v[1].rl=int8_t(t.index());value(h,v[1].rl);break;case S::ReadCurrentMode:v[1].rh=int8_t(t.currentMode());value(h,v[1].rh);break;case S::LocateResult:v[1].rl=h.sub8(v[1].rl,v[1].rh);break;case S::MarkResult:v[2].rh=int8_t(t.result());break;case S::ReadCurrentMask:v[1].rl=int8_t(t.mask());value(h,v[1].rl);break;case S::ShiftMask:clocks.shift(v[1].rl,1,false);break;case S::PublishShiftedMask:value(h,int8_t(t.mask()));break;case S::TickPart:clocks.inc(v[0].rl,1);break;case S::TickParts:clocks.inc(v[0].rh,-1);break;
 case S::DispatchPart:case S::DispatchDrain:clocks.call(stage==S::DispatchPart?0x3770:0x588);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp schedule dispatch timing");return true;case S::Finish:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp schedule return timing");selected->reset();++completed;return true;default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("Arp schedule bus timing");h.pc=h.makepc(point(t).location);return true;
}}
