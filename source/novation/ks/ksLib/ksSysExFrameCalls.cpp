#include "ksSysExFrameCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace{SysExFramePartialTest partialTest=nullptr;using T=SysExFrameTask;using K=T::Kind;using S=T::Stage;using C=T::Child;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){case S::OutputByte:return {0xa924,4};case S::OutputCharacter:return {0xa928,4};case S::OutputText:return {0xa92c,4};case S::OutputNumber:return {0xa930,4};case S::Noop:return {0xa934,2};case S::BeginReadState:return {0xa936,6};case S::BeginAdmitState:return {0xa93c,2};case S::BeginRestart:return {0xa93e,2};case S::BeginDispatch:return {0xa940,2};case S::BeginFinish:return {0xa942,2};case S::RestartReadState:return {0xa944,6};case S::RestartTickState:return {0xa94a,2};case S::RestartPublishState:return {0xa94c,6};case S::RestartClearCount:return {0xa952,2};case S::RestartPublishCount:return {0xa954,6};case S::RestartBindPayload:return {0xa95a,6};case S::RestartPublishCursor:return {0xa960,8};case S::RestartFinish:return {0xa968,2};case S::DispatchReadState:return {0xa96a,6};case S::DispatchCompareHeader:return {0xa970,2};case S::DispatchAdmitHeader:return {0xa972,2};case S::DispatchBindModel:return {0xa974,6};case S::DispatchReadModel:return {0xa97a,4};case S::DispatchCompareCommand:return {0xa97e,2};case S::DispatchAdmitCommand:return {0xa980,2};case S::DispatchReadCommand:return {0xa982,6};case S::DispatchCompareModel:return {0xa988,2};case S::DispatchAdmitModel:return {0xa98a,4};case S::DispatchComparePatchRequest:return {0xa98e,2};case S::DispatchAdmitPatchRequest:return {0xa990,4};case S::DispatchCompareMultiRequest:return {0xa994,2};case S::DispatchAdmitMultiRequest:return {0xa996,4};case S::DispatchSkipCommand:return {0xa99a,2};case S::DispatchComparePayload:return {0xa99c,2};case S::DispatchAdmitPayload:return {0xa99e,2};case S::DispatchPayload:return {0xa9a0,4};case S::DispatchFinish:return {0xa9a4,2};case S::HeaderReadInput:return {0xa9a6,2};case S::HeaderAppend:return {0xa9a8,2};case S::HeaderCompareCount:return {0xa9aa,4};case S::HeaderAdmitCount:return {0xa9ae,2};case S::HeaderFinish:return {0xa9b0,2};case S::AppendReadCursor:return {0xa9b2,8};case S::AppendPublishInput:return {0xa9ba,2};case S::AppendTickCursor:return {0xa9bc,2};case S::AppendPublishCursor:return {0xa9be,8};case S::AppendReadCount:return {0xa9c6,6};case S::AppendTickCount:return {0xa9cc,2};case S::AppendPublishCount:return {0xa9ce,6};case S::AppendFinish:return {0xa9d4,2};case S::ValidateBindHeader:return {0xa9d6,6};case S::ValidateBindModel:return {0xa9dc,6};case S::ValidateSetCount:return {0xa9e2,4};case S::ValidateReadHeader:return {0xa9e6,2};case S::ValidateReadModel:return {0xa9e8,2};case S::ValidateCompareByte:return {0xa9ea,2};case S::ValidateAdmitByte:return {0xa9ec,2};case S::ValidateTickCount:return {0xa9ee,2};case S::ValidateAdmitCount:return {0xa9f0,2};case S::ValidateAdmitMagic:return {0xa9f2,2};case S::ValidateReadCommand:return {0xa9f4,2};case S::ValidatePublishCommand:return {0xa9f6,6};case S::ValidateReadModelCommand:return {0xa9fc,2};case S::ValidateComparePatch:return {0xa9fe,2};case S::ValidateAdmitPatch:return {0xaa00,2};case S::ValidateCompareMulti:return {0xaa02,2};case S::ValidateAdmitMulti:return {0xaa04,2};case S::ValidateCompareModel:return {0xaa06,2};case S::ValidateAdmitModel:return {0xaa08,2};case S::ValidateReadDevice:return {0xaa0a,2};case S::ValidateCompareBroadcast:return {0xaa0c,2};case S::ValidateAdmitBroadcast:return {0xaa0e,2};case S::ValidateBindPatch:return {0xaa10,6};case S::ValidateReadDeviceId:return {0xaa16,8};case S::ValidateCompareDevice:return {0xaa1e,2};case S::ValidateAdmitDevice:return {0xaa20,2};case S::ValidateRestart:return {0xaa22,4};case S::ValidateBindCursor:return {0xaa26,6};case S::ValidatePublishCursor:return {0xaa2c,8};case S::ValidateFinish:return {0xaa34,2};case S::CancelClear:return {0xaa36,2};case S::CancelPublishState:return {0xaa38,6};case S::CancelPublishRemaining:return {0xaa3e,6};case S::CancelFinish:return {0xaa44,2};case S::BankReadInput:return {0xaa46,2};case S::BankAppend:return {0xaa48,4};case S::BankCompareCount:return {0xaa4c,4};case S::BankAdmitCount:return {0xaa50,2};case S::BankFinish:return {0xaa52,2};case S::BankReadType:return {0xaa54,6};case S::BankSmallSize:return {0xaa5a,4};case S::BankCompareSmall:return {0xaa5e,2};case S::BankAdmitSmall:return {0xaa60,2};case S::BankLargeSize:return {0xaa62,4};case S::BankCompareLarge:return {0xaa66,2};case S::BankAdmitLarge:return {0xaa68,2};case S::BankCancel:return {0xaa6a,2};case S::LegacyReadType:return {0xaa6c,6};case S::LegacyComparePatch:return {0xaa72,2};case S::LegacyAdmitPatch:return {0xaa74,4};case S::LegacyComparePatchBank:return {0xaa78,2};case S::LegacyAdmitPatchBank:return {0xaa7a,4};case S::LegacyCompareMulti:return {0xaa7e,2};case S::LegacyAdmitMulti:return {0xaa80,4};case S::LegacyFinish:return {0xaa84,2};case S::ModelReadInput:return {0xaa86,2};case S::ModelAppend:return {0xaa88,4};case S::ModelCompareCount:return {0xaa8c,4};case S::ModelAdmitCount:return {0xaa90,2};case S::ModelFinish:return {0xaa92,2};case S::ModelReadType:return {0xaa94,6};case S::ModelCompareRequest:return {0xaa9a,2};case S::ModelAdmitRequest:return {0xaa9c,4};case S::ModelSmallSize:return {0xaaa0,4};case S::ModelCompareSmall:return {0xaaa4,2};case S::ModelAdmitSmall:return {0xaaa6,2};case S::ModelCompareGlobal:return {0xaaa8,2};case S::ModelAdmitGlobal:return {0xaaaa,2};case S::ModelLargeSize:return {0xaaac,4};case S::ModelCompareLarge:return {0xaab0,2};case S::ModelAdmitLarge:return {0xaab2,2};case S::ModelCancel:return {0xaab4,4};case S::PublishSize:return {0xaab8,6};case S::SizeRestart:return {0xaabe,4};case S::SizeFinish:return {0xaac2,2};case S::PayloadReadInput:return {0xaac4,2};case S::PayloadAppend:return {0xaac6,4};case S::PayloadReadSize:return {0xaaca,6};case S::PayloadCompareSize:return {0xaad0,2};case S::PayloadAdmitSize:return {0xaad2,2};case S::PayloadFinish:return {0xaad4,2};case S::PayloadCancel:return {0xaad6,4};case S::PayloadBindModel:return {0xaada,6};case S::PayloadReadModel:return {0xaae0,4};case S::PayloadReadCommand:return {0xaae4,6};case S::PayloadCompareModel:return {0xaaea,2};case S::PayloadAdmitLegacy:return {0xaaec,4};case S::PayloadReadType:return {0xaaf0,6};case S::PayloadComparePatch:return {0xaaf6,2};case S::PayloadAdmitPatch:return {0xaaf8,2};case S::PayloadComparePatchBank:return {0xaafa,2};case S::PayloadAdmitPatchBank:return {0xaafc,4};case S::PayloadCompareMulti:return {0xab00,2};case S::PayloadAdmitMulti:return {0xab02,2};case S::PayloadCompareMultiBank:return {0xab04,2};case S::PayloadAdmitMultiBank:return {0xab06,4};case S::PayloadCompareGlobal:return {0xab0a,2};case S::PayloadAdmitGlobal:return {0xab0c,4};case S::PayloadDone:return {0xab10,2};case S::RequestComparePatch:return {0xab12,2};case S::RequestAdmitPatch:return {0xab14,4};case S::RequestComparePerformance:return {0xab18,2};case S::RequestAdmitPerformance:return {0xab1a,4};case S::RequestReadSlot:return {0xab1e,6};case S::RequestReadBank:return {0xab24,6};case S::RequestAdmitBank:return {0xab2a,2};case S::RequestCompareBank:return {0xab2c,2};case S::RequestAdmitBankRange:return {0xab2e,2};case S::RequestReadProgram:return {0xab30,6};case S::RequestCompareProgram:return {0xab36,2};case S::RequestAdmitProgram:return {0xab38,2};case S::RequestCompareBankDump:return {0xab3a,2};case S::RequestAdmitBankDump:return {0xab3c,4};case S::RequestFinish:return {0xab40,2};default:throw std::runtime_error("Invalid SysEx framing stage");}}unsigned route(T::Route r){switch(r){case T::Route::Byte:return 0x119c;case T::Route::Character:return 0x976;case T::Route::Text:return 0x9b8;case T::Route::Number:return 0x9dc;case T::Route::LegacyPatch:return 0xaf94;case T::Route::LegacyPatchBank:return 0xafc8;case T::Route::LegacyMulti:return 0xaff0;case T::Route::ApplyPatch:return 0xab42;case T::Route::WritePatchBank:return 0xaba8;case T::Route::ApplyMulti:return 0xab6c;case T::Route::WriteMultiBank:return 0xabca;case T::Route::ApplyGlobal:return 0xab94;case T::Route::DumpPatch:return 0xacaa;case T::Route::DumpPerformance:return 0xaec8;case T::Route::DumpBank:return 0xaeee;default:throw std::runtime_error("Invalid SysEx framing route");}}}
void setSysExFramePartialForTests(SysExFramePartialTest fn){partialTest=fn;}
bool SysExFrameCalls::step(ControllerCpu& h){using namespace controllerDetail;const auto pc=unsigned(h.getPC());auto& v=h.regs;std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}std::optional<K> kind;switch(pc){case 0xa924:kind=K::ByteOutput;break;case 0xa928:kind=K::CharacterOutput;break;case 0xa92c:kind=K::TextOutput;break;case 0xa930:kind=K::NumberOutput;break;case 0xa934:kind=K::Noop;break;case 0xa936:kind=K::Begin;break;case 0xa944:kind=K::Restart;break;case 0xa96a:kind=K::Dispatch;break;case 0xa9a6:kind=K::Header;break;case 0xa9b2:kind=K::Append;break;case 0xa9d6:kind=K::Validate;break;case 0xaa36:kind=K::Cancel;break;case 0xaa46:kind=K::Bank;break;case 0xaa6c:kind=K::Legacy;break;case 0xaa86:kind=K::Model;break;case 0xaac4:kind=K::Payload;break;case 0xab12:kind=K::Request;break;default:break;}
 if(kind&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("SysEx frame capacity exceeded");selected->emplace(*kind,uint8_t(v[0].rl),uint32_t(v[4].er),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& t=call.task;if(t.awaitingChild()){if(t.child()==C::Append)t.resumeAppend(uint16_t(v[0].r));else t.resumeChild();}
 const auto stage=t.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(t).length);uint32_t readData=0;
 auto publicationAddress=[&](S stage)->uint32_t{switch(stage){case S::RestartPublishState:return 0x1603a;case S::AppendPublishInput:return t.cursor();case S::ValidatePublishCommand:return 0x1603b;case S::CancelPublishState:return 0x1603a;case S::CancelPublishRemaining:return 0x15c1d;default:throw std::runtime_error("Invalid SysEx publication");}};auto timing=[&](S stage){const auto at=publicationAddress(stage)&0xffffff;if(at>=0x400000)return 3u;return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=t.nextClocks(t.publication()?timing(stage):3);
 auto read=[&](S stage)->uint32_t{switch(stage){case S::BeginReadState:return uint8_t(h.read8(0x1603a));case S::RestartReadState:return uint8_t(h.read8(0x1603a));case S::DispatchReadState:return uint8_t(h.read8(0x1603a));case S::DispatchReadModel:return uint8_t(h.read8(0x1b4));case S::DispatchReadCommand:return uint8_t(h.read8(0x1603b));case S::HeaderReadInput:return uint8_t(h.read8(t.payload()));case S::AppendReadCursor:return uint32_t(h.read32(0x15c32));case S::AppendReadCount:return uint16_t(h.read16(0x15c36));case S::ValidateReadHeader:return uint8_t(h.read8(0x15c38+t.index()));case S::ValidateReadModel:return uint8_t(h.read8(0x1b0+t.index()));case S::ValidateReadCommand:return uint8_t(h.read8(0x15c38+t.index()));case S::ValidateReadModelCommand:return uint8_t(h.read8(0x1b0+t.index()));case S::ValidateReadDevice:return uint8_t(h.read8(0x15c39+t.index()));case S::ValidateReadDeviceId:return uint8_t(h.read8(0x168fc+242));case S::BankReadInput:return uint8_t(h.read8(t.payload()));case S::BankReadType:return uint8_t(h.read8(0x1603c));case S::LegacyReadType:return uint8_t(h.read8(0x1603c));case S::ModelReadInput:return uint8_t(h.read8(t.payload()));case S::ModelReadType:return uint8_t(h.read8(0x1603c));case S::PayloadReadInput:return uint8_t(h.read8(t.payload()));case S::PayloadReadSize:return uint16_t(h.read16(0x16038));case S::PayloadReadModel:return uint8_t(h.read8(0x1b4));case S::PayloadReadCommand:return uint8_t(h.read8(0x1603b));case S::PayloadReadType:return uint8_t(h.read8(0x1603c));case S::RequestReadSlot:return uint8_t(h.read8(0x1603d));case S::RequestReadBank:return uint8_t(h.read8(0x16040));case S::RequestReadProgram:return uint8_t(h.read8(0x16041));default:throw std::runtime_error("Invalid SysEx framing read");}};auto capture=[&](S stage){return readData=read(stage);};auto write=[&](S stage,uint32_t data){switch(stage){case S::RestartPublishState:h.write8(int8_t(data),0x1603a);break;case S::RestartPublishCount:h.write16(int16_t(data),0x15c36);break;case S::RestartPublishCursor:h.write32(int32_t(data),0x15c32);break;case S::AppendPublishInput:h.write8(int8_t(data),t.cursor());break;case S::AppendPublishCursor:h.write32(int32_t(data),0x15c32);break;case S::AppendPublishCount:h.write16(int16_t(data),0x15c36);break;case S::ValidatePublishCommand:h.write8(int8_t(data),0x1603b);break;case S::ValidatePublishCursor:h.write32(int32_t(data),0x15c32);break;case S::CancelPublishState:h.write8(int8_t(data),0x1603a);break;case S::CancelPublishRemaining:h.write8(int8_t(data),0x15c1d);break;case S::PublishSize:h.write16(int16_t(data),0x16038);break;default:throw std::runtime_error("Invalid SysEx framing write");}};if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("SysEx framing deadline mismatch");switch(stage){
case S::OutputByte:{h.clockI(2);break;}
case S::OutputCharacter:{h.clockI(2);break;}
case S::OutputText:{h.clockI(2);break;}
case S::OutputNumber:{h.clockI(2);break;}
case S::Noop:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::BeginReadState:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::BeginAdmitState:{;break;}
case S::BeginRestart:{clocks.call(0xa944);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame dispatch timing");return true;break;}
case S::BeginDispatch:{clocks.call(0xa96a);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame dispatch timing");return true;break;}
case S::BeginFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::RestartReadState:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::RestartTickState:{clocks.inc(v[0].rl,1);break;}
case S::RestartPublishState:{value(h,v[0].rl);break;}
case S::RestartClearCount:{v[0].r=h.sub16(v[0].r,v[0].r);break;}
case S::RestartPublishCount:{value(h,v[0].r);break;}
case S::RestartBindPayload:{clocks.move(v[0].er,0x15c38);break;}
case S::RestartPublishCursor:{value(h,v[0].er);break;}
case S::RestartFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::DispatchReadState:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::DispatchCompareHeader:{h.sub8(v[0].rl,0x1);break;}
case S::DispatchAdmitHeader:{;break;}
case S::DispatchBindModel:{clocks.move(v[1].er,0x1b0);break;}
case S::DispatchReadModel:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::DispatchCompareCommand:{h.sub8(v[0].rl,0x2);break;}
case S::DispatchAdmitCommand:{;break;}
case S::DispatchReadCommand:{clocks.move(v[0].rh,int8_t(readData));break;}
case S::DispatchCompareModel:{h.sub8(v[0].rh,v[1].rl);break;}
case S::DispatchAdmitModel:{h.clockI(2);break;}
case S::DispatchComparePatchRequest:{h.sub8(v[0].rh,0x40);break;}
case S::DispatchAdmitPatchRequest:{h.clockI(2);break;}
case S::DispatchCompareMultiRequest:{h.sub8(v[0].rh,0x41);break;}
case S::DispatchAdmitMultiRequest:{h.clockI(2);break;}
case S::DispatchSkipCommand:{;break;}
case S::DispatchComparePayload:{h.sub8(v[0].rl,0x3);break;}
case S::DispatchAdmitPayload:{;break;}
case S::DispatchPayload:{h.clockI(2);break;}
case S::DispatchFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::HeaderReadInput:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::HeaderAppend:{clocks.call(0xa9b2);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame dispatch timing");return true;break;}
case S::HeaderCompareCount:{h.sub16(v[0].r,0x6);break;}
case S::HeaderAdmitCount:{;break;}
case S::HeaderFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::AppendReadCursor:{clocks.move(v[1].er,int32_t(readData));break;}
case S::AppendPublishInput:{value(h,v[0].rl);break;}
case S::AppendTickCursor:{v[1].er=int32_t(uint32_t(v[1].er)+0x1);break;}
case S::AppendPublishCursor:{value(h,v[1].er);break;}
case S::AppendReadCount:{clocks.move(v[0].r,int16_t(readData));break;}
case S::AppendTickCount:{clocks.inc(v[0].r,1);break;}
case S::AppendPublishCount:{value(h,v[0].r);break;}
case S::AppendFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::ValidateBindHeader:{clocks.move(v[0].er,0x15c38);break;}
case S::ValidateBindModel:{clocks.move(v[1].er,0x1b0);break;}
case S::ValidateSetCount:{clocks.move(v[2].e,0x4);break;}
case S::ValidateReadHeader:{clocks.move(v[2].rl,int8_t(readData));v[0].er+=1;h.clockI(2);break;}
case S::ValidateReadModel:{clocks.move(v[2].rh,int8_t(readData));v[1].er+=1;h.clockI(2);break;}
case S::ValidateCompareByte:{h.sub8(v[2].rl,v[2].rh);break;}
case S::ValidateAdmitByte:{;break;}
case S::ValidateTickCount:{clocks.inc(v[2].e,-1);break;}
case S::ValidateAdmitCount:{;break;}
case S::ValidateAdmitMagic:{;break;}
case S::ValidateReadCommand:{clocks.move(v[2].rl,int8_t(readData));v[0].er+=1;h.clockI(2);break;}
case S::ValidatePublishCommand:{value(h,v[2].rl);break;}
case S::ValidateReadModelCommand:{clocks.move(v[2].rh,int8_t(readData));v[1].er+=1;h.clockI(2);break;}
case S::ValidateComparePatch:{h.sub8(v[2].rl,0x40);break;}
case S::ValidateAdmitPatch:{;break;}
case S::ValidateCompareMulti:{h.sub8(v[2].rl,0x41);break;}
case S::ValidateAdmitMulti:{;break;}
case S::ValidateCompareModel:{h.sub8(v[2].rh,v[2].rl);break;}
case S::ValidateAdmitModel:{;break;}
case S::ValidateReadDevice:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::ValidateCompareBroadcast:{h.sub8(v[0].rl,0x7f);break;}
case S::ValidateAdmitBroadcast:{;break;}
case S::ValidateBindPatch:{clocks.move(v[3].er,0x168fc);break;}
case S::ValidateReadDeviceId:{clocks.move(v[0].rh,int8_t(readData));break;}
case S::ValidateCompareDevice:{h.sub8(v[0].rl,v[0].rh);break;}
case S::ValidateAdmitDevice:{;break;}
case S::ValidateRestart:{clocks.call(0xa944);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame dispatch timing");return true;break;}
case S::ValidateBindCursor:{clocks.move(v[0].er,0x1603c);break;}
case S::ValidatePublishCursor:{value(h,v[0].er);break;}
case S::ValidateFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::CancelClear:{clocks.move(v[0].rl,0x0);break;}
case S::CancelPublishState:{value(h,v[0].rl);break;}
case S::CancelPublishRemaining:{value(h,v[0].rl);break;}
case S::CancelFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::BankReadInput:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::BankAppend:{clocks.call(0xa9b2);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame dispatch timing");return true;break;}
case S::BankCompareCount:{h.sub16(v[0].r,0x6);break;}
case S::BankAdmitCount:{;break;}
case S::BankFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::BankReadType:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::BankSmallSize:{clocks.move(v[1].r,0x80);break;}
case S::BankCompareSmall:{h.sub8(v[0].rl,0x1);break;}
case S::BankAdmitSmall:{;break;}
case S::BankLargeSize:{clocks.move(v[1].r,0x100);break;}
case S::BankCompareLarge:{h.sub8(v[0].rl,0x2);break;}
case S::BankAdmitLarge:{;break;}
case S::BankCancel:{;break;}
case S::LegacyReadType:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::LegacyComparePatch:{h.sub8(v[0].rl,0x0);break;}
case S::LegacyAdmitPatch:{h.clockI(2);break;}
case S::LegacyComparePatchBank:{h.sub8(v[0].rl,0x1);break;}
case S::LegacyAdmitPatchBank:{h.clockI(2);break;}
case S::LegacyCompareMulti:{h.sub8(v[0].rl,0x2);break;}
case S::LegacyAdmitMulti:{h.clockI(2);break;}
case S::LegacyFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::ModelReadInput:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::ModelAppend:{clocks.call(0xa9b2);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame dispatch timing");return true;break;}
case S::ModelCompareCount:{h.sub16(v[0].r,0x6);break;}
case S::ModelAdmitCount:{;break;}
case S::ModelFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::ModelReadType:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::ModelCompareRequest:{h.sub8(v[0].rl,0x40);break;}
case S::ModelAdmitRequest:{h.clockI(2);break;}
case S::ModelSmallSize:{clocks.move(v[1].r,0x100);break;}
case S::ModelCompareSmall:{h.sub8(v[0].rl,0x1);break;}
case S::ModelAdmitSmall:{;break;}
case S::ModelCompareGlobal:{h.sub8(v[0].rl,0x4);break;}
case S::ModelAdmitGlobal:{;break;}
case S::ModelLargeSize:{clocks.move(v[1].r,0x400);break;}
case S::ModelCompareLarge:{h.sub8(v[0].rl,0x3);break;}
case S::ModelAdmitLarge:{;break;}
case S::ModelCancel:{h.clockI(2);break;}
case S::PublishSize:{value(h,v[1].r);break;}
case S::SizeRestart:{clocks.call(0xa944);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame dispatch timing");return true;break;}
case S::SizeFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::PayloadReadInput:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::PayloadAppend:{clocks.call(0xa9b2);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame dispatch timing");return true;break;}
case S::PayloadReadSize:{clocks.move(v[1].r,int16_t(readData));break;}
case S::PayloadCompareSize:{h.sub16(v[0].r,v[1].r);break;}
case S::PayloadAdmitSize:{;break;}
case S::PayloadFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::PayloadCancel:{clocks.call(0xaa36);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame dispatch timing");return true;break;}
case S::PayloadBindModel:{clocks.move(v[1].er,0x1b0);break;}
case S::PayloadReadModel:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::PayloadReadCommand:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::PayloadCompareModel:{h.sub8(v[0].rl,v[1].rl);break;}
case S::PayloadAdmitLegacy:{h.clockI(2);break;}
case S::PayloadReadType:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::PayloadComparePatch:{h.sub8(v[0].rl,0x0);break;}
case S::PayloadAdmitPatch:{;break;}
case S::PayloadComparePatchBank:{h.sub8(v[0].rl,0x1);break;}
case S::PayloadAdmitPatchBank:{h.clockI(2);break;}
case S::PayloadCompareMulti:{h.sub8(v[0].rl,0x2);break;}
case S::PayloadAdmitMulti:{;break;}
case S::PayloadCompareMultiBank:{h.sub8(v[0].rl,0x3);break;}
case S::PayloadAdmitMultiBank:{h.clockI(2);break;}
case S::PayloadCompareGlobal:{h.sub8(v[0].rl,0x4);break;}
case S::PayloadAdmitGlobal:{h.clockI(2);break;}
case S::PayloadDone:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
case S::RequestComparePatch:{h.sub8(v[0].rl,0x40);break;}
case S::RequestAdmitPatch:{h.clockI(2);break;}
case S::RequestComparePerformance:{h.sub8(v[0].rl,0x44);break;}
case S::RequestAdmitPerformance:{h.clockI(2);break;}
case S::RequestReadSlot:{clocks.move(v[5].rl,int8_t(readData));break;}
case S::RequestReadBank:{clocks.move(v[6].rh,int8_t(readData));break;}
case S::RequestAdmitBank:{;break;}
case S::RequestCompareBank:{h.sub8(v[6].rh,0x4);break;}
case S::RequestAdmitBankRange:{;break;}
case S::RequestReadProgram:{clocks.move(v[6].rl,int8_t(readData));break;}
case S::RequestCompareProgram:{h.sub8(v[6].rl,0x64);break;}
case S::RequestAdmitProgram:{;break;}
case S::RequestCompareBankDump:{h.sub8(v[0].rl,0x41);break;}
case S::RequestAdmitBankDump:{h.clockI(2);break;}
case S::RequestFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame return timing");selected->reset();++completed;return true;break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("SysEx frame bus timing");if(t.handoff()){h.pc=h.makepc(route(t.route()));selected->reset();++handedOff;}else h.pc=h.makepc(point(t).location);return true;}}
