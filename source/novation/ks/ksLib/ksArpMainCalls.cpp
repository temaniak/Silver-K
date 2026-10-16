#include "ksArpMainCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace {ArpMainPartialTest partialTest=nullptr;using T=ArpMainTask;using S=T::Stage;using C=T::Child;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){case S::BindControls:return {0x3770,4};case S::ReadPatch:return {0x3774,10};case S::BindPart:return {0x377e,4};case S::ReadTempo:return {0x3782,8};case S::DispatchTempo:return {0x378a,4};case S::ReadVelocity:return {0x378e,8};case S::PublishVelocity:return {0x3796,8};case S::ReadGate:return {0x379e,8};case S::PublishGate:return {0x37a6,8};case S::ClearModeIndex:return {0x37ae,2};case S::ReadModeIndex:return {0x37b0,8};case S::ReadModeTable:return {0x37b8,8};case S::ReadOldMode:return {0x37c0,8};case S::CompareModes:return {0x37c8,2};case S::AdmitModeChange:return {0x37ca,2};case S::PublishMode:return {0x37cc,8};case S::ObserveNewMono:return {0x37d4,2};case S::AdmitNewMono:return {0x37d6,2};case S::RetainPatchForMono:return {0x37d8,4};case S::RetainPartForMono:return {0x37dc,4};case S::ReleaseForMono:return {0x37e0,4};case S::ReleasePartForMono:return {0x37e4,4};case S::EnableMono:return {0x37e8,4};case S::ReleasePatchForMono:return {0x37ec,4};case S::SkipOldMono:return {0x37f0,2};case S::ObserveOldMono:return {0x37f2,2};case S::AdmitOldMono:return {0x37f4,2};case S::RetainPartForPoly:return {0x37f6,4};case S::RetainPatchForPoly:return {0x37fa,4};case S::ResetPoly:return {0x37fe,4};case S::ReleaseForPoly:return {0x3802,4};case S::ReleasePatchForPoly:return {0x3806,4};case S::ReleasePartForPoly:return {0x380a,4};case S::ReadPolicy:return {0x380e,8};case S::ReadOldPolicy:return {0x3816,8};case S::ObserveHoldPolicy:return {0x381e,2};case S::AdmitHoldPolicy:return {0x3820,2};case S::ReadPedal:return {0x3822,8};case S::MergePedal:return {0x382a,2};case S::PublishPolicy:return {0x382c,8};case S::CaptureOctaves:return {0x3834,2};case S::MaskOctaves:return {0x3836,2};case S::PublishOctaves:return {0x3838,8};case S::ComparePolicy:return {0x3840,2};case S::AdmitPolicyChange:return {0x3842,2};case S::CaptureFalling:return {0x3844,2};case S::InvertPolicy:return {0x3846,2};case S::IntersectChanges:return {0x3848,2};case S::ObservePedalRelease:return {0x384a,2};case S::AdmitPedalRelease:return {0x384c,2};case S::ReleaseHeld:return {0x384e,4};case S::RebindReleased:return {0x3852,4};case S::ReadReleasedMode:return {0x3856,8};case S::ObserveReleasedMono:return {0x385e,2};case S::AdmitReleasedMono:return {0x3860,2};case S::RetainPolicy:return {0x3862,2};case S::ResetReleasedMono:return {0x3864,4};case S::ReleaseReleasedMono:return {0x3868,4};case S::ReleasePolicy:return {0x386c,2};case S::ObserveEnableChange:return {0x386e,2};case S::AdmitEnableChange:return {0x3870,2};case S::ReleaseForEnable:return {0x3872,4};case S::RebindEnabled:return {0x3876,4};case S::ReadEnabledMode:return {0x387a,8};case S::ObserveEnabledMono:return {0x3882,2};case S::AdmitEnabledMono:return {0x3884,2};case S::ResetEnabledMono:return {0x3886,4};case S::BindGatePart:return {0x388a,4};case S::ReadGateMode:return {0x388e,8};case S::ObserveGateMono:return {0x3896,2};case S::AdmitGatePoly:return {0x3898,2};case S::ServiceGate:return {0x389a,4};case S::ExtendChord:return {0x389e,4};case S::BindClockPart:return {0x38a2,4};case S::ReadClock:return {0x38a6,8};case S::ReadPreviousClock:return {0x38ae,10};case S::PublishClock:return {0x38b8,10};case S::ElapsedClock:return {0x38c2,2};case S::ReadAccumulator:return {0x38c4,10};case S::AccumulateClock:return {0x38ce,2};case S::PublishAccumulator:return {0x38d0,10};case S::ReadRate:return {0x38da,10};case S::CaptureHighPhase:return {0x38e4,2};case S::MultiplyHighPhase:return {0x38e6,2};case S::ShiftHighProduct:return {0x38e8,2};case S::ClearLowProduct:return {0x38ea,2};case S::CaptureElapsedHigh:return {0x38ec,2};case S::MultiplyElapsedLow:return {0x38ee,2};case S::MultiplyRateLow:return {0x38f0,2};case S::AddElapsedProduct:return {0x38f2,2};case S::AddRateProduct:return {0x38f4,2};case S::CapturePhase:return {0x38f6,2};case S::ReadOldPhase:return {0x38f8,8};case S::PublishPhase:return {0x3900,8};case S::RebindControls:return {0x3908,4};case S::ReadCurrentPatch:return {0x390c,10};case S::RebindPatternPart:return {0x3916,4};case S::ClearPatternIndex:return {0x391a,2};case S::ReadPatternIndex:return {0x391c,8};case S::ReadOldPatternIndex:return {0x3924,8};case S::PublishPatternIndex:return {0x392c,8};case S::ComparePatternIndex:return {0x3934,2};case S::AdmitPatternChange:return {0x3936,2};case S::ScalePatternIndex:return {0x3938,2};case S::ReadPatternReference:return {0x393a,10};case S::ReadPatternHeader:return {0x3944,2};case S::PublishPatternLength:return {0x3946,8};case S::PublishPatternReference:return {0x394e,10};case S::ResetPattern:return {0x3958,4};case S::ReadPatternMode:return {0x395c,8};case S::ObservePatternMono:return {0x3964,2};case S::AdmitPatternPoly:return {0x3966,4};case S::RetainPatchForPattern:return {0x396a,4};case S::RetainPartForPattern:return {0x396e,4};case S::ReleasePatternVoices:return {0x3972,4};case S::ResetPatternVoices:return {0x3976,4};case S::ReleasePartForPattern:return {0x397a,4};case S::EnablePatternMono:return {0x397e,4};case S::ReleasePatchForPattern:return {0x3982,4};case S::ReadFinalMode:return {0x3986,8};case S::ObserveFinalMono:return {0x398e,2};case S::AdmitFinalMono:return {0x3990,4};case S::ReadCurrentPhase:return {0x3994,8};case S::ReadNextGate:return {0x399c,8};case S::SubtractGate:return {0x39a4,2};case S::AdmitGateSign:return {0x39a6,4};case S::AdmitGateCarry:return {0x39aa,4};case S::ReadFirstStep:return {0x39ae,4};case S::ObserveFirstEvent:return {0x39b2,2};case S::AdmitFirstEvent:return {0x39b4,2};case S::ReadRestPhase:return {0x39b6,8};case S::LocateRestGate:return {0x39be,2};case S::PublishRestGate:return {0x39c0,8};case S::SkipNote:return {0x39c8,2};case S::ReadEventVelocity:return {0x39ca,8};case S::CaptureFirstDelay:return {0x39d2,2};case S::ReadSecondStep:return {0x39d4,4};case S::CaptureSecondDelay:return {0x39d8,2};case S::ReadPeekStep:return {0x39da,2};case S::ObservePeekEvent:return {0x39dc,2};case S::AdmitPeekEvent:return {0x39de,2};case S::ReadThirdStep:return {0x39e0,4};case S::MergeThirdDelay:return {0x39e4,2};case S::ReadNotePhase:return {0x39e6,8};case S::AddFirstDelay:return {0x39ee,2};case S::AddSecondDelay:return {0x39f0,2};case S::PublishNoteGate:return {0x39f2,8};case S::ReadGateShape:return {0x39fa,8};case S::OffsetGateShape:return {0x3a02,2};case S::AdmitShortGate:return {0x3a04,2};case S::ScaleLongGate:return {0x3a06,2};case S::MultiplyLongGate:return {0x3a08,2};case S::MergeLongGate:return {0x3a0a,2};case S::CaptureLongGate:return {0x3a0c,2};case S::SkipShortGate:return {0x3a0e,2};case S::InvertShortGate:return {0x3a10,2};case S::ScaleShortGate:return {0x3a12,2};case S::AdmitShortScale:return {0x3a14,2};case S::SaturateShortScale:return {0x3a16,2};case S::MultiplyShortGate:return {0x3a18,2};case S::ClearShortLow:return {0x3a1a,2};case S::SubtractShortGate:return {0x3a1c,2};case S::PublishDuration:return {0x3a1e,8};case S::ReadNotePolicy:return {0x3a26,8};case S::ObserveNoteEnabled:return {0x3a2e,2};case S::AdmitNoteEnabled:return {0x3a30,2};case S::EmitNote:return {0x3a32,4};case S::Finish:return {0x3a36,2};case S::HandoffMono:return {0xb444,0};default:throw std::runtime_error("Invalid arp main stage");}}
uint8_t binding(uint32_t a,uint32_t base,uint32_t stride){if(a<base||a>=base+256*stride||(a-base)%stride)throw std::runtime_error("Invalid arp main binding");return uint8_t((a-base)/stride);}
}
void setArpMainPartialForTests(ArpMainPartialTest fn){partialTest=fn;}
bool ArpMainCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}if(pc==0x3770){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Arp main capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& t=call.task;
 if(t.awaitingChild()){switch(t.child()){case C::Controls:t.resumeControls(binding(uint32_t(v[0].er),0x174ae,20));break;case C::Part:t.resumePart(binding(uint32_t(v[0].er),0x160d2,334));break;case C::Step:t.resumeStep(uint16_t(v[4].r),uint32_t(v[1].er));break;case C::ReleaseHeld:t.resumeHeld(uint8_t(v[1].rh));break;default:t.resumeChild();break;}}
 const auto stage=t.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(t).length);const auto base=0x160d2 + unsigned(t.part())*334;uint32_t readData=0;
 auto publicationAddress=[&](S stage)->unsigned{switch(stage){case S::PublishVelocity:return base+329;case S::PublishGate:return base+330;case S::PublishMode:return base+324;case S::PublishPolicy:return base+327;case S::PublishOctaves:return base+325;case S::PublishPatternIndex:return base+318;case S::PublishPatternLength:return base+315;case S::PublishRestGate:return base+317;case S::ReadEventVelocity:return base+319;case S::PublishNoteGate:return base+317;default:throw std::runtime_error("Invalid arp main publication");}};
 auto timing=[&](S stage,bool word){if(word){auto at=(stage==S::ReadPatternHeader?t.pattern():t.stepReference())&0xffffff;return at>=0x400000||(at&1)?6u:3u;}const auto at=publicationAddress(stage);return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};
 const auto cost=t.nextClocks(t.publication()?timing(stage,false):3,t.dynamicWordRead()?timing(stage,true):3);
 auto read=[&](S stage)->uint32_t{switch(stage){case S::ReadPatch:return uint32_t(h.read32(0x174ae + unsigned(t.controlPart())*20));case S::ReadTempo:return uint8_t(h.read8(t.patch()+85));case S::ReadVelocity:return uint8_t(h.read8(t.patch()+167));case S::ReadGate:return uint8_t(h.read8(t.patch()+86));case S::ReadModeIndex:return uint8_t(h.read8(t.patch()+87));case S::ReadModeTable:return uint8_t(h.read8(62696 + t.modeIndex()));case S::ReadOldMode:return uint8_t(h.read8(base+324));case S::ReadPolicy:return uint8_t(h.read8(t.patch()+88));case S::ReadOldPolicy:return uint8_t(h.read8(base+327));case S::ReadPedal:return uint8_t(h.read8(base+310));case S::ReadReleasedMode:return uint8_t(h.read8(base+324));case S::ReadEnabledMode:return uint8_t(h.read8(base+324));case S::ReadGateMode:return uint8_t(h.read8(base+324));case S::ReadClock:return uint32_t(h.read32(0xffee78));case S::ReadPreviousClock:return uint32_t(h.read32(base));case S::ReadAccumulator:return uint32_t(h.read32(base+60));case S::ReadRate:return uint32_t(h.read32(base+4));case S::ReadOldPhase:return uint16_t(h.read16(base+304));case S::ReadCurrentPatch:return uint32_t(h.read32(0x174ae + unsigned(t.controlPart())*20));case S::ReadPatternIndex:return uint8_t(h.read8(t.patch()+149));case S::ReadOldPatternIndex:return uint8_t(h.read8(base+318));case S::ReadPatternReference:return uint32_t(h.read32(62704 + t.patternIndex()));case S::ReadPatternHeader:return uint16_t(h.read16(t.pattern()));case S::ReadPatternMode:return uint8_t(h.read8(base+324));case S::ReadFinalMode:return uint8_t(h.read8(base+324));case S::ReadCurrentPhase:return uint8_t(h.read8(base+304));case S::ReadNextGate:return uint8_t(h.read8(base+317));case S::ReadRestPhase:return uint8_t(h.read8(base+304));case S::ReadPeekStep:return uint16_t(h.read16(t.stepReference()));case S::ReadNotePhase:return uint8_t(h.read8(base+304));case S::ReadGateShape:return uint8_t(h.read8(base+330));case S::ReadNotePolicy:return uint8_t(h.read8(base+327));default:throw std::runtime_error("Invalid arp main read");}};
 auto capture=[&](S stage){return readData=read(stage);};auto write=[&](S stage,uint32_t data){switch(stage){case S::PublishVelocity:h.write8(int8_t(data),base+329);break;case S::PublishGate:h.write8(int8_t(data),base+330);break;case S::PublishMode:h.write8(int8_t(data),base+324);break;case S::PublishPolicy:h.write8(int8_t(data),base+327);break;case S::PublishOctaves:h.write8(int8_t(data),base+325);break;case S::PublishClock:h.write32(int32_t(data),base+0);break;case S::PublishAccumulator:h.write32(int32_t(data),base+60);break;case S::PublishPhase:h.write16(int16_t(data),base+304);break;case S::PublishPatternIndex:h.write8(int8_t(data),base+318);break;case S::PublishPatternLength:h.write8(int8_t(data),base+315);break;case S::PublishPatternReference:h.write32(int32_t(data),base+8);break;case S::PublishRestGate:h.write8(int8_t(data),base+317);break;case S::ReadEventVelocity:h.write8(int8_t(data),base+319);break;case S::PublishNoteGate:h.write8(int8_t(data),base+317);break;case S::PublishDuration:h.write16(int16_t(data),base+12);break;default:throw std::runtime_error("Invalid arp main write");}};
 if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("Arp main deadline mismatch");switch(stage){
case S::BindControls:{clocks.call(0xa66a);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReadPatch:{clocks.move(v[4].er,int32_t(readData));break;}
case S::BindPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReadTempo:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::DispatchTempo:{clocks.call(0x36d6);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReadVelocity:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::PublishVelocity:{value(h,v[1].rl);break;}
case S::ReadGate:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::PublishGate:{value(h,v[1].rl);break;}
case S::ClearModeIndex:{v[1].er=h.sub32(v[1].er,v[1].er);break;}
case S::ReadModeIndex:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ReadModeTable:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ReadOldMode:{clocks.move(v[1].rh,int8_t(readData));break;}
case S::CompareModes:{h.sub8(v[1].rl,v[1].rh);break;}
case S::AdmitModeChange:{;break;}
case S::PublishMode:{value(h,v[1].rl);break;}
case S::ObserveNewMono:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x6))==0);break;}
case S::AdmitNewMono:{;break;}
case S::RetainPatchForMono:{clocks.push(v[4].er);call.depth+=4;break;}
case S::RetainPartForMono:{clocks.push(v[0].er);call.depth+=4;break;}
case S::ReleaseForMono:{clocks.call(0x9374);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReleasePartForMono:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::EnableMono:{clocks.call(0xb3ec);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReleasePatchForMono:{clocks.pop(v[4].er);call.depth-=4;break;}
case S::SkipOldMono:{;break;}
case S::ObserveOldMono:{flag(h,4,(uint8_t(v[1].rh)&(1u<<0x6))==0);break;}
case S::AdmitOldMono:{;break;}
case S::RetainPartForPoly:{clocks.push(v[0].er);call.depth+=4;break;}
case S::RetainPatchForPoly:{clocks.push(v[4].er);call.depth+=4;break;}
case S::ResetPoly:{clocks.call(0xa8e6);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReleaseForPoly:{clocks.call(0x9374);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReleasePatchForPoly:{clocks.pop(v[4].er);call.depth-=4;break;}
case S::ReleasePartForPoly:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::ReadPolicy:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ReadOldPolicy:{clocks.move(v[1].rh,int8_t(readData));break;}
case S::ObserveHoldPolicy:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x2))==0);break;}
case S::AdmitHoldPolicy:{;break;}
case S::ReadPedal:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::MergePedal:{v[1].rl|=v[2].rl;value(h,v[1].rl);break;}
case S::PublishPolicy:{value(h,v[1].rl);break;}
case S::CaptureOctaves:{clocks.move(v[2].rl,v[1].rl);break;}
case S::MaskOctaves:{v[2].rl&=0x3;value(h,v[2].rl);break;}
case S::PublishOctaves:{value(h,v[2].rl);break;}
case S::ComparePolicy:{v[1].rh^=v[1].rl;value(h,v[1].rh);break;}
case S::AdmitPolicyChange:{;break;}
case S::CaptureFalling:{clocks.move(v[2].rl,v[1].rl);break;}
case S::InvertPolicy:{v[2].rl=~v[2].rl;value(h,v[2].rl);break;}
case S::IntersectChanges:{v[2].rl&=v[1].rh;value(h,v[2].rl);break;}
case S::ObservePedalRelease:{flag(h,4,(uint8_t(v[2].rl)&(1u<<0x4))==0);break;}
case S::AdmitPedalRelease:{;break;}
case S::ReleaseHeld:{clocks.call(0x3074);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::RebindReleased:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReadReleasedMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ObserveReleasedMono:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x6))==0);break;}
case S::AdmitReleasedMono:{;break;}
case S::RetainPolicy:{clocks.push(v[1].r);call.depth+=2;break;}
case S::ResetReleasedMono:{clocks.call(0xa8e6);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReleaseReleasedMono:{clocks.call(0x9374);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReleasePolicy:{clocks.pop(v[1].r);call.depth-=2;break;}
case S::ObserveEnableChange:{flag(h,4,(uint8_t(v[1].rh)&(1u<<0x2))==0);break;}
case S::AdmitEnableChange:{;break;}
case S::ReleaseForEnable:{clocks.call(0x9374);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::RebindEnabled:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReadEnabledMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ObserveEnabledMono:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x6))==0);break;}
case S::AdmitEnabledMono:{;break;}
case S::ResetEnabledMono:{clocks.call(0xa8e6);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::BindGatePart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReadGateMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ObserveGateMono:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x6))==0);break;}
case S::AdmitGatePoly:{;break;}
case S::ServiceGate:{clocks.call(0x3b30);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ExtendChord:{clocks.call(0x3bf0);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::BindClockPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReadClock:{clocks.move(v[4].er,int32_t(readData));break;}
case S::ReadPreviousClock:{clocks.move(v[1].er,int32_t(readData));break;}
case S::PublishClock:{value(h,v[4].er);break;}
case S::ElapsedClock:{v[4].er=h.sub32(v[4].er,v[1].er);break;}
case S::ReadAccumulator:{clocks.move(v[1].er,int32_t(readData));break;}
case S::AccumulateClock:{v[4].er=h.add32(v[4].er,v[1].er);break;}
case S::PublishAccumulator:{value(h,v[4].er);break;}
case S::ReadRate:{clocks.move(v[1].er,int32_t(readData));break;}
case S::CaptureHighPhase:{clocks.move(v[2].r,v[4].e);break;}
case S::MultiplyHighPhase:{v[2].er=uint16_t(v[2].er)*uint32_t(uint16_t(v[1].e));h.clockI(20);break;}
case S::ShiftHighProduct:{clocks.move(v[2].e,v[2].r);break;}
case S::ClearLowProduct:{v[2].r=h.sub16(v[2].r,v[2].r);break;}
case S::CaptureElapsedHigh:{clocks.move(v[3].r,v[4].e);break;}
case S::MultiplyElapsedLow:{v[4].er=uint16_t(v[4].er)*uint32_t(uint16_t(v[1].e));h.clockI(20);break;}
case S::MultiplyRateLow:{v[1].er=uint16_t(v[1].er)*uint32_t(uint16_t(v[3].r));h.clockI(20);break;}
case S::AddElapsedProduct:{v[2].er=h.add32(v[2].er,v[4].er);break;}
case S::AddRateProduct:{v[2].er=h.add32(v[2].er,v[1].er);break;}
case S::CapturePhase:{clocks.move(v[4].r,v[2].e);break;}
case S::ReadOldPhase:{clocks.move(v[1].r,int16_t(readData));break;}
case S::PublishPhase:{value(h,v[4].r);break;}
case S::RebindControls:{clocks.call(0xa66a);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReadCurrentPatch:{clocks.move(v[4].er,int32_t(readData));break;}
case S::RebindPatternPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ClearPatternIndex:{v[1].er=h.sub32(v[1].er,v[1].er);break;}
case S::ReadPatternIndex:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ReadOldPatternIndex:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::PublishPatternIndex:{value(h,v[1].rl);break;}
case S::ComparePatternIndex:{h.sub8(v[1].rl,v[2].rl);break;}
case S::AdmitPatternChange:{;break;}
case S::ScalePatternIndex:{clocks.shift(v[1].rl,2,true);break;}
case S::ReadPatternReference:{clocks.move(v[1].er,int32_t(readData));break;}
case S::ReadPatternHeader:{clocks.move(v[2].r,int16_t(readData));v[1].er+=2;h.clockI(2);break;}
case S::PublishPatternLength:{value(h,v[2].rh);break;}
case S::PublishPatternReference:{value(h,v[1].er);break;}
case S::ResetPattern:{clocks.call(0x3a38);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReadPatternMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ObservePatternMono:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x6))==0);break;}
case S::AdmitPatternPoly:{h.clockI(2);break;}
case S::RetainPatchForPattern:{clocks.push(v[4].er);call.depth+=4;break;}
case S::RetainPartForPattern:{clocks.push(v[0].er);call.depth+=4;break;}
case S::ReleasePatternVoices:{clocks.call(0x9374);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ResetPatternVoices:{clocks.call(0xa8e6);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReleasePartForPattern:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::EnablePatternMono:{clocks.call(0xb3ec);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ReleasePatchForPattern:{clocks.pop(v[4].er);call.depth-=4;break;}
case S::ReadFinalMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ObserveFinalMono:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x6))==0);break;}
case S::AdmitFinalMono:{h.clockI(2);break;}
case S::ReadCurrentPhase:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ReadNextGate:{clocks.move(v[1].rh,int8_t(readData));break;}
case S::SubtractGate:{v[1].rl=h.sub8(v[1].rl,v[1].rh);break;}
case S::AdmitGateSign:{h.clockI(2);break;}
case S::AdmitGateCarry:{h.clockI(2);break;}
case S::ReadFirstStep:{clocks.call(0x3a6e);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::ObserveFirstEvent:{clocks.move(v[4].rh,v[4].rh);break;}
case S::AdmitFirstEvent:{;break;}
case S::ReadRestPhase:{clocks.move(v[4].rh,int8_t(readData));break;}
case S::LocateRestGate:{v[4].rl=h.add8(v[4].rl,v[4].rh);break;}
case S::PublishRestGate:{value(h,v[4].rl);break;}
case S::SkipNote:{;break;}
case S::ReadEventVelocity:{value(h,v[4].rh);break;}
case S::CaptureFirstDelay:{clocks.move(v[2].rh,v[4].rl);break;}
case S::ReadSecondStep:{clocks.call(0x3a6e);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::CaptureSecondDelay:{clocks.move(v[2].rl,v[4].rl);break;}
case S::ReadPeekStep:{clocks.move(v[4].r,int16_t(readData));break;}
case S::ObservePeekEvent:{clocks.move(v[4].rh,v[4].rh);break;}
case S::AdmitPeekEvent:{;break;}
case S::ReadThirdStep:{clocks.call(0x3a6e);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::MergeThirdDelay:{v[2].rl=h.add8(v[2].rl,v[4].rl);break;}
case S::ReadNotePhase:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AddFirstDelay:{v[1].rl=h.add8(v[1].rl,v[2].rh);break;}
case S::AddSecondDelay:{v[1].rl=h.add8(v[1].rl,v[2].rl);break;}
case S::PublishNoteGate:{value(h,v[1].rl);break;}
case S::ReadGateShape:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::OffsetGateShape:{v[1].rl=h.add8(v[1].rl,0xffffffc0);break;}
case S::AdmitShortGate:{;break;}
case S::ScaleLongGate:{clocks.shift(v[1].rl,2,true);break;}
case S::MultiplyLongGate:{v[1].r=uint8_t(v[1].r)*uint32_t(uint8_t(v[2].rl));h.clockI(12);break;}
case S::MergeLongGate:{v[1].rh=h.add8(v[1].rh,v[2].rh);break;}
case S::CaptureLongGate:{clocks.move(v[2].r,v[1].r);break;}
case S::SkipShortGate:{;break;}
case S::InvertShortGate:{v[1].rl=h.sub8(0,v[1].rl);break;}
case S::ScaleShortGate:{clocks.shift(v[1].rl,2,true);break;}
case S::AdmitShortScale:{;break;}
case S::SaturateShortScale:{clocks.move(v[1].rl,0xfffffffc);break;}
case S::MultiplyShortGate:{v[1].r=uint8_t(v[1].r)*uint32_t(uint8_t(v[2].rh));h.clockI(12);break;}
case S::ClearShortLow:{clocks.move(v[2].rl,0x0);break;}
case S::SubtractShortGate:{v[2].r=h.sub16(v[2].r,v[1].r);break;}
case S::PublishDuration:{value(h,v[2].r);break;}
case S::ReadNotePolicy:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ObserveNoteEnabled:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x2))==0);break;}
case S::AdmitNoteEnabled:{;break;}
case S::EmitNote:{clocks.call(0x3ca4);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main dispatch timing");return true;break;}
case S::Finish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp main return timing");selected->reset();++completed;return true;break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("Arp main bus timing");h.pc=h.makepc(point(t).location);if(t.handoff()){selected->reset();++handedOff;}return true;
}}
