#include "ksArpEventCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace {ArpEventPartialTest partialTest=nullptr;using T=ArpEventTask;using K=T::Kind;using S=T::Stage;using C=T::Child;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){case S::AppendReadMode:return {0x3ab2,8};case S::AppendObservePending:return {0x3aba,2};case S::AppendAdmitPending:return {0x3abc,2};case S::AppendReadCount:return {0x3abe,8};case S::AppendAdmitCount:return {0x3ac6,2};case S::AppendReadState:return {0x3ac8,8};case S::AppendAdmitIdle:return {0x3ad0,2};case S::AppendCompareState:return {0x3ad2,2};case S::AppendAdmitReleased:return {0x3ad4,2};case S::AppendRetainNote:return {0x3ad6,2};case S::AppendRetainPatch:return {0x3ad8,2};case S::AppendReleaseGate:return {0x3ada,4};case S::AppendReleasePatch:return {0x3ade,2};case S::AppendReleaseNote:return {0x3ae0,2};case S::AppendReadPhase:return {0x3ae2,8};case S::AppendReadDuration:return {0x3aea,8};case S::AppendAddDuration:return {0x3af2,2};case S::AppendPublishExpiry:return {0x3af4,8};case S::AppendClearIndex:return {0x3afc,2};case S::AppendReadIndex:return {0x3afe,8};case S::AppendLocateNote:return {0x3b06,2};case S::AppendPublishNote:return {0x3b08,8};case S::AppendRecoverIndex:return {0x3b10,2};case S::AppendTickIndex:return {0x3b12,2};case S::AppendPublishCount:return {0x3b14,8};case S::AppendPublishMirror:return {0x3b1c,8};case S::AppendSetActive:return {0x3b24,2};case S::AppendPublishState:return {0x3b26,8};case S::AppendFinish:return {0x3b2e,2};case S::GateBindPart:return {0x3b30,4};case S::GateReadState:return {0x3b34,8};case S::GateAdmitIdle:return {0x3b3c,4};case S::GateCompareReleased:return {0x3b40,2};case S::GateAdmitReleased:return {0x3b42,2};case S::GateReadPhase:return {0x3b44,8};case S::GateReadExpiry:return {0x3b4c,8};case S::GateCompareExpiry:return {0x3b54,2};case S::GateAdmitSign:return {0x3b56,4};case S::GateAdmitCarry:return {0x3b5a,4};case S::GateSetReleased:return {0x3b5e,2};case S::GatePublishReleased:return {0x3b60,8};case S::GateScanHeld:return {0x3b68,4};case S::GateObserveHeld:return {0x3b6c,2};case S::GateAdmitHeld:return {0x3b6e,2};case S::GateReadVoices:return {0x3b70,8};case S::GateCompareVoices:return {0x3b78,2};case S::GateAdmitVoices:return {0x3b7a,2};case S::GateReadShape:return {0x3b7c,8};case S::GateCompareShape:return {0x3b84,2};case S::GateAdmitHold:return {0x3b86,2};case S::GateReadCount:return {0x3b88,8};case S::GateBindNotes:return {0x3b90,2};case S::GateLocateNotes:return {0x3b92,6};case S::GateReadNote:return {0x3b98,2};case S::GateRetainCursor:return {0x3b9a,4};case S::GateRetainCount:return {0x3b9e,2};case S::GateRetainPart:return {0x3ba0,4};case S::GateReleaseNote:return {0x3ba4,2};case S::GateReleasePart:return {0x3ba6,4};case S::GateReleaseCount:return {0x3baa,2};case S::GateReleaseCursor:return {0x3bac,4};case S::GateTickCount:return {0x3bb0,2};case S::GateAdmitCount:return {0x3bb2,2};case S::GatePublishIdle:return {0x3bb4,8};case S::GatePublishEmpty:return {0x3bbc,8};case S::GateFinish:return {0x3bc4,2};case S::OffReadMask:return {0x3bc6,6};case S::OffShiftMask:return {0x3bcc,2};case S::OffAdmitMask:return {0x3bce,2};case S::OffReadPolicy:return {0x3bd0,8};case S::OffMaskPolicy:return {0x3bd8,2};case S::OffAdmitMidi:return {0x3bda,2};case S::OffSetVelocity:return {0x3bdc,2};case S::OffSendMidi:return {0x3bde,4};case S::OffObservePolicy:return {0x3be2,2};case S::OffAdmitZero:return {0x3be4,2};case S::OffCompareInternal:return {0x3be6,2};case S::OffAdmitInternal:return {0x3be8,2};case S::OffReleaseVoice:return {0x3bea,4};case S::OffFinish:return {0x3bee,2};case S::ChordBindPart:return {0x3bf0,4};case S::ChordReadPolicy:return {0x3bf4,8};case S::ChordObserveEnable:return {0x3bfc,2};case S::ChordAdmitEnable:return {0x3bfe,4};case S::ChordReadMode:return {0x3c02,8};case S::ChordObserveChord:return {0x3c0a,2};case S::ChordAdmitChord:return {0x3c0c,4};case S::ChordReadQueue:return {0x3c10,8};case S::ChordAdmitQueue:return {0x3c18,4};case S::ChordReadHeld:return {0x3c1c,8};case S::ChordClearIndex:return {0x3c24,2};case S::ChordCaptureIndex:return {0x3c26,2};case S::ChordSubtractQueue:return {0x3c28,2};case S::ChordAdmitNewKeys:return {0x3c2a,2};case S::ChordPublishCount:return {0x3c2c,8};case S::ChordReadCurrentMode:return {0x3c34,8};case S::ChordMarkPending:return {0x3c3c,2};case S::ChordPublishPending:return {0x3c3e,8};case S::ChordReadOctave:return {0x3c46,8};case S::ChordReadOldOctave:return {0x3c4e,8};case S::ChordPublishOctave:return {0x3c56,8};case S::ChordScaleIndex:return {0x3c5e,2};case S::ChordBindKeys:return {0x3c60,2};case S::ChordLocateKeys:return {0x3c62,6};case S::ChordPreviousKey:return {0x3c68,2};case S::ChordReadKey:return {0x3c6a,2};case S::ChordRetainPart:return {0x3c6c,4};case S::ChordRetainState:return {0x3c70,2};case S::ChordRetainCursor:return {0x3c72,4};case S::ChordEmitKey:return {0x3c76,4};case S::ChordReleaseCursor:return {0x3c7a,4};case S::ChordReleaseState:return {0x3c7e,2};case S::ChordReleasePart:return {0x3c80,4};case S::ChordTickCount:return {0x3c84,2};case S::ChordAdmitCount:return {0x3c86,2};case S::ChordRestoreOctave:return {0x3c88,8};case S::ChordReadFinalMode:return {0x3c90,8};case S::ChordClearPending:return {0x3c98,2};case S::ChordPublishFinalMode:return {0x3c9a,8};case S::ChordFinish:return {0x3ca2,2};case S::EmitReadOverride:return {0x3f18,8};case S::EmitAdmitOverride:return {0x3f20,2};case S::EmitReadVelocity:return {0x3f22,8};case S::EmitPublishLast:return {0x3f2a,8};case S::EmitRetainNote:return {0x3f32,2};case S::EmitReadOctave:return {0x3f34,8};case S::EmitSetOctaveScale:return {0x3f3c,2};case S::EmitMultiplyOctave:return {0x3f3e,2};case S::EmitTranspose:return {0x3f40,2};case S::EmitAdmitPitch:return {0x3f42,2};case S::EmitAppendNote:return {0x3f44,4};case S::EmitReadMask:return {0x3f48,6};case S::EmitShiftMask:return {0x3f4e,2};case S::EmitAdmitMask:return {0x3f50,2};case S::EmitReadPolicy:return {0x3f52,8};case S::EmitMaskPolicy:return {0x3f5a,2};case S::EmitAdmitMidi:return {0x3f5c,2};case S::EmitRetainPatch:return {0x3f5e,2};case S::EmitSendMidi:return {0x3f60,4};case S::EmitReleasePatch:return {0x3f64,2};case S::EmitObservePolicy:return {0x3f66,2};case S::EmitAdmitZero:return {0x3f68,2};case S::EmitCompareInternal:return {0x3f6a,2};case S::EmitAdmitInternal:return {0x3f6c,2};case S::EmitPlayVoice:return {0x3f6e,4};case S::EmitSetWork:return {0x3f72,2};case S::EmitPublishWork:return {0x3f74,6};case S::EmitReleaseNote:return {0x3f7a,2};case S::EmitObserveNote:return {0x3f7c,2};case S::EmitFinish:return {0x3f7e,2};case S::ResetScanHeld:return {0x3f80,4};case S::ResetAdmitEmpty:return {0x3f84,2};case S::ResetReadFirstPolicy:return {0x3f86,8};case S::ResetObserveLegato:return {0x3f8e,2};case S::ResetAdmitLegato:return {0x3f90,2};case S::ResetPhase:return {0x3f92,4};case S::ResetClearCandidate:return {0x3f96,2};case S::ResetClearOctave:return {0x3f98,2};case S::ResetReadMode:return {0x3f9a,8};case S::ResetObserveDirection:return {0x3fa2,2};case S::ResetAdmitDirection:return {0x3fa4,2};case S::ResetDescendingKey:return {0x3fa6,2};case S::ResetDescendingDirection:return {0x3fa8,2};case S::ResetReadOctaves:return {0x3faa,8};case S::ResetReadPolicy:return {0x3fb2,8};case S::ResetObserveHold:return {0x3fba,2};case S::ResetAdmitHold:return {0x3fbc,2};case S::ResetObservePedal:return {0x3fbe,2};case S::ResetAdmitPedal:return {0x3fc0,2};case S::ResetScanCurrentHeld:return {0x3fc2,4};case S::ResetObserveHeld:return {0x3fc6,2};case S::ResetAdmitHeld:return {0x3fc8,2};case S::ResetPublishLast:return {0x3fca,8};case S::ResetPublishDirection:return {0x3fd2,8};case S::ResetPublishOctave:return {0x3fda,8};case S::ResetClearIndex:return {0x3fe2,2};case S::ResetPublishIndex:return {0x3fe4,8};case S::ResetReleaseHeld:return {0x3fec,4};case S::ResetSetSentinel:return {0x3ff0,2};case S::ResetPublishSentinel:return {0x3ff2,8};case S::ResetDispatchRefresh:return {0x3ffa,4};case S::ResetFinish:return {0x3ffe,2};default:throw std::runtime_error("Invalid arp event stage");}}uint8_t binding(uint32_t a){if(a<0x160d2||a>=0x160d2 + 256*334||(a-0x160d2)%334)throw std::runtime_error("Invalid arp event part binding");return uint8_t((a-0x160d2)/334);}}
void setArpEventPartialForTests(ArpEventPartialTest fn){partialTest=fn;}
bool ArpEventCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 const bool root=pc==0x3ab2||pc==0x3b30||pc==0x3bc6||pc==0x3bf0||pc==0x3f18||pc==0x3f80||(pc==0x3b68&&(!selected||point((*selected)->task).location!=pc));
 if(root&&pc!=0x3b30&&pc!=0x3bf0&&partialTest){const auto a=uint32_t(v[0].er);if(a<0x160d2||a>=0x160d2 + 256*334||(a-0x160d2)%334){if(partialTest(h)){++partialEntries;return true;}}}
 if(root){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Arp event capacity exceeded");const auto kind=pc==0x3ab2?K::Append:pc==0x3b30?K::Gate:pc==0x3b68?K::DirectGate:pc==0x3bc6?K::Off:pc==0x3bf0?K::Chord:pc==0x3f18?K::Emit:K::Reset;selected->emplace(kind,(kind==K::Gate||kind==K::Chord)?0:binding(uint32_t(v[0].er)),uint8_t(v[5].rl),uint8_t(v[5].rh),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& t=call.task;
 if(t.awaitingChild()){switch(t.child()){case C::Part:case C::ReleaseGate:case C::Append:case C::ReleaseHeld:t.resumePart(binding(uint32_t(v[0].er)));break;case C::Held:t.resumeHeld(binding(uint32_t(v[0].er)),v[1].r!=0);break;case C::MidiOff:case C::MidiOn:t.resumeMidi(uint8_t(v[1].rl));break;default:t.resumeChild();break;}}
 const auto stage=t.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(t).length);const auto base=0x160d2 + unsigned(t.part())*334;uint32_t readData=0;
 auto publicationAddress=[&](S stage)->unsigned{switch(stage){case S::AppendPublishNote:return base+16+t.queueCount();case S::AppendPublishCount:return base+320;case S::AppendPublishMirror:return base+312;case S::AppendPublishState:return base+311;case S::GatePublishReleased:return base+311;case S::GatePublishIdle:return base+311;case S::GatePublishEmpty:return base+312;case S::ChordPublishCount:return base+320;case S::ChordPublishPending:return base+324;case S::ChordPublishOctave:return base+326;case S::ChordRestoreOctave:return base+326;case S::ChordPublishFinalMode:return base+324;case S::EmitPublishLast:return base+322;case S::EmitPublishWork:return 0x160d1;case S::ResetPublishLast:return base+322;case S::ResetPublishDirection:return base+323;case S::ResetPublishOctave:return base+326;case S::ResetPublishIndex:return base+308;case S::ResetPublishSentinel:return base+314;default:throw std::runtime_error("Invalid arp event publication");}};auto timing=[&](S stage){const auto at=publicationAddress(stage);return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=t.nextClocks(t.publication()?timing(stage):3);
 auto read=[&](S stage)->uint32_t{switch(stage){case S::AppendReadMode:return uint8_t(h.read8(base+324));case S::AppendReadCount:return uint8_t(h.read8(base+320));case S::AppendReadState:return uint8_t(h.read8(base+311));case S::AppendReadPhase:return uint16_t(h.read16(base+304));case S::AppendReadDuration:return uint16_t(h.read16(base+12));case S::AppendReadIndex:return uint8_t(h.read8(base+320));case S::GateReadState:return uint8_t(h.read8(base+311));case S::GateReadPhase:return uint16_t(h.read16(base+304));case S::GateReadExpiry:return uint16_t(h.read16(base+14));case S::GateReadVoices:return uint8_t(h.read8(base+313));case S::GateReadShape:return uint8_t(h.read8(base+330));case S::GateReadCount:return uint8_t(h.read8(base+312));case S::GateReadNote:return uint8_t(h.read8(base+16+t.cursor()));case S::OffReadMask:return uint8_t(h.read8(0x160d0));case S::OffReadPolicy:return uint8_t(h.read8(base+327));case S::ChordReadPolicy:return uint8_t(h.read8(base+327));case S::ChordReadMode:return uint8_t(h.read8(base+324));case S::ChordReadQueue:return uint8_t(h.read8(base+312));case S::ChordReadHeld:return uint8_t(h.read8(base+307));case S::ChordReadCurrentMode:return uint8_t(h.read8(base+324));case S::ChordReadOctave:return uint8_t(h.read8(base+321));case S::ChordReadOldOctave:return uint8_t(h.read8(base+326));case S::ChordReadKey:return uint16_t(h.read16(uint32_t(base+int32_t(t.listOffset()))));case S::ChordReadFinalMode:return uint8_t(h.read8(base+324));case S::EmitReadOverride:return uint8_t(h.read8(base+329));case S::EmitReadVelocity:return uint8_t(h.read8(base+319));case S::EmitReadOctave:return uint8_t(h.read8(base+326));case S::EmitReadMask:return uint8_t(h.read8(0x160d0));case S::EmitReadPolicy:return uint8_t(h.read8(base+327));case S::ResetReadFirstPolicy:return uint8_t(h.read8(base+327));case S::ResetReadMode:return uint8_t(h.read8(base+324));case S::ResetReadOctaves:return uint8_t(h.read8(base+325));case S::ResetReadPolicy:return uint8_t(h.read8(base+327));default:throw std::runtime_error("Invalid arp event read");}};auto capture=[&](S stage){return readData=read(stage);};auto write=[&](S stage,uint32_t data){switch(stage){case S::AppendPublishExpiry:h.write16(int16_t(data),base+14);break;case S::AppendPublishNote:h.write8(int8_t(data),base+16+t.queueCount());break;case S::AppendPublishCount:h.write8(int8_t(data),base+320);break;case S::AppendPublishMirror:h.write8(int8_t(data),base+312);break;case S::AppendPublishState:h.write8(int8_t(data),base+311);break;case S::GatePublishReleased:h.write8(int8_t(data),base+311);break;case S::GatePublishIdle:h.write8(int8_t(data),base+311);break;case S::GatePublishEmpty:h.write8(int8_t(data),base+312);break;case S::ChordPublishCount:h.write8(int8_t(data),base+320);break;case S::ChordPublishPending:h.write8(int8_t(data),base+324);break;case S::ChordPublishOctave:h.write8(int8_t(data),base+326);break;case S::ChordRestoreOctave:h.write8(int8_t(data),base+326);break;case S::ChordPublishFinalMode:h.write8(int8_t(data),base+324);break;case S::EmitPublishLast:h.write8(int8_t(data),base+322);break;case S::EmitPublishWork:h.write8(int8_t(data),0x160d1);break;case S::ResetPublishLast:h.write8(int8_t(data),base+322);break;case S::ResetPublishDirection:h.write8(int8_t(data),base+323);break;case S::ResetPublishOctave:h.write8(int8_t(data),base+326);break;case S::ResetPublishIndex:h.write8(int8_t(data),base+308);break;case S::ResetPublishSentinel:h.write8(int8_t(data),base+314);break;default:throw std::runtime_error("Invalid arp event write");}};
 if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("Arp event deadline mismatch");switch(stage){
case S::AppendReadMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AppendObservePending:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x7))==0);break;}
case S::AppendAdmitPending:{;break;}
case S::AppendReadCount:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AppendAdmitCount:{;break;}
case S::AppendReadState:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AppendAdmitIdle:{;break;}
case S::AppendCompareState:{h.sub8(v[1].rl,0x2);break;}
case S::AppendAdmitReleased:{;break;}
case S::AppendRetainNote:{clocks.push(v[5].r);call.depth+=2;break;}
case S::AppendRetainPatch:{clocks.push(v[6].r);call.depth+=2;break;}
case S::AppendReleaseGate:{clocks.call(0x3b68);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::AppendReleasePatch:{clocks.pop(v[6].r);call.depth-=2;break;}
case S::AppendReleaseNote:{clocks.pop(v[5].r);call.depth-=2;break;}
case S::AppendReadPhase:{clocks.move(v[1].r,int16_t(readData));break;}
case S::AppendReadDuration:{clocks.move(v[1].e,int16_t(readData));break;}
case S::AppendAddDuration:{v[1].r=h.add16(v[1].r,v[1].e);break;}
case S::AppendPublishExpiry:{value(h,v[1].r);break;}
case S::AppendClearIndex:{v[1].er=h.sub32(v[1].er,v[1].er);break;}
case S::AppendReadIndex:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AppendLocateNote:{v[1].er=h.add32(v[1].er,v[0].er);break;}
case S::AppendPublishNote:{value(h,v[5].rl);break;}
case S::AppendRecoverIndex:{v[1].er=h.sub32(v[1].er,v[0].er);break;}
case S::AppendTickIndex:{clocks.inc(v[1].rl,1);break;}
case S::AppendPublishCount:{value(h,v[1].rl);break;}
case S::AppendPublishMirror:{value(h,v[1].rl);break;}
case S::AppendSetActive:{clocks.move(v[1].rl,0x1);break;}
case S::AppendPublishState:{value(h,v[1].rl);break;}
case S::AppendFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event return timing");selected->reset();++completed;return true;break;}
case S::GateBindPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::GateReadState:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::GateAdmitIdle:{h.clockI(2);break;}
case S::GateCompareReleased:{h.sub8(v[1].rl,0x2);break;}
case S::GateAdmitReleased:{;break;}
case S::GateReadPhase:{clocks.move(v[1].r,int16_t(readData));break;}
case S::GateReadExpiry:{clocks.move(v[1].e,int16_t(readData));break;}
case S::GateCompareExpiry:{h.sub16(v[1].r,v[1].e);break;}
case S::GateAdmitSign:{h.clockI(2);break;}
case S::GateAdmitCarry:{h.clockI(2);break;}
case S::GateSetReleased:{clocks.move(v[1].rl,0x2);break;}
case S::GatePublishReleased:{value(h,v[1].rl);break;}
case S::GateScanHeld:{clocks.call(0x30fe);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::GateObserveHeld:{clocks.move(v[1].r,v[1].r);break;}
case S::GateAdmitHeld:{;break;}
case S::GateReadVoices:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::GateCompareVoices:{h.sub8(v[1].rl,0x1);break;}
case S::GateAdmitVoices:{;break;}
case S::GateReadShape:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::GateCompareShape:{h.sub8(v[1].rl,0x7f);break;}
case S::GateAdmitHold:{;break;}
case S::GateReadCount:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::GateBindNotes:{clocks.move(v[1].er,v[0].er);break;}
case S::GateLocateNotes:{v[1].er=h.add32(v[1].er,0x10);break;}
case S::GateReadNote:{clocks.move(v[5].rl,int8_t(readData));v[1].er+=1;h.clockI(2);break;}
case S::GateRetainCursor:{clocks.push(v[1].er);call.depth+=4;break;}
case S::GateRetainCount:{clocks.push(v[2].r);call.depth+=2;break;}
case S::GateRetainPart:{clocks.push(v[0].er);call.depth+=4;break;}
case S::GateReleaseNote:{clocks.call(0x3bc6);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::GateReleasePart:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::GateReleaseCount:{clocks.pop(v[2].r);call.depth-=2;break;}
case S::GateReleaseCursor:{clocks.pop(v[1].er);call.depth-=4;break;}
case S::GateTickCount:{clocks.inc(v[2].rl,-1);break;}
case S::GateAdmitCount:{;break;}
case S::GatePublishIdle:{value(h,v[2].rl);break;}
case S::GatePublishEmpty:{value(h,v[2].rl);break;}
case S::GateFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event return timing");selected->reset();++completed;return true;break;}
case S::OffReadMask:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::OffShiftMask:{clocks.shift(v[1].rl,1,false);break;}
case S::OffAdmitMask:{;break;}
case S::OffReadPolicy:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::OffMaskPolicy:{v[1].rl&=0x60;value(h,v[1].rl);break;}
case S::OffAdmitMidi:{;break;}
case S::OffSetVelocity:{clocks.move(v[5].rh,0x40);break;}
case S::OffSendMidi:{clocks.call(0x2a58);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::OffObservePolicy:{clocks.move(v[1].rl,v[1].rl);break;}
case S::OffAdmitZero:{;break;}
case S::OffCompareInternal:{h.sub8(v[1].rl,0x40);break;}
case S::OffAdmitInternal:{;break;}
case S::OffReleaseVoice:{clocks.call(0x92c0);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::OffFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event return timing");selected->reset();++completed;return true;break;}
case S::ChordBindPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::ChordReadPolicy:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ChordObserveEnable:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x2))==0);break;}
case S::ChordAdmitEnable:{h.clockI(2);break;}
case S::ChordReadMode:{clocks.move(v[6].rl,int8_t(readData));break;}
case S::ChordObserveChord:{flag(h,4,(uint8_t(v[6].rl)&(1u<<0x5))==0);break;}
case S::ChordAdmitChord:{h.clockI(2);break;}
case S::ChordReadQueue:{clocks.move(v[1].rh,int8_t(readData));break;}
case S::ChordAdmitQueue:{h.clockI(2);break;}
case S::ChordReadHeld:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ChordClearIndex:{v[2].er=h.sub32(v[2].er,v[2].er);break;}
case S::ChordCaptureIndex:{clocks.move(v[2].rl,v[1].rl);break;}
case S::ChordSubtractQueue:{v[1].rl=h.sub8(v[1].rl,v[1].rh);break;}
case S::ChordAdmitNewKeys:{;break;}
case S::ChordPublishCount:{value(h,v[1].rh);break;}
case S::ChordReadCurrentMode:{clocks.move(v[1].rh,int8_t(readData));break;}
case S::ChordMarkPending:{v[1].rh|=(1u<<0x7);break;}
case S::ChordPublishPending:{value(h,v[1].rh);break;}
case S::ChordReadOctave:{clocks.move(v[3].rl,int8_t(readData));break;}
case S::ChordReadOldOctave:{clocks.move(v[1].rh,int8_t(readData));break;}
case S::ChordPublishOctave:{value(h,v[3].rl);break;}
case S::ChordScaleIndex:{clocks.shift(v[2].rl,1,true);break;}
case S::ChordBindKeys:{v[2].er=h.add32(v[2].er,v[0].er);break;}
case S::ChordLocateKeys:{v[2].er=h.add32(v[2].er,0xf0);break;}
case S::ChordPreviousKey:{v[2].er=int32_t(uint32_t(v[2].er)-0x2);break;}
case S::ChordReadKey:{clocks.move(v[5].r,int16_t(readData));break;}
case S::ChordRetainPart:{clocks.push(v[0].er);call.depth+=4;break;}
case S::ChordRetainState:{clocks.push(v[1].r);call.depth+=2;break;}
case S::ChordRetainCursor:{clocks.push(v[2].er);call.depth+=4;break;}
case S::ChordEmitKey:{clocks.call(0x3f18);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::ChordReleaseCursor:{clocks.pop(v[2].er);call.depth-=4;break;}
case S::ChordReleaseState:{clocks.pop(v[1].r);call.depth-=2;break;}
case S::ChordReleasePart:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::ChordTickCount:{clocks.inc(v[1].rl,-1);break;}
case S::ChordAdmitCount:{;break;}
case S::ChordRestoreOctave:{value(h,v[1].rh);break;}
case S::ChordReadFinalMode:{clocks.move(v[1].rh,int8_t(readData));break;}
case S::ChordClearPending:{v[1].rh&=~(1u<<0x7);break;}
case S::ChordPublishFinalMode:{value(h,v[1].rh);break;}
case S::ChordFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event return timing");selected->reset();++completed;return true;break;}
case S::EmitReadOverride:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::EmitAdmitOverride:{;break;}
case S::EmitReadVelocity:{clocks.move(v[5].rh,int8_t(readData));break;}
case S::EmitPublishLast:{value(h,v[5].rl);break;}
case S::EmitRetainNote:{clocks.push(v[5].r);call.depth+=2;break;}
case S::EmitReadOctave:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::EmitSetOctaveScale:{clocks.move(v[1].rh,0xc);break;}
case S::EmitMultiplyOctave:{v[1].r=uint8_t(v[1].r)*uint32_t(uint8_t(v[1].rh));h.clockI(12);break;}
case S::EmitTranspose:{v[5].rl=h.add8(v[5].rl,v[1].rl);break;}
case S::EmitAdmitPitch:{;break;}
case S::EmitAppendNote:{clocks.call(0x3ab2);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::EmitReadMask:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::EmitShiftMask:{clocks.shift(v[1].rl,1,false);break;}
case S::EmitAdmitMask:{;break;}
case S::EmitReadPolicy:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::EmitMaskPolicy:{v[1].rl&=0x60;value(h,v[1].rl);break;}
case S::EmitAdmitMidi:{;break;}
case S::EmitRetainPatch:{clocks.push(v[6].r);call.depth+=2;break;}
case S::EmitSendMidi:{clocks.call(0x2a60);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::EmitReleasePatch:{clocks.pop(v[6].r);call.depth-=2;break;}
case S::EmitObservePolicy:{clocks.move(v[1].rl,v[1].rl);break;}
case S::EmitAdmitZero:{;break;}
case S::EmitCompareInternal:{h.sub8(v[1].rl,0x40);break;}
case S::EmitAdmitInternal:{;break;}
case S::EmitPlayVoice:{clocks.call(0x3642);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::EmitSetWork:{clocks.move(v[1].rl,0x1);break;}
case S::EmitPublishWork:{value(h,v[1].rl);break;}
case S::EmitReleaseNote:{clocks.pop(v[5].r);call.depth-=2;break;}
case S::EmitObserveNote:{clocks.move(v[5].rl,v[5].rl);break;}
case S::EmitFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event return timing");selected->reset();++completed;return true;break;}
case S::ResetScanHeld:{clocks.call(0x30fe);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::ResetAdmitEmpty:{;break;}
case S::ResetReadFirstPolicy:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ResetObserveLegato:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x3))==0);break;}
case S::ResetAdmitLegato:{;break;}
case S::ResetPhase:{clocks.call(0x3a38);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::ResetClearCandidate:{v[4].r=h.sub16(v[4].r,v[4].r);break;}
case S::ResetClearOctave:{clocks.move(v[3].rl,0x0);break;}
case S::ResetReadMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ResetObserveDirection:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x0))==0);break;}
case S::ResetAdmitDirection:{;break;}
case S::ResetDescendingKey:{clocks.move(v[4].rl,0x7f);break;}
case S::ResetDescendingDirection:{clocks.move(v[4].rh,0x1);break;}
case S::ResetReadOctaves:{clocks.move(v[3].rl,int8_t(readData));break;}
case S::ResetReadPolicy:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ResetObserveHold:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x3))==0);break;}
case S::ResetAdmitHold:{;break;}
case S::ResetObservePedal:{flag(h,4,(uint8_t(v[1].rl)&(1u<<0x4))==0);break;}
case S::ResetAdmitPedal:{;break;}
case S::ResetScanCurrentHeld:{clocks.call(0x30fe);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::ResetObserveHeld:{clocks.move(v[1].r,v[1].r);break;}
case S::ResetAdmitHeld:{;break;}
case S::ResetPublishLast:{value(h,v[4].rl);break;}
case S::ResetPublishDirection:{value(h,v[4].rh);break;}
case S::ResetPublishOctave:{value(h,v[3].rl);break;}
case S::ResetClearIndex:{clocks.move(v[1].rl,0x0);break;}
case S::ResetPublishIndex:{value(h,v[1].rl);break;}
case S::ResetReleaseHeld:{clocks.call(0x3074);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::ResetSetSentinel:{clocks.move(v[1].rl,0xffffffff);break;}
case S::ResetPublishSentinel:{value(h,v[1].rl);break;}
case S::ResetDispatchRefresh:{clocks.call(0xa008);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event dispatch timing");return true;break;}
case S::ResetFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Arp event return timing");selected->reset();++completed;return true;break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("Arp event bus timing");h.pc=h.makepc(point(t).location);return true;}}
