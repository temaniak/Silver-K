#include "ksDiagnosticCodecCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
DiagnosticCodecPartialTest partialTest=nullptr;using T=DiagnosticCodecTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,64> points{{{0x1a2c,2},{0x1a2e,4},{0x1a32,6},{0x1a38,2},{0x1a3a,2},{0x1a3c,4},{0x1a40,2},{0x1a42,2},{0x1a44,2},{0x1a46,2},{0x1a48,2},{0x1a4a,2},{0x1a4c,4},{0x1a50,2},{0x1a52,4},{0x1a56,2},{0x1a58,4},{0x1a5c,4},{0x1a60,2},{0x1a62,4},{0x1a66,2},{0x1a68,2},{0x1a6a,2},{0x1a6c,2},{0x1a6e,4},{0x1a72,4},{0x1a76,2},{0x1a78,4},{0x1a7c,2},{0x1a7e,2},{0x1a80,2},{0x1a82,2},{0x1a84,4},{0x1a88,2},{0x1a8a,2},{0x1a8c,4},{0x1a90,2},{0x1a92,2},{0x1a94,2},{0x1a96,2},{0x1a98,2},{0x1a9a,4},{0x1a9e,2},{0x1aa0,2},{0x1aa2,2},{0x1aa4,2},{0x1aa6,2},{0x1aa8,2},{0x1aaa,2},{0x1aac,2},{0x1aae,2},{0x1ab0,2},{0x1ab2,2},{0x1ab4,2},{0x1ab6,2},{0x1ab8,2},{0x1aba,2},{0x1abc,2},{0x1abe,2},{0x1ac0,4},{0x1ac4,2},{0x1ac6,4},{0x1aca,2},{0x1acc,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setDiagnosticCodecPartialForTests(DiagnosticCodecPartialTest value){partialTest=value;}
bool DiagnosticCodecCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x1a2c:kind=T::Kind::Prefix;break;case 0x1a46:kind=T::Kind::Word;break;case 0x1a58:kind=T::Kind::Long;break;case 0x1a8a:kind=T::Kind::Block;break;case 0x1a98:kind=T::Kind::Group;break;case 0x1aac:kind=T::Kind::Sample;break;case 0x1ab6:kind=T::Kind::Tail;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native DiagnosticCodec capacity exceeded");slot->emplace(*kind,h.getSP(),uint32_t(h.regs[1].er),uint32_t(h.regs[6].er),uint32_t(h.regs[3].er),uint16_t(h.regs[2].r),uint16_t(h.regs[2].e));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint32_t(r[6].er),uint32_t(r[3].er),uint16_t(r[2].e),uint32_t(r[1].er));const auto stage=task.stage();
 const unsigned ref=task.source()&0xffffff;const unsigned store=ref<0x400000?3u+3u*(ref&1):ref>=0xfffd10? (ref>=0xffffa0?2u:1u+(ref&1)):6u;const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read f,uint32_t ref)->uint16_t{return f==T::Read::Prefix?uint8_t(h.read8(0x1b0 + ref)):uint16_t(h.read16(ref));};auto write=[](int,int){};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native DiagnosticCodec admission differs");
 switch(stage){case S::BindPrefixStart:f.move(r[0].rh,0xfffffff0);break;
case S::SendPrefixStart:f.call(0x119c);break;
case S::BindPrefixData:f.move(r[1].er,0x1b0);break;
case S::BindPrefixCount:f.move(r[0].rl,0x6);break;
case S::ReadPrefixByte:f.move(r[0].rh,task.byte());r[1].er+=1;h.clockI(2);break;
case S::SendPrefixByte:f.call(0x119c);break;
case S::DecrementPrefixCount:f.inc(r[0].rl,-1);break;
case S::ReturnPrefix:f.ret();break;
case S::ShiftWordPayload:f.shift(r[1].r,1,true);break;
case S::ClearWordLowHighBit:f.shift(r[1].rl,1,false);break;
case S::BindWordLow:f.move(r[0].rh,r[1].rl);break;
case S::SendWordLow:f.call(0x119c);break;
case S::BindWordHigh:f.move(r[0].rh,r[1].rh);break;
case S::SendWordHigh:f.call(0x119c);break;
case S::ReturnWord:f.ret();break;
case S::SaveLongPayload:f.push(r[1].er);break;
case S::MaskLongLow:r[1].r&=0x3fff;value(h,r[1].r);break;
case S::SendLongLow:f.call(0x1a46);break;
case S::RestoreLongPayload:f.pop(r[1].er);break;
case S::BindLongShiftCount:f.move(r[0].rl,0xe);break;
case S::ShiftLongPayload:f.shift(r[1].er,1,false);break;
case S::DecrementLongShiftCount:f.inc(r[0].rl,-1);break;
case S::SaveLongHigh:f.push(r[1].er);break;
case S::MaskLongHigh:r[1].r&=0x3fff;value(h,r[1].r);break;
case S::SendLongHigh:f.call(0x1a46);break;
case S::RestoreLongHigh:f.pop(r[1].er);break;
case S::ShiftLongTopFirst:f.shift(r[1].er,1,true);break;
case S::ShiftLongTopSecond:f.shift(r[1].er,1,true);break;
case S::BindLongTop:f.move(r[1].r,r[1].e);break;
case S::BindLongTopByte:f.move(r[0].rh,r[1].rl);break;
case S::SendLongTop:f.call(0x119c);break;
case S::ReturnLong:f.ret();break;
case S::ClearBlockChecksum:r[3].er=h.sub32(r[3].er,r[3].er);break;
case S::BindBlockCount:f.move(r[0].e,0x20);break;
case S::PackBlockGroup:f.call(0x1a98);break;
case S::DecrementBlockCount:f.inc(r[0].e,-1);break;
case S::ReturnBlock:f.ret();break;
case S::ClearGroupCarry:r[2].e=h.sub16(r[2].e,r[2].e);break;
case S::BindGroupWeight:f.move(r[2].r,0x1);break;
case S::PackGroupSample:f.call(0x1aac);break;
case S::ShiftGroupWeightFirst:f.shift(r[2].r,1,true);break;
case S::ShiftGroupWeightSecond:f.shift(r[2].r,1,true);break;
case S::TestGroupWeight:h.sub8(r[2].rh,0x40);break;
case S::BindGroupTail:f.move(r[1].r,r[2].e);break;
case S::ReadSample:f.move(r[1].r,task.sample());r[6].er+=2;h.clockI(2);break;
case S::ClearSampleHigh:r[1].e=h.sub16(r[1].e,r[1].e);break;
case S::AddSampleChecksum:r[3].er=h.add32(r[3].er,r[1].er);break;
case S::MultiplySample:r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[2].r));h.clockI(20);break;
case S::MergeSampleCarry:r[1].r|=r[2].e;value(h,r[1].r);break;
case S::ShiftPackedFirst:f.shift(r[1].er,1,true);break;
case S::ShiftPackedSecond:f.shift(r[1].er,1,true);break;
case S::ClearPackedHighBit:f.shift(r[1].r,1,false);break;
case S::ClearPackedLowHighBit:f.shift(r[1].rl,1,false);break;
case S::BindPackedLow:f.move(r[0].rh,r[1].rl);break;
case S::SendPackedLow:f.call(0x119c);break;
case S::BindPackedHigh:f.move(r[0].rh,r[1].rh);break;
case S::SendPackedHigh:f.call(0x119c);break;
case S::PublishGroupCarry:f.move(r[2].e,r[1].e);break;
case S::ReturnPacked:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native DiagnosticCodec clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
