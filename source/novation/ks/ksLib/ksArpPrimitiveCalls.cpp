#include "ksArpPrimitiveCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace {ArpPrimitivePartialTest partialTest=nullptr;using T=ArpPrimitiveTask;using S=T::Stage;using K=T::Kind;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){
case S::ZeroResult:return {0x36d2,2};
case S::FinishNull:return {0x36d4,2};
case S::DispatchPart:return {0x36d6,4};
case S::AwaitPart:return {0x36da,8};
case S::ReadTempo:return {0x36da,8};
case S::CompareTempo:return {0x36e2,2};
case S::AdmitTempo:return {0x36e4,2};
case S::PublishTempo:return {0x36e6,8};
case S::ScaleIndex:return {0x36ee,2};
case S::NormalizeIndexWord:return {0x36f0,2};
case S::NormalizeIndex:return {0x36f2,2};
case S::ReadRate:return {0x36f4,10};
case S::PublishRate:return {0x36fe,10};
case S::DispatchReset:return {0x3708,4};
case S::AwaitReset:return {0x370c,2};
case S::FinishTempo:return {0x370c,2};
case S::ZeroPhase:return {0x3a38,2};
case S::PublishAccumulator:return {0x3a3a,10};
case S::PublishCursorZero:return {0x3a44,8};
case S::PublishPhaseZero:return {0x3a4c,8};
case S::PublishGateZero:return {0x3a54,8};
case S::ReadMode:return {0x3a5c,8};
case S::ObserveMono:return {0x3a64,2};
case S::AdmitMono:return {0x3a66,2};
case S::DispatchMono:return {0x3a68,4};
case S::AwaitMono:return {0x3a6c,2};
case S::FinishReset:return {0x3a6c,2};
case S::RetainInput:return {0x3a6e,2};
case S::ClearPatternIndex:return {0x3a70,2};
case S::ReadCursor:return {0x3a72,8};
case S::CaptureCursor:return {0x3a7a,2};
case S::DoubleCursor:return {0x3a7c,2};
case S::ReadPattern:return {0x3a7e,10};
case S::LocateStep:return {0x3a88,2};
case S::ReadStep:return {0x3a8a,2};
case S::TickCursor:return {0x3a8c,2};
case S::ReadLength:return {0x3a8e,8};
case S::CompareLength:return {0x3a96,2};
case S::AdmitLength:return {0x3a98,2};
case S::WrapCursor:return {0x3a9a,2};
case S::ReloadPattern:return {0x3a9c,10};
case S::PublishCursor:return {0x3aa6,8};
case S::ReleaseInput:return {0x3aae,2};
case S::FinishPattern:return {0x3ab0,2};
default:throw std::runtime_error("Invalid arpeggiator primitive stage");}}
unsigned offset(T::Write w){switch(w){case T::Write::Tempo:return 328;case T::Write::Rate:return 4;case T::Write::Accumulator:return 60;case T::Write::CursorZero:case T::Write::Cursor:return 316;case T::Write::PhaseZero:return 304;default:return 317;}}
uint8_t boundPart(ControllerCpu& h){auto a=uint32_t(h.regs[0].er);if(a<0x160d2||a>=0x160d2 + 256*334||(a-0x160d2)%334)throw std::runtime_error("Invalid arp primitive part binding");return uint8_t((a-0x160d2)/334);}
}
void setArpPrimitivePartialForTests(ArpPrimitivePartialTest fn){partialTest=fn;}
bool ArpPrimitiveCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 const bool root=pc==0x36d2||pc==0x36d6||pc==0x3a38||pc==0x3a6e;
 if(root&&(pc==0x3a38||pc==0x3a6e)&&partialTest){const auto a=uint32_t(v[0].er);if(a<0x160d2||a>=0x160d2 + 256*334||(a-0x160d2)%334){if(partialTest(h)){++partialEntries;return true;}}}
 if(root){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Arp primitive capacity exceeded");selected->emplace(pc==0x36d2?K::NullResult:pc==0x36d6?K::Tempo:pc==0x3a38?K::Reset:K::Pattern,(pc==0x3a38||pc==0x3a6e)?boundPart(h):0,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;if(task.awaitingPart())task.resumePart(boundPart(h));if(task.awaitingChild())task.resumeChild();
 const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const auto base=0x160d2 + unsigned(task.part())*334;
 auto timing=[&](T::Write field){if(stage==S::ReadStep){const auto at=task.nextReference()&0xffffffu;return at>=0x400000u||(at&1)?6u:3u;}const auto at=base+offset(field);return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=task.nextClocks(task.publication()?timing(task.publicationKind()):3,stage==S::ReadStep?timing(T::Write::Rate):3);
 auto read=[&](T::Read field)->uint32_t{switch(field){case T::Read::Tempo:return uint8_t(h.read8(base+328));case T::Read::DesiredTempo:return uint8_t(v[1].rl);case T::Read::Rate:return uint32_t(h.read32(62632 + task.rateIndex()));case T::Read::Mode:return uint8_t(h.read8(base+324));case T::Read::Cursor:return uint8_t(h.read8(base+316));case T::Read::Pattern:return uint32_t(h.read32(base+8));case T::Read::Step:return uint16_t(h.read16(task.nextReference()));default:return uint8_t(h.read8(base+315));}};
 auto write=[&](T::Write field,uint32_t data){const auto at=base+offset(field);if(field==T::Write::Rate||field==T::Write::Accumulator)h.write32(int32_t(data),at);else h.write8(int8_t(data),at);};if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Arp primitive deadline mismatch");
 switch(stage){case S::ZeroResult:v[0].er=h.sub32(v[0].er,v[0].er);break;case S::ReadTempo:v[1].rh=int8_t(task.tempo());value(h,v[1].rh);break;case S::CompareTempo:h.sub8(v[1].rl,v[1].rh);break;case S::PublishTempo:value(h,int8_t(task.desiredTempo()));break;case S::ScaleIndex:clocks.shift(v[1].rl,2,true);break;case S::NormalizeIndexWord:v[1].r&=0xff;value(h,v[1].r);break;case S::NormalizeIndex:v[1].er&=0xffff;value(h,v[1].er);break;case S::ReadRate:v[1].er=int32_t(task.rate());value(h,v[1].er);break;case S::PublishRate:value(h,int32_t(task.rate()));break;
 case S::ZeroPhase:v[1].er=h.sub32(v[1].er,v[1].er);break;case S::PublishAccumulator:value(h,0);break;case S::PublishCursorZero:case S::PublishPhaseZero:case S::PublishGateZero:value(h,int8_t(0));break;case S::ReadMode:v[1].rl=int8_t(task.mode());value(h,v[1].rl);break;case S::ObserveMono:flag(h,4,(task.mode()&64)==0);break;
 case S::RetainInput:clocks.push(v[2].r);call.depth+=2;break;case S::ClearPatternIndex:v[4].er=h.sub32(v[4].er,v[4].er);break;case S::ReadCursor:v[4].rl=int8_t(task.cursor());value(h,v[4].rl);break;case S::CaptureCursor:v[2].rl=int8_t(task.nextCursor());value(h,v[2].rl);break;case S::DoubleCursor:clocks.shift(v[4].rl,1,true);break;case S::ReadPattern:v[1].er=int32_t(task.pattern());value(h,v[1].er);break;case S::LocateStep:v[1].er=h.add32(v[1].er,v[4].er);break;
 case S::ReadStep:v[4].r=int16_t(task.step());value(h,v[4].r);v[1].er=int32_t(task.nextReference());h.clockI(2);break;case S::TickCursor:clocks.inc(v[2].rl,1);break;case S::ReadLength:v[2].rh=int8_t(task.length());value(h,v[2].rh);break;case S::CompareLength:h.sub8(v[2].rl,v[2].rh);break;case S::WrapCursor:v[2].rl=0;value(h,v[2].rl);break;case S::ReloadPattern:v[1].er=int32_t(task.nextReference());value(h,v[1].er);break;case S::PublishCursor:value(h,int8_t(task.nextCursor()));break;case S::ReleaseInput:clocks.pop(v[2].r);call.depth-=2;break;
 case S::DispatchPart:case S::DispatchReset:case S::DispatchMono:clocks.call(stage==S::DispatchPart?0x4000:stage==S::DispatchReset?0x3a38:0xb40c);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp primitive call timing");return true;
 case S::FinishNull:case S::FinishTempo:case S::FinishReset:case S::FinishPattern:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp primitive return timing");selected->reset();++completed;return true;default:break;}
 if(h.cycles-begin!=cost)throw std::runtime_error("Arp primitive bus timing");h.pc=h.makepc(point(task).location);return true;
}
}
