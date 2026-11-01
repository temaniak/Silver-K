#include "ksArpSelectCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace {ArpSelectPartialTest partialTest=nullptr;using T=ArpSelectTask;using K=T::Kind;using S=T::Stage;using C=T::Child;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){case S::BindPart:return {0x3ca4,4};case S::ClearQueue:return {0x3ca8,2};case S::PublishQueue:return {0x3caa,8};case S::ReadMode:return {0x3cb2,8};case S::ObserveOrdered:return {0x3cba,2};case S::AdmitOrdered:return {0x3cbc,4};case S::ReadCount:return {0x3cc0,8};case S::AdmitCount:return {0x3cc8,4};case S::ObserveChord:return {0x3ccc,2};case S::AdmitChord:return {0x3cce,2};case S::ClearOrderedIndex:return {0x3cd0,2};case S::PublishOrderedIndex:return {0x3cd2,8};case S::CaptureChordCount:return {0x3cda,2};case S::RetainChord:return {0x3cdc,2};case S::RetainOrdered:return {0x3cde,2};case S::OrderedChild:return {0x3ce0,2};case S::ReleaseOrdered:return {0x3ce2,2};case S::ReleaseChord:return {0x3ce4,2};case S::TickChord:return {0x3ce6,2};case S::AdmitChordCount:return {0x3ce8,2};case S::FinishChord:return {0x3cea,2};case S::OrderedClearIndex:return {0x3cec,2};case S::OrderedReadIndex:return {0x3cee,8};case S::OrderedCaptureIndex:return {0x3cf6,2};case S::OrderedObserveRandom:return {0x3cf8,2};case S::OrderedAdmitRandom:return {0x3cfa,2};case S::OrderedRandom:return {0x3cfc,2};case S::OrderedCaptureRandom:return {0x3cfe,2};case S::OrderedScaleIndex:return {0x3d00,2};case S::OrderedBindKey:return {0x3d02,2};case S::OrderedReadKey:return {0x3d04,8};case S::OrderedRestoreIndex:return {0x3d0c,2};case S::OrderedTickIndex:return {0x3d0e,2};case S::OrderedCompareCount:return {0x3d10,2};case S::OrderedAdmitIndex:return {0x3d12,2};case S::OrderedWrapIndex:return {0x3d14,2};case S::OrderedReadOctave:return {0x3d16,8};case S::OrderedPublishOldOctave:return {0x3d1e,8};case S::OrderedReadRange:return {0x3d26,8};case S::OrderedCompareRange:return {0x3d2e,2};case S::OrderedAdmitRange:return {0x3d30,2};case S::OrderedWrapOctave:return {0x3d32,2};case S::OrderedSkipTick:return {0x3d34,2};case S::OrderedTickOctave:return {0x3d36,2};case S::OrderedPublishOctave:return {0x3d38,8};case S::OrderedPublishIndex:return {0x3d40,8};case S::OrderedRetainPart:return {0x3d48,4};case S::OrderedEmit:return {0x3d4c,4};case S::OrderedReleasePart:return {0x3d50,4};case S::OrderedFinish:return {0x3d54,2};case S::RandomClock:return {0x3d56,4};case S::RandomReadSeed:return {0x3d5a,6};case S::RandomMultiplySeed:return {0x3d60,2};case S::RandomReadIncrement:return {0x3d62,6};case S::RandomAddIncrement:return {0x3d68,2};case S::RandomPublishSeed:return {0x3d6a,6};case S::RandomCaptureSeed:return {0x3d70,2};case S::RandomReadCount:return {0x3d72,8};case S::RandomNormalizeCount:return {0x3d7a,2};case S::RandomScaleCount:return {0x3d7c,2};case S::RandomCaptureIndex:return {0x3d7e,2};case S::RandomFinish:return {0x3d80,2};case S::ScanHeld:return {0x3d82,4};case S::ObserveHeld:return {0x3d86,2};case S::AdmitHeld:return {0x3d88,4};case S::BindScanPart:return {0x3d8c,4};case S::BindMap:return {0x3d90,2};case S::LocateMap:return {0x3d92,6};case S::ReadLast:return {0x3d98,8};case S::ReadDirection:return {0x3da0,8};case S::AdmitDirection:return {0x3da8,4};case S::ForwardFirst:return {0x3dac,4};case S::AdmitForwardFirst:return {0x3db0,4};case S::BindForwardPart:return {0x3db4,4};case S::ReadForwardOctave:return {0x3db8,8};case S::ReadForwardRange:return {0x3dc0,8};case S::CompareForwardRange:return {0x3dc8,2};case S::AdmitForwardEnd:return {0x3dca,2};case S::TickForwardOctave:return {0x3dcc,2};case S::PublishForwardOctave:return {0x3dce,8};case S::SkipForwardEnd:return {0x3dd6,2};case S::ReadForwardMode:return {0x3dd8,8};case S::ObserveBounceForward:return {0x3de0,2};case S::AdmitBounceForward:return {0x3de2,2};case S::ClearForwardOctave:return {0x3de4,2};case S::PublishWrappedForwardOctave:return {0x3de6,8};case S::ClearForwardNote:return {0x3dee,2};case S::PublishForwardNote:return {0x3df0,8};case S::ForwardWrapped:return {0x3df8,4};case S::SkipForward:return {0x3dfc,4};case S::ReadBounceForwardMode:return {0x3e00,8};case S::ObserveRepeatForward:return {0x3e08,2};case S::AdmitRepeatForward:return {0x3e0a,2};case S::LastForwardNote:return {0x3e0c,2};case S::PublishLastForwardNote:return {0x3e0e,8};case S::SetDescending:return {0x3e16,2};case S::PublishDescending:return {0x3e18,8};case S::ReadDescendingRange:return {0x3e20,8};case S::PublishDescendingRange:return {0x3e28,8};case S::BackwardBounce:return {0x3e30,4};case S::AdmitBackwardBounce:return {0x3e34,4};case S::BindBackwardFallback:return {0x3e38,4};case S::ReadBackwardFallbackRange:return {0x3e3c,8};case S::AdmitBackwardFallbackRange:return {0x3e44,2};case S::TickBackwardFallbackOctave:return {0x3e46,2};case S::PublishBackwardFallbackOctave:return {0x3e48,8};case S::CaptureBackwardFallbackNote:return {0x3e50,2};case S::EmitBackwardFallback:return {0x3e52,4};case S::SkipBackwardFallback:return {0x3e56,4};case S::BackwardFirst:return {0x3e5a,4};case S::AdmitBackwardFirst:return {0x3e5e,4};case S::BindBackwardPart:return {0x3e62,4};case S::ReadBackwardOctave:return {0x3e66,8};case S::AdmitBackwardStart:return {0x3e6e,2};case S::TickBackwardOctave:return {0x3e70,2};case S::PublishBackwardOctave:return {0x3e72,8};case S::SkipBackwardStart:return {0x3e7a,2};case S::ReadBackwardMode:return {0x3e7c,8};case S::ObserveBounceBackward:return {0x3e84,2};case S::AdmitBounceBackward:return {0x3e86,2};case S::ReadWrappedBackwardRange:return {0x3e88,8};case S::PublishWrappedBackwardRange:return {0x3e90,8};case S::LastBackwardNote:return {0x3e98,2};case S::PublishBackwardNote:return {0x3e9a,8};case S::BackwardWrapped:return {0x3ea2,2};case S::SkipBackward:return {0x3ea4,2};case S::ReadBounceBackwardMode:return {0x3ea6,8};case S::ObserveRepeatBackward:return {0x3eae,2};case S::AdmitRepeatBackward:return {0x3eb0,2};case S::FirstBackwardNote:return {0x3eb2,2};case S::PublishFirstBackwardNote:return {0x3eb4,8};case S::SetAscending:return {0x3ebc,2};case S::PublishAscending:return {0x3ebe,8};case S::PublishAscendingOctave:return {0x3ec6,8};case S::ForwardBounce:return {0x3ece,2};case S::AdmitForwardBounce:return {0x3ed0,2};case S::BindForwardFallback:return {0x3ed2,4};case S::ReadForwardFallbackRange:return {0x3ed6,8};case S::AdmitForwardFallbackRange:return {0x3ede,2};case S::SetForwardFallbackOctave:return {0x3ee0,2};case S::PublishForwardFallbackOctave:return {0x3ee2,8};case S::CaptureForwardFallbackNote:return {0x3eea,2};case S::EmitForwardFallback:return {0x3eec,2};case S::Finish:return {0x3eee,2};case S::ForwardSearch:return {0x3ef0,4};case S::ForwardObserve:return {0x3ef4,2};case S::ForwardAdmit:return {0x3ef6,2};case S::ForwardFinish:return {0x3ef8,2};case S::BackwardSearch:return {0x3efa,4};case S::BackwardObserve:return {0x3efe,2};case S::BackwardAdmit:return {0x3f00,2};case S::BackwardFinish:return {0x3f02,2};case S::VelocityCaptureNote:return {0x3f04,2};case S::VelocityNormalizeWord:return {0x3f06,2};case S::VelocityNormalize:return {0x3f08,2};case S::VelocityBindPart:return {0x3f0a,4};case S::VelocityLocate:return {0x3f0e,2};case S::VelocityRead:return {0x3f10,8};case S::HandoffEmit:return {0x3f18,0};default:throw std::runtime_error("Invalid arp selection stage");}}uint8_t binding(uint32_t a){if(a<0x160d2||a>=0x160d2 + 256*334||(a-0x160d2)%334)throw std::runtime_error("Invalid arp selection part");return uint8_t((a-0x160d2)/334);}}
void setArpSelectPartialForTests(ArpSelectPartialTest fn){partialTest=fn;}
bool ArpSelectCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 const bool root=pc==0x3ca4||pc==0x3d56||pc==0x3ef0||pc==0x3efa||((pc==0x3cec||pc==0x3f04)&&(!selected||point((*selected)->task).location!=pc));
 if(root&&pc!=0x3ca4&&partialTest){const auto a=uint32_t(v[0].er);if(a<0x160d2||a>=0x160d2 + 256*334||(a-0x160d2)%334){if(partialTest(h)){++partialEntries;return true;}}}
 if(root){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Arp selection capacity exceeded");const auto kind=pc==0x3ca4?K::Run:pc==0x3cec?K::Ordered:pc==0x3d56?K::Random:pc==0x3ef0?K::Forward:pc==0x3efa?K::Backward:K::Velocity;selected->emplace(kind,kind==K::Run?0:binding(uint32_t(v[0].er)),uint8_t(v[6].rl),uint8_t(v[1].rl),uint8_t(v[5].rl),uint8_t(v[4].rl),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& t=call.task;
 if(t.awaitingChild()){switch(t.child()){case C::Part:t.resumePart(binding(uint32_t(v[0].er)));break;case C::Held:t.resumeHeld(binding(uint32_t(v[0].er)),v[1].r!=0);break;case C::Clock:t.resumeClock(uint32_t(v[6].er));break;case C::Random:t.resumeRandom(uint16_t(v[6].r));break;case C::SearchForward:case C::SearchBackward:t.resumeSearch(uint8_t(v[4].rl));break;case C::Forward:case C::Backward:t.resumeKey(uint8_t(v[5].rl),(h.ccr&8)==0);break;default:t.resumeChild();break;}}
 const auto stage=t.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(t).length);const auto base=0x160d2 + unsigned(t.part())*334;uint32_t readData=0;
 auto publicationAddress=[&](S stage)->unsigned{switch(stage){case S::PublishQueue:return base+320;case S::PublishOrderedIndex:return base+308;case S::OrderedPublishOldOctave:return base+321;case S::OrderedPublishOctave:return base+326;case S::OrderedPublishIndex:return base+308;case S::PublishForwardOctave:return base+326;case S::PublishWrappedForwardOctave:return base+326;case S::PublishForwardNote:return base+322;case S::PublishLastForwardNote:return base+322;case S::PublishDescending:return base+323;case S::PublishDescendingRange:return base+326;case S::PublishBackwardFallbackOctave:return base+326;case S::PublishBackwardOctave:return base+326;case S::PublishWrappedBackwardRange:return base+326;case S::PublishBackwardNote:return base+322;case S::PublishFirstBackwardNote:return base+322;case S::PublishAscending:return base+323;case S::PublishAscendingOctave:return base+326;case S::PublishForwardFallbackOctave:return base+326;default:throw std::runtime_error("Invalid arp selection publication");}};auto timing=[&](S stage){const auto at=publicationAddress(stage);return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=t.nextClocks(t.publication()?timing(stage):3);
 auto read=[&](S stage)->uint32_t{switch(stage){case S::ReadMode:return uint8_t(h.read8(base+324));case S::ReadCount:return uint8_t(h.read8(base+307));case S::OrderedReadIndex:return uint8_t(h.read8(base+308));case S::OrderedReadKey:return uint16_t(h.read16(base+240+t.keyOffset()));case S::OrderedReadOctave:return uint8_t(h.read8(base+326));case S::OrderedReadRange:return uint8_t(h.read8(base+325));case S::RandomReadSeed:return uint16_t(h.read16(0x15c30));case S::RandomReadIncrement:return uint16_t(h.read16(0x15c2e));case S::RandomReadCount:return uint8_t(h.read8(base+307));case S::ReadLast:return uint8_t(h.read8(base+322));case S::ReadDirection:return uint8_t(h.read8(base+323));case S::ReadForwardOctave:return uint8_t(h.read8(base+326));case S::ReadForwardRange:return uint8_t(h.read8(base+325));case S::ReadForwardMode:return uint8_t(h.read8(base+324));case S::ReadBounceForwardMode:return uint8_t(h.read8(base+324));case S::ReadDescendingRange:return uint8_t(h.read8(base+325));case S::ReadBackwardFallbackRange:return uint8_t(h.read8(base+325));case S::ReadBackwardOctave:return uint8_t(h.read8(base+326));case S::ReadBackwardMode:return uint8_t(h.read8(base+324));case S::ReadWrappedBackwardRange:return uint8_t(h.read8(base+325));case S::ReadBounceBackwardMode:return uint8_t(h.read8(base+324));case S::ReadForwardFallbackRange:return uint8_t(h.read8(base+325));case S::VelocityRead:return uint8_t(h.read8(base+64+t.candidate()));default:throw std::runtime_error("Invalid arp selection read");}};auto capture=[&](S stage){return readData=read(stage);};auto write=[&](S stage,uint32_t data){switch(stage){case S::PublishQueue:h.write8(int8_t(data),base+320);break;case S::PublishOrderedIndex:h.write8(int8_t(data),base+308);break;case S::OrderedPublishOldOctave:h.write8(int8_t(data),base+321);break;case S::OrderedPublishOctave:h.write8(int8_t(data),base+326);break;case S::OrderedPublishIndex:h.write8(int8_t(data),base+308);break;case S::RandomPublishSeed:h.write16(int16_t(data),0x15c30);break;case S::PublishForwardOctave:h.write8(int8_t(data),base+326);break;case S::PublishWrappedForwardOctave:h.write8(int8_t(data),base+326);break;case S::PublishForwardNote:h.write8(int8_t(data),base+322);break;case S::PublishLastForwardNote:h.write8(int8_t(data),base+322);break;case S::PublishDescending:h.write8(int8_t(data),base+323);break;case S::PublishDescendingRange:h.write8(int8_t(data),base+326);break;case S::PublishBackwardFallbackOctave:h.write8(int8_t(data),base+326);break;case S::PublishBackwardOctave:h.write8(int8_t(data),base+326);break;case S::PublishWrappedBackwardRange:h.write8(int8_t(data),base+326);break;case S::PublishBackwardNote:h.write8(int8_t(data),base+322);break;case S::PublishFirstBackwardNote:h.write8(int8_t(data),base+322);break;case S::PublishAscending:h.write8(int8_t(data),base+323);break;case S::PublishAscendingOctave:h.write8(int8_t(data),base+326);break;case S::PublishForwardFallbackOctave:h.write8(int8_t(data),base+326);break;default:throw std::runtime_error("Invalid arp selection write");}};if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("Arp selection deadline mismatch");switch(stage){
case S::BindPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::ClearQueue:{clocks.move(v[1].rl,0x0);break;}
case S::PublishQueue:{value(h,v[1].rl);break;}
case S::ReadMode:{clocks.move(v[6].rl,int8_t(readData));break;}
case S::ObserveOrdered:{flag(h,4,(uint8_t(v[6].rl)&(1u<<0x3))==0);break;}
case S::AdmitOrdered:{h.clockI(2);break;}
case S::ReadCount:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AdmitCount:{h.clockI(2);break;}
case S::ObserveChord:{flag(h,4,(uint8_t(v[6].rl)&(1u<<0x5))==0);break;}
case S::AdmitChord:{;break;}
case S::ClearOrderedIndex:{clocks.move(v[1].rh,0x0);break;}
case S::PublishOrderedIndex:{value(h,v[1].rh);break;}
case S::CaptureChordCount:{clocks.move(v[6].rh,v[1].rl);break;}
case S::RetainChord:{clocks.push(v[6].r);call.depth+=2;break;}
case S::RetainOrdered:{clocks.push(v[1].r);call.depth+=2;break;}
case S::OrderedChild:{clocks.call(0x3cec);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::ReleaseOrdered:{clocks.pop(v[1].r);call.depth-=2;break;}
case S::ReleaseChord:{clocks.pop(v[6].r);call.depth-=2;break;}
case S::TickChord:{clocks.inc(v[6].rh,-1);break;}
case S::AdmitChordCount:{;break;}
case S::FinishChord:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection return timing");selected->reset();++completed;return true;break;}
case S::OrderedClearIndex:{v[4].er=h.sub32(v[4].er,v[4].er);break;}
case S::OrderedReadIndex:{clocks.move(v[4].rl,int8_t(readData));break;}
case S::OrderedCaptureIndex:{clocks.move(v[1].rh,v[4].rl);break;}
case S::OrderedObserveRandom:{flag(h,4,(uint8_t(v[6].rl)&(1u<<0x4))==0);break;}
case S::OrderedAdmitRandom:{;break;}
case S::OrderedRandom:{clocks.call(0x3d56);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::OrderedCaptureRandom:{clocks.move(v[4].rl,v[6].rl);break;}
case S::OrderedScaleIndex:{clocks.shift(v[4].rl,1,true);break;}
case S::OrderedBindKey:{v[4].er=h.add32(v[4].er,v[0].er);break;}
case S::OrderedReadKey:{clocks.move(v[5].r,int16_t(readData));break;}
case S::OrderedRestoreIndex:{clocks.move(v[4].rl,v[1].rh);break;}
case S::OrderedTickIndex:{clocks.inc(v[4].rl,1);break;}
case S::OrderedCompareCount:{h.sub8(v[4].rl,v[1].rl);break;}
case S::OrderedAdmitIndex:{;break;}
case S::OrderedWrapIndex:{clocks.move(v[4].rl,0x0);break;}
case S::OrderedReadOctave:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::OrderedPublishOldOctave:{value(h,v[1].rl);break;}
case S::OrderedReadRange:{clocks.move(v[1].rh,int8_t(readData));break;}
case S::OrderedCompareRange:{h.sub8(v[1].rl,v[1].rh);break;}
case S::OrderedAdmitRange:{;break;}
case S::OrderedWrapOctave:{clocks.move(v[1].rl,0x0);break;}
case S::OrderedSkipTick:{;break;}
case S::OrderedTickOctave:{clocks.inc(v[1].rl,1);break;}
case S::OrderedPublishOctave:{value(h,v[1].rl);break;}
case S::OrderedPublishIndex:{value(h,v[4].rl);break;}
case S::OrderedRetainPart:{clocks.push(v[0].er);call.depth+=4;break;}
case S::OrderedEmit:{clocks.call(0x3f18);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::OrderedReleasePart:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::OrderedFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection return timing");selected->reset();++completed;return true;break;}
case S::RandomClock:{clocks.call(0x12fa);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::RandomReadSeed:{clocks.move(v[6].e,int16_t(readData));break;}
case S::RandomMultiplySeed:{v[6].er=uint16_t(v[6].er)*uint32_t(uint16_t(v[6].e));h.clockI(20);break;}
case S::RandomReadIncrement:{clocks.move(v[6].e,int16_t(readData));break;}
case S::RandomAddIncrement:{v[6].r=h.add16(v[6].r,v[6].e);break;}
case S::RandomPublishSeed:{value(h,v[6].r);break;}
case S::RandomCaptureSeed:{clocks.move(v[6].e,v[6].r);break;}
case S::RandomReadCount:{clocks.move(v[6].rl,int8_t(readData));break;}
case S::RandomNormalizeCount:{v[6].r&=0xff;value(h,v[6].r);break;}
case S::RandomScaleCount:{v[6].er=uint16_t(v[6].er)*uint32_t(uint16_t(v[6].e));h.clockI(20);break;}
case S::RandomCaptureIndex:{clocks.move(v[6].r,v[6].e);break;}
case S::RandomFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection return timing");selected->reset();++completed;return true;break;}
case S::ScanHeld:{clocks.call(0x30fe);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::ObserveHeld:{clocks.move(v[1].r,v[1].r);break;}
case S::AdmitHeld:{h.clockI(2);break;}
case S::BindScanPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::BindMap:{clocks.move(v[3].er,v[0].er);break;}
case S::LocateMap:{v[3].er=h.add32(v[3].er,0xd0);break;}
case S::ReadLast:{clocks.move(v[5].rl,int8_t(readData));break;}
case S::ReadDirection:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AdmitDirection:{h.clockI(2);break;}
case S::ForwardFirst:{clocks.call(0x3ef0);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::AdmitForwardFirst:{h.clockI(2);break;}
case S::BindForwardPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::ReadForwardOctave:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ReadForwardRange:{clocks.move(v[1].rh,int8_t(readData));break;}
case S::CompareForwardRange:{h.sub8(v[1].rl,v[1].rh);break;}
case S::AdmitForwardEnd:{;break;}
case S::TickForwardOctave:{clocks.inc(v[1].rl,1);break;}
case S::PublishForwardOctave:{value(h,v[1].rl);break;}
case S::SkipForwardEnd:{;break;}
case S::ReadForwardMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ObserveBounceForward:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x1))==0);break;}
case S::AdmitBounceForward:{;break;}
case S::ClearForwardOctave:{clocks.move(v[1].rl,0x0);break;}
case S::PublishWrappedForwardOctave:{value(h,v[1].rl);break;}
case S::ClearForwardNote:{clocks.move(v[5].rl,0x0);break;}
case S::PublishForwardNote:{value(h,v[5].rl);break;}
case S::ForwardWrapped:{clocks.call(0x3ef0);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::SkipForward:{h.clockI(2);break;}
case S::ReadBounceForwardMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ObserveRepeatForward:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x2))==0);break;}
case S::AdmitRepeatForward:{;break;}
case S::LastForwardNote:{clocks.move(v[5].rl,0x7f);break;}
case S::PublishLastForwardNote:{value(h,v[5].rl);break;}
case S::SetDescending:{clocks.move(v[1].rl,0x1);break;}
case S::PublishDescending:{value(h,v[1].rl);break;}
case S::ReadDescendingRange:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::PublishDescendingRange:{value(h,v[1].rl);break;}
case S::BackwardBounce:{clocks.call(0x3efa);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::AdmitBackwardBounce:{h.clockI(2);break;}
case S::BindBackwardFallback:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::ReadBackwardFallbackRange:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AdmitBackwardFallbackRange:{;break;}
case S::TickBackwardFallbackOctave:{clocks.inc(v[1].rl,-1);break;}
case S::PublishBackwardFallbackOctave:{value(h,v[1].rl);break;}
case S::CaptureBackwardFallbackNote:{clocks.move(v[4].rl,v[5].rl);break;}
case S::EmitBackwardFallback:{clocks.call(0x3f04);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::SkipBackwardFallback:{h.clockI(2);break;}
case S::BackwardFirst:{clocks.call(0x3efa);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::AdmitBackwardFirst:{h.clockI(2);break;}
case S::BindBackwardPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::ReadBackwardOctave:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AdmitBackwardStart:{;break;}
case S::TickBackwardOctave:{clocks.inc(v[1].rl,-1);break;}
case S::PublishBackwardOctave:{value(h,v[1].rl);break;}
case S::SkipBackwardStart:{;break;}
case S::ReadBackwardMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ObserveBounceBackward:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x0))==0);break;}
case S::AdmitBounceBackward:{;break;}
case S::ReadWrappedBackwardRange:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::PublishWrappedBackwardRange:{value(h,v[1].rl);break;}
case S::LastBackwardNote:{clocks.move(v[5].rl,0x7f);break;}
case S::PublishBackwardNote:{value(h,v[5].rl);break;}
case S::BackwardWrapped:{clocks.call(0x3efa);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::SkipBackward:{;break;}
case S::ReadBounceBackwardMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ObserveRepeatBackward:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x2))==0);break;}
case S::AdmitRepeatBackward:{;break;}
case S::FirstBackwardNote:{clocks.move(v[5].rl,0x0);break;}
case S::PublishFirstBackwardNote:{value(h,v[5].rl);break;}
case S::SetAscending:{clocks.move(v[1].rl,0x0);break;}
case S::PublishAscending:{value(h,v[1].rl);break;}
case S::PublishAscendingOctave:{value(h,v[1].rl);break;}
case S::ForwardBounce:{clocks.call(0x3ef0);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::AdmitForwardBounce:{;break;}
case S::BindForwardFallback:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::ReadForwardFallbackRange:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AdmitForwardFallbackRange:{;break;}
case S::SetForwardFallbackOctave:{clocks.move(v[1].rl,0x1);break;}
case S::PublishForwardFallbackOctave:{value(h,v[1].rl);break;}
case S::CaptureForwardFallbackNote:{clocks.move(v[4].rl,v[5].rl);break;}
case S::EmitForwardFallback:{clocks.call(0x3f04);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::Finish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection return timing");selected->reset();++completed;return true;break;}
case S::ForwardSearch:{clocks.call(0x317a);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::ForwardObserve:{clocks.move(v[4].rl,v[4].rl);break;}
case S::ForwardAdmit:{;break;}
case S::ForwardFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection return timing");selected->reset();++completed;return true;break;}
case S::BackwardSearch:{clocks.call(0x31ac);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::BackwardObserve:{clocks.move(v[4].rl,v[4].rl);break;}
case S::BackwardAdmit:{;break;}
case S::BackwardFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection return timing");selected->reset();++completed;return true;break;}
case S::VelocityCaptureNote:{clocks.move(v[5].rl,v[4].rl);break;}
case S::VelocityNormalizeWord:{v[4].r&=0xff;value(h,v[4].r);break;}
case S::VelocityNormalize:{v[4].er&=0xffff;value(h,v[4].er);break;}
case S::VelocityBindPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection dispatch timing");return true;break;}
case S::VelocityLocate:{v[4].er=h.add32(v[4].er,v[0].er);break;}
case S::VelocityRead:{clocks.move(v[5].rh,int8_t(readData));break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("Arp selection bus timing");h.pc=h.makepc(point(t).location);if(t.handoff()){selected->reset();++handedOff;}return true;}}
