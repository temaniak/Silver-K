#include "ksLegacySysExCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks{namespace{LegacySysExPartialTest partialTest=nullptr;using T=LegacySysExTask;using K=T::Kind;using S=T::Stage;using C=T::Child;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){case S::ApplyReadMode:return{0xaf94,6};case S::ApplyAdmitMode:return{0xaf9a,2};case S::ApplyBindExpand:return{0xaf9c,6};case S::ApplyExpand:return{0xafa2,4};case S::ApplyBindPayload:return{0xafa6,6};case S::ApplyPatch:return{0xafac,4};case S::ApplyScanActive:return{0xafb0,4};case S::ApplyObserveActive:return{0xafb4,2};case S::ApplyAdmitActive:return{0xafb6,2};case S::ApplyRefresh:return{0xafb8,4};case S::ApplyMessage:return{0xafbc,6};case S::ApplyNotify:return{0xafc2,4};case S::ApplyFinish:return{0xafc6,2};case S::PatchResolve:return{0xafc8,4};case S::PatchReadBank:return{0xafcc,2};case S::PatchAdmitBank:return{0xafce,2};case S::PatchCompareBank:return{0xafd0,2};case S::PatchAdmitBankRange:return{0xafd2,2};case S::PatchReadProgram:return{0xafd4,4};case S::PatchCompareProgram:return{0xafd8,2};case S::PatchAdmitProgram:return{0xafda,2};case S::PatchReport:return{0xafdc,4};case S::PatchBindExpand:return{0xafe0,6};case S::PatchExpand:return{0xafe6,4};case S::PatchCommit:return{0xafea,4};case S::PatchFinish:return{0xafee,2};case S::MultiResolve:return{0xaff0,4};case S::MultiReadBank:return{0xaff4,2};case S::MultiAdmitBank:return{0xaff6,2};case S::MultiCompareBank:return{0xaff8,2};case S::MultiAdmitBankRange:return{0xaffa,2};case S::MultiReadProgram:return{0xaffc,4};case S::MultiCaptureParity:return{0xb000,2};case S::MultiMaskParity:return{0xb002,2};case S::MultiAdmitParity:return{0xb004,2};case S::MultiCompareProgram:return{0xb006,2};case S::MultiAdmitProgram:return{0xb008,2};case S::MultiReport:return{0xb00a,4};case S::MultiWrite:return{0xb00e,2};case S::MultiFinish:return{0xb010,2};case S::PairBindBackup:return{0xb012,6};case S::PairBindSource:return{0xb018,6};case S::PairCount:return{0xb01e,4};case S::PairReadSource:return{0xb022,2};case S::PairPublishBackup:return{0xb024,2};case S::PairAdvanceBackup:return{0xb026,2};case S::PairTickBackup:return{0xb028,2};case S::PairAdmitBackup:return{0xb02a,2};case S::PairBindFirst:return{0xb02c,6};case S::PairExpandFirst:return{0xb032,2};case S::PairCommitFirst:return{0xb034,4};case S::PairRebindBackup:return{0xb038,6};case S::PairBindSecond:return{0xb03e,6};case S::PairSecondCount:return{0xb044,4};case S::PairReadBackup:return{0xb048,2};case S::PairPublishSource:return{0xb04a,2};case S::PairAdvanceSource:return{0xb04c,2};case S::PairTickSource:return{0xb04e,2};case S::PairAdmitSource:return{0xb050,2};case S::PairBindFinal:return{0xb052,6};case S::PairExpandSecond:return{0xb058,2};case S::PairResolve:return{0xb05a,4};case S::PairReadProgram:return{0xb05e,4};case S::PairAdvanceProgram:return{0xb062,2};case S::PairPublishProgram:return{0xb064,4};case S::PairCommitFinal:return{0xb068,4};default:throw std::runtime_error("Invalid legacy SysEx stage");}}}
void setLegacySysExPartialForTests(LegacySysExPartialTest f){partialTest=f;}
bool LegacySysExCalls::step(ControllerCpu& h){using namespace controllerDetail;const auto pc=unsigned(h.getPC());auto& v=h.regs;std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}std::optional<K> kind;switch(pc){case 0xaf94:kind=K::Apply;break;case 0xafc8:kind=K::Patch;break;case 0xaff0:kind=K::Multi;break;case 0xb012:kind=K::Pair;break;default:break;}if(kind&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Legacy SysEx capacity exceeded");selected->emplace(*kind,uint32_t(v[0].er),h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& t=(**selected).task;if(t.awaitingChild()){switch(t.child()){case C::Active:t.resumeActive(v[0].r!=0);break;case C::Resolve:t.resumeResolve(uint32_t(v[0].er));break;default:t.resumeChild();break;}}
 const auto stage=t.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(t).length);uint32_t readData=0;auto address=[&](S stage)->uint32_t{switch(stage){case S::PairPublishBackup:return 0x16050+t.index();case S::PairPublishSource:return 0x15c38+t.index();case S::PairPublishProgram:return t.record()+1;default:throw std::runtime_error("Invalid legacy SysEx publication");}};auto timing=[&](S stage){const auto at=address(stage)&0xffffff;return at>=0x400000||(h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=t.nextClocks(t.publication()?timing(stage):3);auto read=[&](S stage)->uint32_t{switch(stage){case S::ApplyReadMode:return uint8_t(h.read8(0x167ba));case S::PatchReadBank:return uint8_t(h.read8(t.record()));case S::PatchReadProgram:return uint8_t(h.read8(t.record()+1));case S::MultiReadBank:return uint8_t(h.read8(t.record()));case S::MultiReadProgram:return uint8_t(h.read8(t.record()+1));case S::PairReadSource:return uint8_t(h.read8(0x15cb8+t.index()));case S::PairReadBackup:return uint8_t(h.read8(0x16050+t.index()));case S::PairReadProgram:return uint8_t(h.read8(t.record()+1));default:throw std::runtime_error("Invalid legacy SysEx read");}};auto capture=[&](S s){return readData=read(s);};auto write=[&](S stage,uint32_t data){switch(stage){case S::PairPublishBackup:h.write8(int8_t(data),0x16050+t.index());break;case S::PairPublishSource:h.write8(int8_t(data),0x15c38+t.index());break;case S::PairPublishProgram:h.write8(int8_t(data),t.record()+1);break;default:throw std::runtime_error("Invalid legacy SysEx write");}};if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("Legacy SysEx deadline mismatch");switch(stage){
case S::ApplyReadMode:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::ApplyAdmitMode:{;break;}
case S::ApplyBindExpand:{clocks.move(v[5].er,0x15c38);break;}
case S::ApplyExpand:{clocks.call(0xb06c);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::ApplyBindPayload:{clocks.move(v[0].er,0x15c38);break;}
case S::ApplyPatch:{clocks.call(0x63fe);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::ApplyScanActive:{clocks.call(0x30e4);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::ApplyObserveActive:{clocks.move(v[0].r,v[0].r);break;}
case S::ApplyAdmitActive:{;break;}
case S::ApplyRefresh:{clocks.call(0x4c44);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::ApplyMessage:{clocks.move(v[6].er,0xf349);break;}
case S::ApplyNotify:{h.clockI(2);break;}
case S::ApplyFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx return timing");selected->reset();++completed;return true;break;}
case S::PatchResolve:{clocks.call(0xabea);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::PatchReadBank:{clocks.move(v[2].rh,int8_t(readData));break;}
case S::PatchAdmitBank:{;break;}
case S::PatchCompareBank:{h.sub8(v[2].rh,0x4);break;}
case S::PatchAdmitBankRange:{;break;}
case S::PatchReadProgram:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::PatchCompareProgram:{h.sub8(v[2].rl,0x64);break;}
case S::PatchAdmitProgram:{;break;}
case S::PatchReport:{clocks.call(0xac08);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::PatchBindExpand:{clocks.move(v[5].er,0x15c38);break;}
case S::PatchExpand:{clocks.call(0xb06c);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::PatchCommit:{clocks.call(0x64e4);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::PatchFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx return timing");selected->reset();++completed;return true;break;}
case S::MultiResolve:{clocks.call(0xabea);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::MultiReadBank:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::MultiAdmitBank:{;break;}
case S::MultiCompareBank:{h.sub8(v[2].rl,0x4);break;}
case S::MultiAdmitBankRange:{;break;}
case S::MultiReadProgram:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::MultiCaptureParity:{clocks.move(v[2].rh,v[2].rl);break;}
case S::MultiMaskParity:{v[2].rh&=0x1;value(h,v[2].rh);break;}
case S::MultiAdmitParity:{;break;}
case S::MultiCompareProgram:{h.sub8(v[2].rl,0x62);break;}
case S::MultiAdmitProgram:{;break;}
case S::MultiReport:{clocks.call(0xac08);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::MultiWrite:{clocks.call(0xb012);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::MultiFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx return timing");selected->reset();++completed;return true;break;}
case S::PairBindBackup:{clocks.move(v[6].er,0x16050);break;}
case S::PairBindSource:{clocks.move(v[5].er,0x15cb8);break;}
case S::PairCount:{clocks.move(v[4].e,0x80);break;}
case S::PairReadSource:{clocks.move(v[4].rl,int8_t(readData));v[5].er+=1;h.clockI(2);break;}
case S::PairPublishBackup:{value(h,v[4].rl);break;}
case S::PairAdvanceBackup:{clocks.inc(v[6].er,1);break;}
case S::PairTickBackup:{clocks.inc(v[4].e,-1);break;}
case S::PairAdmitBackup:{;break;}
case S::PairBindFirst:{clocks.move(v[5].er,0x15c38);break;}
case S::PairExpandFirst:{clocks.call(0xb06c);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::PairCommitFirst:{clocks.call(0x64e4);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::PairRebindBackup:{clocks.move(v[6].er,0x16050);break;}
case S::PairBindSecond:{clocks.move(v[5].er,0x15c38);break;}
case S::PairSecondCount:{clocks.move(v[4].e,0x80);break;}
case S::PairReadBackup:{clocks.move(v[4].rl,int8_t(readData));v[6].er+=1;h.clockI(2);break;}
case S::PairPublishSource:{value(h,v[4].rl);break;}
case S::PairAdvanceSource:{clocks.inc(v[5].er,1);break;}
case S::PairTickSource:{clocks.inc(v[4].e,-1);break;}
case S::PairAdmitSource:{;break;}
case S::PairBindFinal:{clocks.move(v[5].er,0x15c38);break;}
case S::PairExpandSecond:{clocks.call(0xb06c);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::PairResolve:{clocks.call(0xabea);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx call timing");return true;break;}
case S::PairReadProgram:{clocks.move(v[2].rl,int8_t(readData));break;}
case S::PairAdvanceProgram:{clocks.inc(v[2].rl,1);break;}
case S::PairPublishProgram:{value(h,v[2].rl);break;}
case S::PairCommitFinal:{h.clockI(2);break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("Legacy SysEx bus timing");if(t.handoff()){h.pc=h.makepc(t.route()==T::Route::Notify?0x55b8:0x64e4);selected->reset();++handedOff;}else h.pc=h.makepc(point(t).location);return true;}}
