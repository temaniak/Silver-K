#include "ksInternalMidiCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace {InternalMidiPartialTest partialTest=nullptr;using T=InternalMidiTask;using S=T::Stage;using C=T::Child;using D=MidiDescriptorTask;using DS=D::Stage;struct Point{unsigned location,length;};unsigned descriptorAddress(MidiDescriptorKey k){return (k.family==MidiDescriptorKey::Family::Channel?0xf2dau:0xf27au)+k.index*6;}
Point point(const D& t){switch(t.stage()){
 case DS::CompareSystem:return {0xa85a,2};case DS::AdmitSystem:return {0xa85c,2};case DS::MaskChannel:return {0xa85e,2};case DS::QuarterChannel:return {0xa860,2};case DS::SixteenthChannel:return {0xa862,2};case DS::SetChannelStride:return {0xa864,2};case DS::ChannelProduct:return {0xa866,2};case DS::NormalizeChannel:return {0xa868,2};case DS::ResolveChannel:return {0xa86a,6};case DS::FinishChannel:return {0xa870,2};case DS::MaskSystem:return {0xa872,2};case DS::SetSystemStride:return {0xa874,2};case DS::SystemProduct:return {0xa876,2};case DS::NormalizeSystem:return {0xa878,2};case DS::ResolveSystem:return {0xa87a,6};case DS::FinishSystem:return {0xa880,2};default:throw std::runtime_error("Invalid MIDI descriptor stage");}}
Point point(const T& t){switch(t.stage()){case S::CaptureInput:return {0xa762,2};case S::AdmitStatus:return {0xa764,2};case S::StatusDescriptor:return {0xa766,4};case S::ReadStatusLength:return {0xa76a,8};case S::ReadStatusFlags:return {0xa772,8};case S::MaskImmediate:return {0xa77a,2};case S::AdmitImmediate:return {0xa77c,2};case S::PublishStatusRemaining:return {0xa77e,6};case S::AdmitStatusLength:return {0xa784,2};case S::PublishRunningStatus:return {0xa786,6};case S::MaskChannel:return {0xa78c,2};case S::PublishChannel:return {0xa78e,6};case S::SetDataCursor:return {0xa794,6};case S::PublishStatusCursor:return {0xa79a,8};case S::FinishStatus:return {0xa7a2,2};case S::ReadImmediateHandler:return {0xa7a4,10};case S::SkipToGuard:return {0xa7ae,4};case S::ReadRemaining:return {0xa7b2,6};case S::AdmitRemaining:return {0xa7b8,2};case S::ReadCursor:return {0xa7ba,8};case S::PublishData:return {0xa7c2,2};case S::TickRemaining:return {0xa7c4,2};case S::PublishDataRemaining:return {0xa7c6,6};case S::AdmitMessage:return {0xa7cc,2};case S::TickCursor:return {0xa7ce,2};case S::PublishDataCursor:return {0xa7d0,8};case S::FinishData:return {0xa7d8,2};case S::ReadRunningStatus:return {0xa7da,6};case S::PublishDispatchStatus:return {0xa7e0,6};case S::MessageDescriptor:return {0xa7e6,2};case S::ReadRunningFlags:return {0xa7e8,8};case S::MaskRunning:return {0xa7f0,2};case S::AdmitRunning:return {0xa7f2,2};case S::ReadRunningLength:return {0xa7f4,8};case S::PublishRunningRemaining:return {0xa7fc,6};case S::ResetCursor:return {0xa802,6};case S::PublishRunningCursor:return {0xa808,8};case S::ReadMessageHandler:return {0xa810,10};case S::BindPayload:return {0xa81a,6};case S::ReadChannel:return {0xa820,6};case S::PublishDispatchChannel:return {0xa826,6};case S::GuardRetainHandler:return {0xa82c,4};case S::GuardRetainPayload:return {0xa830,4};case S::GuardScanHeld:return {0xa834,4};case S::GuardAdmitHeld:return {0xa838,2};case S::GuardReadPolicy:return {0xa83a,8};case S::GuardObserveEnabled:return {0xa842,2};case S::GuardAdmitEnabled:return {0xa844,2};case S::GuardReleasePayload:return {0xa846,4};case S::GuardReleaseHandler:return {0xa84a,4};case S::GuardDispatch:return {0xa84e,2};case S::GuardReleaseIgnoredPayload:return {0xa850,4};case S::GuardReleaseIgnoredHandler:return {0xa854,4};case S::FinishIgnored:return {0xa858,2};default:throw std::runtime_error("Invalid internal MIDI stage");}}uint8_t binding(uint32_t a){if(a<0x160d2||a>=0x160d2 + 256*334||(a-0x160d2)%334)throw std::runtime_error("Invalid internal MIDI part");return uint8_t((a-0x160d2)/334);}}
void setInternalMidiPartialForTests(InternalMidiPartialTest fn){partialTest=fn;}
bool InternalMidiCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;auto partial=[&]{if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;};
 if(pc>=0xa85a&&pc<=0xa880){
  std::optional<Descriptor>* selected=nullptr;for(auto& c:descriptors)if(c&&c->token==h.getSP()){selected=&c;break;}
  if(pc==0xa85a){if(!selected)for(auto& c:descriptors)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native MIDI descriptor capacity exceeded");selected->emplace(uint8_t(v[0].rl),h.getSP());++descriptorStarted;}
  if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
  auto& task=(*selected)->task;const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const auto cost=task.nextClocks();
  if(task.advanceBefore(cost)!=cost)throw std::runtime_error("MIDI descriptor deadline mismatch");
  switch(stage){case DS::CompareSystem:h.sub8(v[0].rl,0xf0);break;case DS::MaskChannel:v[0].rl&=0x70;value(h,v[0].rl);break;
  case DS::QuarterChannel:case DS::SixteenthChannel:clocks.shift(v[0].rl,2,false);break;case DS::MaskSystem:v[0].rl&=15;value(h,v[0].rl);break;
  case DS::SetChannelStride:case DS::SetSystemStride:v[0].rh=6;value(h,v[0].rh);break;case DS::ChannelProduct:case DS::SystemProduct:v[0].r=int16_t(task.offset());h.clockI(12);break;
  case DS::NormalizeChannel:case DS::NormalizeSystem:v[0].er&=0xffff;value(h,v[0].er);break;case DS::ResolveChannel:case DS::ResolveSystem:v[0].er=h.add32(v[0].er,stage==DS::ResolveChannel?0xf2da:0xf27a);break;
  case DS::FinishChannel:case DS::FinishSystem:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI descriptor return clocks differ");selected->reset();++descriptorCompleted;return true;default:break;}
  if(h.cycles-begin!=cost)throw std::runtime_error("MIDI descriptor boundary clocks differ");h.pc=h.makepc(point(task).location);return true;
 }

 std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}if(pc==0xa762){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Internal MIDI capacity exceeded");selected->emplace(uint8_t(v[6].rl),uint32_t(v[4].er),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}auto& call=**selected;auto& t=call.task;
 if(t.awaitingChild()){if(t.child()==C::Descriptor){if(uint32_t(v[0].er)!=descriptorAddress(t.descriptor()))throw std::runtime_error("Internal descriptor reply differs");t.resumeDescriptor();}else t.resumeHeld(binding(uint32_t(v[0].er)),v[1].r!=0);}
 const auto stage=t.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(t).length);const auto desc=descriptorAddress(t.descriptor());uint32_t readData=0;
 auto publicationAddress=[&](S stage)->uint32_t{switch(stage){case S::PublishStatusRemaining:return 0x15c2c;case S::PublishRunningStatus:return 0x15c2b;case S::PublishChannel:return 0x15c2a;case S::PublishData:return t.cursor();case S::PublishDataRemaining:return 0x15c2c;case S::PublishDispatchStatus:return 0x15c22;case S::PublishRunningRemaining:return 0x15c2c;case S::PublishDispatchChannel:return 0x15c21;default:throw std::runtime_error("Invalid internal MIDI publication");}};auto timing=[&](S stage){const auto at=publicationAddress(stage)&0xffffff;if(at>=0x400000)return 3u;return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=t.nextClocks(t.publication()?timing(stage):3);
 auto read=[&](S stage)->uint32_t{switch(stage){case S::ReadStatusLength:return uint8_t(h.read8(desc+4));case S::ReadStatusFlags:return uint8_t(h.read8(desc+5));case S::ReadImmediateHandler:return uint32_t(h.read32(desc));case S::ReadRemaining:return uint8_t(h.read8(0x15c2c));case S::ReadCursor:return uint32_t(h.read32(0x15c24));case S::ReadRunningStatus:return uint8_t(h.read8(0x15c2b));case S::ReadRunningFlags:return uint8_t(h.read8(desc+5));case S::ReadRunningLength:return uint8_t(h.read8(desc+4));case S::ReadMessageHandler:return uint32_t(h.read32(desc));case S::ReadChannel:return uint8_t(h.read8(0x15c2a));case S::GuardReadPolicy:return uint8_t(h.read8(0x160d2 + unsigned(t.part())*334+327));default:throw std::runtime_error("Invalid internal MIDI read");}};auto capture=[&](S stage){readData=read(stage);if(stage==S::ReadImmediateHandler||stage==S::ReadMessageHandler)call.handlerReference=readData;return readData;};auto write=[&](S stage,uint32_t data){switch(stage){case S::PublishStatusRemaining:h.write8(int8_t(data),0x15c2c);break;case S::PublishRunningStatus:h.write8(int8_t(data),0x15c2b);break;case S::PublishChannel:h.write8(int8_t(data),0x15c2a);break;case S::PublishStatusCursor:h.write32(int32_t(data),0x15c24);break;case S::PublishData:h.write8(int8_t(data),t.cursor());break;case S::PublishDataRemaining:h.write8(int8_t(data),0x15c2c);break;case S::PublishDataCursor:h.write32(int32_t(data),0x15c24);break;case S::PublishDispatchStatus:h.write8(int8_t(data),0x15c22);break;case S::PublishRunningRemaining:h.write8(int8_t(data),0x15c2c);break;case S::PublishRunningCursor:h.write32(int32_t(data),0x15c24);break;case S::PublishDispatchChannel:h.write8(int8_t(data),0x15c21);break;default:throw std::runtime_error("Invalid internal MIDI write");}};if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("Internal MIDI deadline mismatch");switch(stage){
case S::CaptureInput:{clocks.move(v[0].rl,v[6].rl);break;}
case S::AdmitStatus:{;break;}
case S::StatusDescriptor:{clocks.call(0xa85a);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal MIDI dispatch timing");return true;break;}
case S::ReadStatusLength:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ReadStatusFlags:{clocks.move(v[1].rh,int8_t(readData));break;}
case S::MaskImmediate:{v[1].rh&=0x2;value(h,v[1].rh);break;}
case S::AdmitImmediate:{;break;}
case S::PublishStatusRemaining:{value(h,v[1].rl);break;}
case S::AdmitStatusLength:{;break;}
case S::PublishRunningStatus:{value(h,v[6].rl);break;}
case S::MaskChannel:{v[6].rl&=0xf;value(h,v[6].rl);break;}
case S::PublishChannel:{value(h,v[6].rl);break;}
case S::SetDataCursor:{clocks.move(v[0].er,0x15c28);break;}
case S::PublishStatusCursor:{value(h,v[0].er);break;}
case S::FinishStatus:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal MIDI return timing");selected->reset();++completed;return true;break;}
case S::ReadImmediateHandler:{clocks.move(v[0].er,int32_t(readData));break;}
case S::SkipToGuard:{h.clockI(2);break;}
case S::ReadRemaining:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::AdmitRemaining:{;break;}
case S::ReadCursor:{clocks.move(v[1].er,int32_t(readData));break;}
case S::PublishData:{value(h,v[6].rl);break;}
case S::TickRemaining:{clocks.inc(v[0].rl,-1);break;}
case S::PublishDataRemaining:{value(h,v[0].rl);break;}
case S::AdmitMessage:{;break;}
case S::TickCursor:{clocks.inc(v[1].er,1);break;}
case S::PublishDataCursor:{value(h,v[1].er);break;}
case S::FinishData:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal MIDI return timing");selected->reset();++completed;return true;break;}
case S::ReadRunningStatus:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::PublishDispatchStatus:{value(h,v[0].rl);break;}
case S::MessageDescriptor:{clocks.call(0xa85a);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal MIDI dispatch timing");return true;break;}
case S::ReadRunningFlags:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::MaskRunning:{v[1].rl&=0x1;value(h,v[1].rl);break;}
case S::AdmitRunning:{;break;}
case S::ReadRunningLength:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::PublishRunningRemaining:{value(h,v[1].rl);break;}
case S::ResetCursor:{clocks.move(v[1].er,0x15c28);break;}
case S::PublishRunningCursor:{value(h,v[1].er);break;}
case S::ReadMessageHandler:{clocks.move(v[0].er,int32_t(readData));break;}
case S::BindPayload:{clocks.move(v[4].er,0x15c28);break;}
case S::ReadChannel:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::PublishDispatchChannel:{value(h,v[1].rl);break;}
case S::GuardRetainHandler:{clocks.push(v[0].er);call.depth+=4;break;}
case S::GuardRetainPayload:{clocks.push(v[4].er);call.depth+=4;break;}
case S::GuardScanHeld:{clocks.call(0x30fe);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal MIDI dispatch timing");return true;break;}
case S::GuardAdmitHeld:{;break;}
case S::GuardReadPolicy:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::GuardObserveEnabled:{flag(h,4,(uint8_t(v[0].rl)&(1u<<0x2))==0);break;}
case S::GuardAdmitEnabled:{;break;}
case S::GuardReleasePayload:{clocks.pop(v[4].er);call.depth-=4;break;}
case S::GuardReleaseHandler:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::GuardDispatch:{;break;}
case S::GuardReleaseIgnoredPayload:{clocks.pop(v[4].er);call.depth-=4;break;}
case S::GuardReleaseIgnoredHandler:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::FinishIgnored:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal MIDI return timing");selected->reset();++completed;return true;break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("Internal MIDI bus timing");if(t.handoff()){h.pc=h.makepc(call.handlerReference&0xffffff);selected->reset();++handedOff;}else h.pc=h.makepc(point(t).location);return true;}}
