#include "ksSysExActionCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace{SysExActionPartialTest partialTest=nullptr;using T=SysExActionTask;using K=T::Kind;using S=T::Stage;using C=T::Child;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){case S::PatchReadMode:return {0xab42,6};case S::PatchAdmitMode:return {0xab48,2};case S::PatchBindPayload:return {0xab4a,6};case S::PatchApply:return {0xab50,4};case S::PatchScanActive:return {0xab54,4};case S::PatchObserveActive:return {0xab58,2};case S::PatchAdmitActive:return {0xab5a,2};case S::PatchRefresh:return {0xab5c,4};case S::PatchMessage:return {0xab60,6};case S::PatchNotify:return {0xab66,4};case S::PatchFinish:return {0xab6a,2};case S::MultiReadMode:return {0xab6c,6};case S::MultiAdmitMode:return {0xab72,2};case S::MultiBindPayload:return {0xab74,6};case S::MultiApply:return {0xab7a,4};case S::MultiScanActive:return {0xab7e,4};case S::MultiObserveActive:return {0xab82,2};case S::MultiAdmitActive:return {0xab84,2};case S::MultiRefresh:return {0xab86,4};case S::MultiMessage:return {0xab8a,6};case S::MultiNotify:return {0xab90,4};case S::GlobalBindPayload:return {0xab94,6};case S::GlobalApply:return {0xab9a,4};case S::GlobalMessage:return {0xab9e,6};case S::GlobalNotify:return {0xaba4,4};case S::PatchBankResolve:return {0xaba8,2};case S::PatchBankReadBank:return {0xabaa,2};case S::PatchBankAdmitBank:return {0xabac,2};case S::PatchBankCompareBank:return {0xabae,2};case S::PatchBankAdmitBankRange:return {0xabb0,2};case S::PatchBankLargeLimit:return {0xabb2,2};case S::PatchBankCompareLimit:return {0xabb4,2};case S::PatchBankAdmitLimit:return {0xabb6,2};case S::PatchBankSmallLimit:return {0xabb8,2};case S::PatchBankReadProgram:return {0xabba,4};case S::PatchBankCompareProgram:return {0xabbe,2};case S::PatchBankAdmitProgram:return {0xabc0,2};case S::PatchBankReport:return {0xabc2,2};case S::PatchBankCommit:return {0xabc4,4};case S::PatchBankFinish:return {0xabc8,2};case S::MultiBankBindRecord:return {0xabca,6};case S::MultiBankBindPayload:return {0xabd0,6};case S::MultiBankSetBank:return {0xabd6,2};case S::MultiBankPublishBank:return {0xabd8,2};case S::MultiBankReadProgram:return {0xabda,4};case S::MultiBankCompareProgram:return {0xabde,2};case S::MultiBankAdmitProgram:return {0xabe0,2};case S::MultiBankReport:return {0xabe2,2};case S::MultiBankCommit:return {0xabe4,4};case S::MultiBankFinish:return {0xabe8,2};case S::ResolveBindRecord:return {0xabea,6};case S::ResolveBindPayload:return {0xabf0,6};case S::ResolveReadSlot:return {0xabf6,6};case S::ResolveAdmitSlot:return {0xabfc,2};case S::ResolveReadFallback:return {0xabfe,6};case S::ResolvePublishFallback:return {0xac04,2};case S::ResolveFinish:return {0xac06,2};case S::PatchReportRetainRecord:return {0xac08,4};case S::PatchReportRetainPayload:return {0xac0c,4};case S::PatchReportRetainValue:return {0xac10,4};case S::PatchReportMessage:return {0xac14,6};case S::PatchReportShow:return {0xac1a,4};case S::PatchReportReadBank:return {0xac1e,2};case S::PatchReportDigit:return {0xac20,2};case S::PatchReportShowBank:return {0xac22,4};case S::PatchReportCaptureProgram:return {0xac26,2};case S::PatchReportShowProgram:return {0xac28,4};case S::PatchReportReleaseValue:return {0xac2c,4};case S::PatchReportReleasePayload:return {0xac30,4};case S::PatchReportReleaseRecord:return {0xac34,4};case S::PatchReportFinish:return {0xac38,2};case S::MultiReportRetainRecord:return {0xac3a,4};case S::MultiReportRetainPayload:return {0xac3e,4};case S::MultiReportRetainValue:return {0xac42,4};case S::MultiReportMessage:return {0xac46,6};case S::MultiReportShow:return {0xac4c,4};case S::MultiReportReadBank:return {0xac50,2};case S::MultiReportDigit:return {0xac52,2};case S::MultiReportShowBank:return {0xac54,4};case S::MultiReportCaptureProgram:return {0xac58,2};case S::MultiReportShowProgram:return {0xac5a,4};case S::MultiReportReleaseValue:return {0xac5e,4};case S::MultiReportReleasePayload:return {0xac62,4};case S::MultiReportReleaseRecord:return {0xac66,4};case S::MultiReportFinish:return {0xac6a,2};case S::HeaderBindDestination:return {0xac6c,6};case S::HeaderBindSource:return {0xac72,6};case S::HeaderSetCount:return {0xac78,4};case S::HeaderRead:return {0xac7c,2};case S::HeaderPublish:return {0xac7e,2};case S::HeaderAdvance:return {0xac80,2};case S::HeaderTick:return {0xac82,2};case S::HeaderAdmit:return {0xac84,2};case S::HeaderFinish:return {0xac86,2};case S::SendRead:return {0xac88,2};case S::SendMask:return {0xac8a,2};case S::SendByte:return {0xac8c,4};case S::SendTick:return {0xac90,2};case S::SendAdmit:return {0xac92,2};case S::SendFinish:return {0xac94,2};case S::SendHeaderStatus:return {0xac96,2};case S::SendHeaderByte:return {0xac98,4};case S::SendHeaderBind:return {0xac9c,6};case S::SendHeaderCount:return {0xaca2,4};case S::SendHeaderData:return {0xaca6,2};case S::SendHeaderFinish:return {0xaca8,2};default:throw std::runtime_error("Invalid SysEx action stage");}}}
void setSysExActionPartialForTests(SysExActionPartialTest fn){partialTest=fn;}
bool SysExActionCalls::step(ControllerCpu& h){using namespace controllerDetail;const auto pc=unsigned(h.getPC());auto& v=h.regs;std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}std::optional<K> kind;switch(pc){case 0xab42:kind=K::Patch;break;case 0xab6c:kind=K::Multi;break;case 0xab94:kind=K::Global;break;case 0xaba8:kind=K::PatchBank;break;case 0xabca:kind=K::MultiBank;break;case 0xabea:kind=K::Resolve;break;case 0xac08:kind=K::PatchReport;break;case 0xac3a:kind=K::MultiReport;break;case 0xac6c:kind=K::Header;break;case 0xac88:kind=K::Send;break;case 0xac96:kind=K::SendHeader;break;default:break;}
 if(kind&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("SysEx action capacity exceeded");selected->emplace(*kind,uint32_t(v[0].er),uint32_t(v[6].er),uint16_t(v[5].r),uint8_t(v[2].rl),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& t=call.task;if(t.awaitingChild()){switch(t.child()){case C::Active:t.resumeActive(v[0].r!=0);break;case C::Resolve:t.resumeResolve(uint32_t(v[0].er));break;case C::UiText:case C::UiByte:t.resumeUi(uint32_t(v[0].er),uint8_t(v[2].rl));break;default:t.resumeChild();break;}}
 const auto stage=t.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(t).length);uint32_t readData=0;
 auto publicationAddress=[&](S stage)->uint32_t{switch(stage){case S::MultiBankPublishBank:return 0x16040;case S::ResolvePublishFallback:return 0x16040;case S::HeaderPublish:return 0x16044+t.index();default:throw std::runtime_error("Invalid SysEx action publication");}};auto timing=[&](S stage){const auto at=publicationAddress(stage)&0xffffff;if(at>=0x400000)return 3u;return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=t.nextClocks(t.publication()?timing(stage):3);
 auto read=[&](S stage)->uint32_t{switch(stage){case S::PatchReadMode:return uint8_t(h.read8(0x167ba));case S::MultiReadMode:return uint8_t(h.read8(0x167ba));case S::PatchBankReadBank:return uint8_t(h.read8(t.record()));case S::PatchBankReadProgram:return uint8_t(h.read8(t.record()+1));case S::MultiBankReadProgram:return uint8_t(h.read8(0x16041));case S::ResolveReadSlot:return uint8_t(h.read8(0x1603d));case S::ResolveReadFallback:return uint8_t(h.read8(0x167ac));case S::PatchReportReadBank:return uint8_t(h.read8(t.record()));case S::MultiReportReadBank:return uint8_t(h.read8(t.record()));case S::HeaderRead:return uint8_t(h.read8(0x1b0+t.index()));case S::SendRead:return uint8_t(h.read8(t.source()));default:throw std::runtime_error("Invalid SysEx action read");}};auto capture=[&](S stage){return readData=read(stage);};auto write=[&](S stage,uint32_t data){switch(stage){case S::MultiBankPublishBank:h.write8(int8_t(data),0x16040);break;case S::ResolvePublishFallback:h.write8(int8_t(data),0x16040);break;case S::HeaderPublish:h.write8(int8_t(data),0x16044+t.index());break;default:throw std::runtime_error("Invalid SysEx action write");}};if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("SysEx action deadline mismatch");switch(stage){
case S::PatchReadMode:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::PatchAdmitMode:{;break;}
case S::PatchBindPayload:{clocks.move(v[0].er,0x15c38);break;}
case S::PatchApply:{clocks.call(0x63fe);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::PatchScanActive:{clocks.call(0x30e4);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::PatchObserveActive:{clocks.move(v[0].r,v[0].r);break;}
case S::PatchAdmitActive:{;break;}
case S::PatchRefresh:{clocks.call(0x4c44);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::PatchMessage:{clocks.move(v[6].er,0xf349);break;}
case S::PatchNotify:{h.clockI(2);break;}
case S::PatchFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action return timing");selected->reset();++completed;return true;break;}
case S::MultiReadMode:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::MultiAdmitMode:{;break;}
case S::MultiBindPayload:{clocks.move(v[0].er,0x15c38);break;}
case S::MultiApply:{clocks.call(0x6ffc);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::MultiScanActive:{clocks.call(0x30e4);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::MultiObserveActive:{clocks.move(v[0].r,v[0].r);break;}
case S::MultiAdmitActive:{;break;}
case S::MultiRefresh:{clocks.call(0x4c44);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::MultiMessage:{clocks.move(v[6].er,0xf35e);break;}
case S::MultiNotify:{h.clockI(2);break;}
case S::GlobalBindPayload:{clocks.move(v[1].er,0x15c38);break;}
case S::GlobalApply:{clocks.call(0x66dc);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::GlobalMessage:{clocks.move(v[6].er,0xf373);break;}
case S::GlobalNotify:{h.clockI(2);break;}
case S::PatchBankResolve:{clocks.call(0xabea);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::PatchBankReadBank:{clocks.move(v[2].rh,int8_t(readData));break;}
case S::PatchBankAdmitBank:{;break;}
case S::PatchBankCompareBank:{h.sub8(v[2].rh,0x8);break;}
case S::PatchBankAdmitBankRange:{;break;}
case S::PatchBankLargeLimit:{clocks.move(v[3].rl,0x64);break;}
case S::PatchBankCompareLimit:{h.sub8(v[2].rh,0x4);break;}
case S::PatchBankAdmitLimit:{;break;}
case S::PatchBankSmallLimit:{clocks.move(v[3].rl,0x32);break;}
case S::PatchBankReadProgram:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::PatchBankCompareProgram:{h.sub8(v[2].rl,v[3].rl);break;}
case S::PatchBankAdmitProgram:{;break;}
case S::PatchBankReport:{clocks.call(0xac08);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::PatchBankCommit:{clocks.call(0x64e4);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::PatchBankFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action return timing");selected->reset();++completed;return true;break;}
case S::MultiBankBindRecord:{clocks.move(v[0].er,0x16040);break;}
case S::MultiBankBindPayload:{clocks.move(v[1].er,0x15c38);break;}
case S::MultiBankSetBank:{clocks.move(v[2].rl,0x1);break;}
case S::MultiBankPublishBank:{value(h,v[2].rl);break;}
case S::MultiBankReadProgram:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::MultiBankCompareProgram:{h.sub8(v[2].rl,0x64);break;}
case S::MultiBankAdmitProgram:{;break;}
case S::MultiBankReport:{clocks.call(0xac3a);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::MultiBankCommit:{clocks.call(0x72e0);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::MultiBankFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action return timing");selected->reset();++completed;return true;break;}
case S::ResolveBindRecord:{clocks.move(v[0].er,0x16040);break;}
case S::ResolveBindPayload:{clocks.move(v[1].er,0x15c38);break;}
case S::ResolveReadSlot:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::ResolveAdmitSlot:{;break;}
case S::ResolveReadFallback:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::ResolvePublishFallback:{value(h,v[2].rl);break;}
case S::ResolveFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action return timing");selected->reset();++completed;return true;break;}
case S::PatchReportRetainRecord:{clocks.push(v[0].er);call.depth+=4;break;}
case S::PatchReportRetainPayload:{clocks.push(v[1].er);call.depth+=4;break;}
case S::PatchReportRetainValue:{clocks.push(v[6].er);call.depth+=4;break;}
case S::PatchReportMessage:{clocks.move(v[6].er,0xf385);break;}
case S::PatchReportShow:{clocks.call(0x55b8);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::PatchReportReadBank:{clocks.move(v[6].rl,int8_t(readData));break;}
case S::PatchReportDigit:{v[6].rl=h.add8(v[6].rl,0x30);break;}
case S::PatchReportShowBank:{clocks.call(0xa928);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::PatchReportCaptureProgram:{clocks.move(v[0].rl,v[2].rl);break;}
case S::PatchReportShowProgram:{clocks.call(0xa930);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::PatchReportReleaseValue:{clocks.pop(v[6].er);call.depth-=4;break;}
case S::PatchReportReleasePayload:{clocks.pop(v[1].er);call.depth-=4;break;}
case S::PatchReportReleaseRecord:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::PatchReportFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action return timing");selected->reset();++completed;return true;break;}
case S::MultiReportRetainRecord:{clocks.push(v[0].er);call.depth+=4;break;}
case S::MultiReportRetainPayload:{clocks.push(v[1].er);call.depth+=4;break;}
case S::MultiReportRetainValue:{clocks.push(v[6].er);call.depth+=4;break;}
case S::MultiReportMessage:{clocks.move(v[6].er,0xf398);break;}
case S::MultiReportShow:{clocks.call(0x55b8);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::MultiReportReadBank:{clocks.move(v[6].rl,int8_t(readData));break;}
case S::MultiReportDigit:{v[6].rl=h.add8(v[6].rl,0x30);break;}
case S::MultiReportShowBank:{clocks.call(0xa928);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::MultiReportCaptureProgram:{clocks.move(v[0].rl,v[2].rl);break;}
case S::MultiReportShowProgram:{clocks.call(0xa930);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::MultiReportReleaseValue:{clocks.pop(v[6].er);call.depth-=4;break;}
case S::MultiReportReleasePayload:{clocks.pop(v[1].er);call.depth-=4;break;}
case S::MultiReportReleaseRecord:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::MultiReportFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action return timing");selected->reset();++completed;return true;break;}
case S::HeaderBindDestination:{clocks.move(v[1].er,0x16044);break;}
case S::HeaderBindSource:{clocks.move(v[0].er,0x1b0);break;}
case S::HeaderSetCount:{clocks.move(v[2].e,0xc);break;}
case S::HeaderRead:{clocks.move(v[2].rl,int8_t(readData));v[0].er+=1;h.clockI(2);break;}
case S::HeaderPublish:{value(h,v[2].rl);break;}
case S::HeaderAdvance:{clocks.inc(v[1].er,1);break;}
case S::HeaderTick:{clocks.inc(v[2].e,-1);break;}
case S::HeaderAdmit:{;break;}
case S::HeaderFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action return timing");selected->reset();++completed;return true;break;}
case S::SendRead:{clocks.move(v[0].rh,int8_t(readData));v[6].er+=1;h.clockI(2);break;}
case S::SendMask:{v[0].rh&=0x7f;value(h,v[0].rh);break;}
case S::SendByte:{clocks.call(0xa924);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::SendTick:{clocks.inc(v[5].r,-1);break;}
case S::SendAdmit:{;break;}
case S::SendFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action return timing");selected->reset();++completed;return true;break;}
case S::SendHeaderStatus:{clocks.move(v[0].rh,0xfffffff0);break;}
case S::SendHeaderByte:{clocks.call(0xa924);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::SendHeaderBind:{clocks.move(v[6].er,0x16044);break;}
case S::SendHeaderCount:{clocks.move(v[5].r,0xc);break;}
case S::SendHeaderData:{clocks.call(0xac88);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action dispatch timing");return true;break;}
case S::SendHeaderFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action return timing");selected->reset();++completed;return true;break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("SysEx action bus timing");if(t.handoff()){h.pc=h.makepc(0x55b8);selected->reset();++handedOff;}else h.pc=h.makepc(point(t).location);return true;}}
