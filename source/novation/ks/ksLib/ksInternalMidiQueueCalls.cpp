#include "ksInternalMidiQueueCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
InternalMidiQueuePartialTest partialTest=nullptr;using T=InternalMidiQueueTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,44> points{{{0x2c30,2},{0x2c32,2},{0x2c34,6},{0x2c3a,6},{0x2c40,6},{0x2c46,2},{0x2c48,2},{0x2c4a,2},{0x2c4c,4},{0x2c50,4},{0x2c54,6},{0x2c5a,6},{0x2c60,2},{0x2c62,2},{0x2c64,6},{0x2c6a,2},{0x2c6c,2},{0x2c6e,2},{0x2c70,4},{0x2c74,6},{0x2c7a,2},{0x2c7c,2},{0x2c7e,4},{0x2c82,4},{0x2c86,2},{0x2c88,4},{0x2c8c,4},{0x2c90,6},{0x2c96,2},{0x2c98,6},{0x2c9e,2},{0x2ca0,2},{0x2ca2,2},{0x2ca4,4},{0x2ca8,6},{0x2cae,2},{0x2cb0,2},{0x2cb2,6},{0x2cb8,2},{0x2cba,2},{0x2cbc,6},{0x2cc2,4},{0x2cc6,4},{0x2cca,2}}};
unsigned byteClocks(unsigned address,int previous){return address<0x400000&&!(previous&1)&&address==(unsigned(previous)|1u)?0:3;}
}
void setInternalMidiQueuePartialForTests(InternalMidiQueuePartialTest value){partialTest=value;}
bool InternalMidiQueueCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
 for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 if(pc==0x2c30||pc==0x2c4a||pc==0x2c88){
  if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
  if(!slot)throw std::runtime_error("Native internal MIDI queue capacity exceeded");
  slot->emplace(pc==0x2c30?T::Kind::Reset:pc==0x2c4a?T::Kind::Pop:T::Kind::Push,h.getSP(),uint8_t(h.regs[6].rl));++started;
 }
 if(!slot||points[unsigned((*slot)->task.stage())].pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;const auto stage=task.stage();
 const auto byteWrite=byteClocks(0x15b08+task.index(),h.lastwrite),flagWrite=byteClocks(0x15c08,h.lastwrite);
 const auto cost=task.nextClocks(byteWrite,flagWrite);const auto before=h.cycles;RuntimeBoundary f(h,pc,points[unsigned(stage)].length);
 auto read=[&](T::Read field)->uint16_t{return field==T::Read::Byte?uint8_t(h.read8(0x15b08+task.index())):h.read16(field==T::Read::Consumer?0x15b06:0x15b04);};
 auto write=[&](T::Write field,uint16_t value){if(field==T::Write::Byte||field==T::Write::Overflow)f.store8(field==T::Write::Byte?0x15b08+task.index():0x15c08,int8_t(value));else f.store16(field==T::Write::Consumer?0x15b06:0x15b04,int16_t(value));};
 if(task.advanceBefore(cost,read,write,byteWrite,flagWrite)!=cost)throw std::runtime_error("Native internal MIDI queue admission differs");
 switch(stage){
 case S::SaveReset:f.push(r[0].r);break;
 case S::ClearReset:r[0].r=h.sub16(r[0].r,r[0].r);break;
 case S::RestoreReset:f.pop(r[0].r);break;
 case S::ReturnReset:f.ret();break;
 case S::ClearResult:r[6].r=h.sub16(r[6].r,r[6].r);break;
 case S::SavePop:f.push(r[0].er);break;
 case S::SavePopAddress:f.push(r[1].er);break;
 case S::ReadConsumer:f.move(r[0].r,task.consumer());break;
 case S::ReadProducer:f.move(r[0].e,task.producer());break;
 case S::CompareEmpty:h.sub16(r[0].r,r[0].e);break;
 case S::SelectPopBuffer:f.move(r[1].er,0x15b08);break;
 case S::NormalizeConsumer:r[0].er&=0xffff;value(h,r[0].er);break;
 case S::LocatePop:r[1].er=h.add32(r[1].er,r[0].er);break;
 case S::AdvanceConsumer:f.inc(r[0].r,1);break;
 case S::WrapConsumer:r[0].r&=0xff;value(h,r[0].r);break;
 case S::ReadByte:f.move(r[6].rl,task.value());break;
 case S::SetAvailable:r[6].rh|=0x1;value(h,r[6].rh);break;
 case S::RestorePopAddress:f.pop(r[1].er);break;
 case S::RestorePop:f.pop(r[0].er);break;
 case S::ReturnPop:f.ret();break;
 case S::SavePush:f.push(r[0].er);break;
 case S::SavePushAddress:f.push(r[1].er);break;
 case S::ReadPushProducer:f.move(r[0].r,task.producer());break;
 case S::NormalizeProducer:r[0].er&=0xffff;value(h,r[0].er);break;
 case S::SelectPushBuffer:f.move(r[1].er,0x15b08);break;
 case S::LocatePush:r[1].er=h.add32(r[1].er,r[0].er);break;
 case S::AdvanceProducer:f.inc(r[0].r,1);break;
 case S::WrapProducer:r[0].r&=0xff;value(h,r[0].r);break;
 case S::ReadPushConsumer:f.move(r[1].r,task.consumer());break;
 case S::CompareFull:h.sub16(r[1].r,r[0].r);break;
 case S::MarkOverflow:f.move(r[1].rl,0x2);break;
 case S::RestorePushAddress:f.pop(r[1].er);break;
 case S::RestorePush:f.pop(r[0].er);break;
 case S::ReturnPush:f.ret();break;
 default:break;
 }
 if(h.cycles-before!=cost)throw std::runtime_error("Native internal MIDI queue clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 call.token=h.getSP();h.pc=h.makepc(points[unsigned(task.stage())].pc);return true;
}
}
