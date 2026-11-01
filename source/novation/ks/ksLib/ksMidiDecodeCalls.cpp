#include "ksMidiDecodeCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
MidiDecodePartialTest partialTest=nullptr;struct Point{unsigned location,length;};
using T=MidiDecodeTask;using S=T::Stage;using D=MidiDescriptorTask;using DS=D::Stage;
unsigned descriptorAddress(MidiDescriptorKey key){return (key.family==MidiDescriptorKey::Family::Channel?0xf250u:0xf27au)+key.index*6;}
Point point(const D& t){switch(t.stage()){
 case DS::CompareSystem:return {0x2ed4,2};case DS::AdmitSystem:return {0x2ed6,2};case DS::MaskChannel:return {0x2ed8,2};case DS::QuarterChannel:return {0x2eda,2};case DS::SixteenthChannel:return {0x2edc,2};case DS::SetChannelStride:return {0x2ede,2};case DS::ChannelProduct:return {0x2ee0,2};case DS::NormalizeChannel:return {0x2ee2,2};case DS::ResolveChannel:return {0x2ee4,6};case DS::FinishChannel:return {0x2eea,2};case DS::MaskSystem:return {0x2eec,2};case DS::SetSystemStride:return {0x2eee,2};case DS::SystemProduct:return {0x2ef0,2};case DS::NormalizeSystem:return {0x2ef2,2};case DS::ResolveSystem:return {0x2ef4,6};case DS::FinishSystem:return {0x2efa,2};default:throw std::runtime_error("Invalid MIDI descriptor stage");}}
Point point(const T& t){switch(t.stage()){
 case S::ClearPreview:return {0x2de8,2};case S::PublishPreview:return {0x2dea,6};case S::ReadInput:return {0x2df0,2};case S::AdmitStatus:return {0x2df2,2};case S::PublishReceived:return {0x2df4,6};case S::ClearActivity:return {0x2dfa,2};case S::PublishActivity:return {0x2dfc,6};case S::DispatchStatusDescriptor:return {0x2e02,4};case S::AwaitStatusDescriptor:case S::ReadStatusLength:return {0x2e06,8};case S::PublishExpected:return {0x2e0e,6};case S::ReadStatusFlags:return {0x2e14,8};case S::MaskImmediate:return {0x2e1c,2};case S::AdmitImmediate:return {0x2e1e,2};case S::PublishStatusRemaining:return {0x2e20,6};case S::AdmitStatusLength:return {0x2e26,2};case S::PublishRunningStatus:return {0x2e28,6};case S::MaskChannel:return {0x2e2e,2};case S::PublishChannel:return {0x2e30,6};case S::SetDataCursor:return {0x2e36,6};case S::PublishStatusCursor:return {0x2e3c,8};case S::FinishStatus:return {0x2e44,2};case S::ReadImmediateHandler:return {0x2e46,10};case S::TailImmediate:return {0x2e50,2};
 case S::ReadRemaining:return {0x2e52,6};case S::AdmitRemaining:return {0x2e58,2};case S::ReadCursor:return {0x2e5a,8};case S::PublishData:return {0x2e62,2};case S::DecrementRemaining:return {0x2e64,2};case S::PublishDataRemaining:return {0x2e66,6};case S::AdmitMessage:return {0x2e6c,2};case S::AdvanceCursor:return {0x2e6e,2};case S::PublishDataCursor:return {0x2e70,8};case S::FinishData:return {0x2e78,2};
 case S::ReadRunningStatus:return {0x2e7a,6};case S::PublishDispatchStatus:return {0x2e80,6};case S::DispatchMessageDescriptor:return {0x2e86,2};case S::AwaitMessageDescriptor:case S::ReadRunningFlags:return {0x2e88,8};case S::MaskRunning:return {0x2e90,2};case S::AdmitRunning:return {0x2e92,2};case S::ReadRunningLength:return {0x2e94,8};case S::PublishRunningRemaining:return {0x2e9c,6};case S::ResetDataCursor:return {0x2ea2,6};case S::PublishRunningCursor:return {0x2ea8,8};case S::ReadMessageHandler:return {0x2eb0,10};case S::BindPayload:return {0x2eba,6};case S::ReadChannel:return {0x2ec0,6};case S::PublishDispatchChannel:return {0x2ec6,6};case S::DispatchMessage:return {0x2ecc,4};case S::AwaitMessage:case S::ReturnToLoop:return {0x2ed0,4};default:throw std::runtime_error("Invalid native MIDI decode stage");}}
unsigned address(T::Write field,uint32_t cursor){switch(field){case T::Write::Preview:return 0x15c15;case T::Write::ReceivedStatus:return 0x15c1c;case T::Write::Activity:return 0x1603a;case T::Write::Expected:return 0x15c1e;case T::Write::Remaining:return 0x15c1d;case T::Write::RunningStatus:return 0x15c1b;case T::Write::Channel:return 0x15c1a;case T::Write::Cursor:return 0x15c16;case T::Write::Data:return cursor;case T::Write::DispatchStatus:return 0x15c22;case T::Write::DispatchChannel:return 0x15c21;}throw std::runtime_error("Invalid native MIDI publication");}
}
void setMidiDecodePartialForTests(MidiDecodePartialTest fn){partialTest=fn;}
bool MidiDecodeCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;auto partial=[&]{if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;};
 if(pc>=0x2ed4&&pc<0x2efc){
  std::optional<Descriptor>* selected=nullptr;for(auto& c:descriptors)if(c&&c->token==h.getSP()){selected=&c;break;}
  if(pc==0x2ed4){if(!selected)for(auto& c:descriptors)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native MIDI descriptor capacity exceeded");selected->emplace(uint8_t(v[0].rl),h.getSP());++descriptorStarted;}
  if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
  auto& task=(*selected)->task;const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const auto cost=task.nextClocks();
  if(task.advanceBefore(cost)!=cost)throw std::runtime_error("MIDI descriptor deadline mismatch");
  switch(stage){case DS::CompareSystem:h.sub8(v[0].rl,0xf0);break;case DS::MaskChannel:v[0].rl&=0x70;value(h,v[0].rl);break;
  case DS::QuarterChannel:case DS::SixteenthChannel:clocks.shift(v[0].rl,2,false);break;case DS::MaskSystem:v[0].rl&=15;value(h,v[0].rl);break;
  case DS::SetChannelStride:case DS::SetSystemStride:v[0].rh=6;value(h,v[0].rh);break;case DS::ChannelProduct:case DS::SystemProduct:v[0].r=int16_t(task.offset());h.clockI(12);break;
  case DS::NormalizeChannel:case DS::NormalizeSystem:v[0].er&=0xffff;value(h,v[0].er);break;case DS::ResolveChannel:case DS::ResolveSystem:v[0].er=h.add32(v[0].er,stage==DS::ResolveChannel?0xf250:0xf27a);break;
  case DS::FinishChannel:case DS::FinishSystem:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI descriptor return clocks differ");selected->reset();++descriptorCompleted;return true;default:break;}
  if(h.cycles-begin!=cost)throw std::runtime_error("MIDI descriptor boundary clocks differ");h.pc=h.makepc(point(task).location);return true;
 }
 std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0x2de8){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native MIDI decoder capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
 auto& call=**selected;auto& task=call.task;
 if(task.awaitingDescriptor()){if(uint32_t(v[0].er)!=descriptorAddress(task.descriptor()))throw std::runtime_error("Native MIDI descriptor reply differs");task.resumeDescriptor();}if(task.awaitingMessage())task.resumeMessage();
 const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const unsigned desc=descriptorAddress(task.descriptor());
 auto timing=[&](T::Write field){const auto at=address(field,task.cursor())&0xffffff;if(at>=0x400000)return 3u;return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};
 const auto cost=task.nextClocks(task.publication()?timing(task.publicationKind()):3);
 auto read=[&](T::Read field)->uint32_t{switch(field){case T::Read::Input:return uint8_t(v[6].rl);case T::Read::Length:return uint8_t(h.read8(desc+4));case T::Read::Flags:return uint8_t(h.read8(desc+5));case T::Read::Remaining:return uint8_t(h.read8(0x15c1d));case T::Read::Cursor:return uint32_t(h.read32(0x15c16));case T::Read::RunningStatus:return uint8_t(h.read8(0x15c1b));case T::Read::Channel:return uint8_t(h.read8(0x15c1a));case T::Read::Handler:call.handlerReference=uint32_t(h.read32(desc));return 0;}return 0;};
 auto write=[&](T::Write field,uint32_t data){const unsigned at=address(field,task.cursor());if(field==T::Write::Cursor)h.write32(int32_t(data),at);else h.write8(int8_t(data),at);};
 if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Native MIDI decode deadline mismatch");
 switch(stage){
 case S::ClearPreview:v[0].rl=0;value(h,v[0].rl);break;case S::PublishPreview:case S::PublishActivity:value(h,0);break;
 case S::ReadInput:v[0].rl=int8_t(task.input());value(h,v[0].rl);break;case S::PublishReceived:value(h,int8_t(task.received()));break;case S::ClearActivity:v[0].rh=0;value(h,v[0].rh);break;
 case S::DispatchStatusDescriptor:case S::DispatchMessageDescriptor:case S::DispatchMessage:clocks.call(stage==S::DispatchMessage?0x328e:0x2ed4);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native MIDI decode dispatch clocks differ");return true;
 case S::ReadStatusLength:case S::ReadRunningLength:v[1].rl=int8_t(task.length());value(h,v[1].rl);break;case S::PublishExpected:value(h,int8_t(task.length()));break;
 case S::ReadStatusFlags:v[1].rh=int8_t(task.flags());value(h,v[1].rh);break;case S::MaskImmediate:v[1].rh&=2;value(h,v[1].rh);break;
 case S::PublishStatusRemaining:case S::PublishDataRemaining:case S::PublishRunningRemaining:value(h,int8_t(task.remaining()));break;
 case S::PublishRunningStatus:value(h,int8_t(task.status()));break;case S::MaskChannel:v[6].rl=int8_t(task.channel());value(h,v[6].rl);break;case S::PublishChannel:value(h,int8_t(task.channel()));break;
 case S::SetDataCursor:v[0].er=int32_t(task.cursor());value(h,v[0].er);break;case S::PublishStatusCursor:case S::PublishDataCursor:case S::PublishRunningCursor:value(h,int32_t(task.cursor()));break;
 case S::ReadImmediateHandler:case S::ReadMessageHandler:v[0].er=int32_t(call.handlerReference);value(h,v[0].er);break;
 case S::ReadRemaining:v[0].rl=int8_t(task.remaining());value(h,v[0].rl);break;case S::ReadCursor:v[1].er=int32_t(task.cursor());value(h,v[1].er);break;case S::PublishData:value(h,int8_t(task.data()));break;
 case S::DecrementRemaining:clocks.inc(v[0].rl,-1);break;case S::AdvanceCursor:clocks.inc(v[1].er,1);break;
 case S::ReadRunningStatus:v[0].rl=int8_t(task.status());value(h,v[0].rl);break;case S::PublishDispatchStatus:value(h,int8_t(task.status()));break;
 case S::ReadRunningFlags:v[1].rl=int8_t(task.flags());value(h,v[1].rl);break;case S::MaskRunning:v[1].rl&=1;value(h,v[1].rl);break;case S::ResetDataCursor:v[1].er=int32_t(task.cursor());value(h,v[1].er);break;
 case S::BindPayload:v[4].er=0x15c1f;value(h,v[4].er);break;case S::ReadChannel:v[1].rl=int8_t(task.channel());value(h,v[1].rl);break;case S::PublishDispatchChannel:value(h,int8_t(task.channel()));break;
 case S::FinishStatus:case S::FinishData:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native MIDI decode return clocks differ");selected->reset();++completed;return true;
 case S::ReturnToLoop:h.clockI(2);break;default:break;
 }
 if(h.cycles-begin!=cost)throw std::runtime_error("Native MIDI decode bus clocks differ");if(task.handoff()){h.pc=h.makepc(task.stage()==S::HandoffImmediate?(call.handlerReference&0xffffff):0x588);selected->reset();++handedOff;}else h.pc=h.makepc(point(task).location);return true;
}
}
