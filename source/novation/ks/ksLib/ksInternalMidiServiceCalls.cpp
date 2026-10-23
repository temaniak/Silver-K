#include "ksInternalMidiServiceCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
InternalMidiServicePartialTest partialTest=nullptr;using T=InternalMidiServiceTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,81> points{{{0x2a18,4},{0x2a1c,4},{0x2a20,4},{0x2a24,4},{0x2a28,2},{0x2a2a,4},{0x2a2e,2},{0x2a30,2},{0x2a32,4},{0x2a36,2},{0x2a38,2},{0x2a3a,2},{0x2a3c,4},{0x2a40,4},{0x2a44,2},{0x2a46,2},{0x2a48,4},{0x2a4c,2},{0x2a4e,2},{0x2a50,2},{0x2a52,4},{0x2a56,2},{0x2ccc,2},{0x2cce,6},{0x2cd4,2},{0x2cd6,2},{0x2cd8,6},{0x2cde,4},{0x2ce2,8},{0x2cea,6},{0x2cf0,8},{0x2cf8,2},{0x2cfa,4},{0x2cfe,6},{0x2d04,4},{0x2d08,6},{0x2d0e,2},{0x2d10,6},{0x2d16,6},{0x2d1c,8},{0x2d24,2},{0x2d26,6},{0x2d2c,2},{0x2d2e,8},{0x2d36,2},{0x2d38,2},{0x2d3a,6},{0x2d40,2},{0x2d42,2},{0x2d44,8},{0x2d4c,2},{0x2d4e,6},{0x2d54,6},{0x2d5a,4},{0x2d5e,8},{0x2d66,2},{0x2d68,2},{0x2d6a,8},{0x2d72,6},{0x2d78,6},{0x2d7e,8},{0x2d86,10},{0x2d90,6},{0x2d96,6},{0x2d9c,6},{0x2da2,4},{0x2da6,2},{0x2da8,6},{0x2dae,6},{0x2db4,6},{0x2dba,6},{0x2dc0,6},{0x2dc6,6},{0x2dcc,6},{0x2dd2,6},{0x2dd8,6},{0x2dde,4},{0x2de2,4},{0x2de6,2},{0x2e46,10},{0x2e50,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned descriptorAddress(MidiDescriptorKey key){return (key.family==MidiDescriptorKey::Family::Channel?0xf250u:0xf27au)+key.index*6;}
unsigned address(T::Write field,const T& task){switch(field){case T::Write::Preview:return 0x15c15;case T::Write::Received:return 0x15c10;case T::Write::Expected:return 0x15c12;case T::Write::Remaining:return 0x15c11;case T::Write::RunningStatus:return 0x15c0f;case T::Write::Channel:return 0x15c0e;case T::Write::Cursor:return 0x15c0a;case T::Write::Data:return task.cursor();case T::Write::DispatchStatus:return 0x15c22;case T::Write::DispatchChannel:return 0x15c21;case T::Write::ResetReceiveRemaining:return 0x15c1d;case T::Write::ResetReceiveExpected:return 0x15c1e;case T::Write::ResetInternalRemaining:return 0x15c11;case T::Write::ResetInternalExpected:return 0x15c12;case T::Write::ResetExclusive:return 0x15c2c;case T::Write::ResetTransfer:return 0x1603a;case T::Write::ResetActivity:return 0x15c23;case T::Write::ResetFirstWord:return 0x15c2e;case T::Write::ResetSecondWord:return 0x15c30;}return 0;}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;return at>=0xffffa0?1:3;}
unsigned longClocks(unsigned at,int previous){unsigned cost=0;for(unsigned i=0;i<4;++i){const unsigned p=(at+i)&0xffffff;cost+=byteClocks(p,previous);previous=p<0x400000||(p>=0xfffd10&&p<0xffffa0)?int(p):-1;}return cost;}
}
void setInternalMidiServicePartialForTests(InternalMidiServicePartialTest value){partialTest=value;}
bool InternalMidiServiceCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x2a18:kind=T::Kind::Initialize;break;case 0x2a2a:kind=T::Kind::Pump;break;case 0x2ccc:kind=T::Kind::Byte;break;case 0x2da6:kind=T::Kind::Reset;break; default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native internal MIDI service capacity exceeded");slot->emplace(*kind,h.getSP());++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(pc>=0x2e46)return false;if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint16_t(r[6].r));const auto stage=task.stage();
 const unsigned target=task.publication()?address(task.publicationKind(),task):0;
 const unsigned store=stage==S::WriteDataCursor?longClocks(target,h.lastwrite):byteClocks(target,h.lastwrite);
 const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);const unsigned desc=descriptorAddress(task.descriptor());
 auto read=[&](T::Read field)->uint32_t{switch(field){case T::Read::Input:return uint8_t(r[6].rl);case T::Read::Length:return uint8_t(h.read8(desc+4));case T::Read::Flags:return uint8_t(h.read8(desc+5));case T::Read::Remaining:return uint8_t(h.read8(0x15c11));case T::Read::Cursor:return uint32_t(h.read32(0x15c0a));case T::Read::RunningStatus:return uint8_t(h.read8(0x15c0f));case T::Read::Handler:return uint32_t(h.read32(desc));case T::Read::Channel:return uint8_t(h.read8(0x15c0e));}return 0;};
 auto write=[&](T::Write field,uint32_t value){const unsigned at=address(field,task);if(field==T::Write::Cursor)f.store32(at,int32_t(value));else if(field==T::Write::ResetFirstWord||field==T::Write::ResetSecondWord)f.store16(at,int16_t(value));else f.store8(at,int8_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native internal MIDI service admission differs");
 switch(stage){case S::ResetReceive:f.call(0xfe4);break;
case S::ResetTransmit:f.call(0x10e4);break;
case S::ResetQueue:f.call(0x2c30);break;
case S::ResetParser:f.call(0x2da6);break;
case S::ReturnInit:f.ret();break;
case S::PopReceive:f.call(0x100a);break;
case S::CompareReceiveByte:h.sub8(r[6].rh,0x1);break;
case S::ParseReceive:f.call(0x2de8);break;
case S::CompareReceiveEmpty:h.sub8(r[6].rh,0x0);break;
case S::ClearReceive:f.call(0xfe4);break;
case S::PopQueue:f.call(0x2c4a);break;
case S::CompareQueueByte:h.sub8(r[6].rh,0x1);break;
case S::ParseQueue:f.call(0x2ccc);break;
case S::CompareQueueEmpty:h.sub8(r[6].rh,0x0);break;
case S::ClearQueue:f.call(0x2c30);break;
case S::ReturnPump:f.ret();break;
case S::SetPreview:f.move(r[0].rl,0x1);break;
case S::ReadInput:f.move(r[0].rl,r[6].rl);break;
case S::LookupStatus:f.call(0x2ed4);break;
case S::ReadLength:f.move(r[1].rl,task.length());break;
case S::ReadFlags:f.move(r[1].rh,task.flags());break;
case S::MaskImmediate:r[1].rh&=0x2;value(h,r[1].rh);break;
case S::SelectImmediate:h.clockI(2);break;
case S::SelectLength:h.clockI(2);break;
case S::CaptureChannel:r[6].rl&=0xf;value(h,r[6].rl);break;
case S::SelectPayload:f.move(r[0].er,0x15c13);break;
case S::ReturnStatus:f.ret();break;
case S::ReadRemaining:f.move(r[0].rl,task.remaining());break;
case S::ReadCursor:f.move(r[1].er,task.cursor());break;
case S::DecrementRemaining:f.inc(r[0].rl,-1);break;
case S::AdvanceCursor:f.inc(r[1].er,1);break;
case S::ReturnData:f.ret();break;
case S::ReadRunningStatus:f.move(r[0].rl,task.statusByte());break;
case S::LookupRunning:f.call(0x2ed4);break;
case S::ReadRunningFlags:f.move(r[1].rl,task.flags());break;
case S::MaskRunning:r[1].rl&=0x1;value(h,r[1].rl);break;
case S::ReadRunningLength:f.move(r[1].rl,task.length());break;
case S::SelectRunningPayload:f.move(r[1].er,0x15c13);break;
case S::ReadHandler:f.move(r[0].er,task.handler());break;
case S::BindPayload:f.move(r[4].er,0x15c13);break;
case S::ReadChannel:f.move(r[1].rl,task.channel());break;
case S::DispatchMessage:h.clockI(2);f.next=0x328e;break;
case S::ClearReset:r[0].r=h.sub16(r[0].r,r[0].r);break;
case S::ResetSystem:f.call(0x31dc);break;
case S::ResetSysEx:f.call(0x3254);break;
case S::ReturnReset:f.ret();break;
case S::ReadImmediateHandler:f.move(r[0].er,task.handler());break;
case S::DispatchImmediate:f.next=task.handler()&0xffffff;break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native internal MIDI service clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
