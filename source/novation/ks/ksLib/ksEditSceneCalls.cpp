#include "ksEditSceneCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks{namespace{EditScenePartialTest partialTest=nullptr;using T=EditSceneTask;using K=T::Kind;using S=T::Stage;using C=T::Child;struct Point{unsigned location,length;};Point point(const T&t){switch(t.stage()){case S::ActivateReadBank:return{0xb786,6};case S::ActivatePublishPrevious:return{0xb78c,6};case S::ActivateAdmitBank:return{0xb792,2};case S::ActivateLoad:return{0xb794,4};case S::ActivateSave:return{0xb798,2};case S::ActivateApply:return{0xb79a,2};case S::ActivateFinish:return{0xb79c,2};case S::ApplyReadEdit:return{0xb79e,8};case S::ApplySkipHeader:return{0xb7a6,6};case S::ApplyReadPatch:return{0xb7ac,8};case S::ApplyBindSaved:return{0xb7b4,6};case S::ApplyCount:return{0xb7ba,2};case S::ApplyClearIndex:return{0xb7bc,2};case S::ApplyReadIndex:return{0xb7be,2};case S::ApplyReadValue:return{0xb7c0,2};case S::ApplyAdmitValue:return{0xb7c2,2};case S::ApplyDecodeValue:return{0xb7c4,2};case S::ApplyLocateParameter:return{0xb7c6,2};case S::ApplyReadOld:return{0xb7c8,2};case S::ApplyPublishValue:return{0xb7ca,2};case S::ApplyObserveSave:return{0xb7cc,2};case S::ApplyAdmitSave:return{0xb7ce,2};case S::ApplyPublishOld:return{0xb7d0,2};case S::ApplyAdvanceSaved:return{0xb7d2,2};case S::ApplyTick:return{0xb7d4,2};case S::ApplyAdmit:return{0xb7d6,2};case S::ApplyNotify:return{0xb7d8,4};case S::ApplyFinish:return{0xb7dc,2};case S::RestoreReadPrevious:return{0xb7de,6};case S::RestoreAdmitPrevious:return{0xb7e4,2};case S::RestoreReadSelected:return{0xb7e6,6};case S::RestoreRetainSelection:return{0xb7ec,2};case S::RestorePublishSelected:return{0xb7ee,6};case S::RestoreLoad:return{0xb7f4,4};case S::RestoreReadEdit:return{0xb7f8,8};case S::RestoreSkipHeader:return{0xb800,6};case S::RestoreReadPatch:return{0xb806,8};case S::RestoreBindSaved:return{0xb80e,6};case S::RestoreCount:return{0xb814,2};case S::RestoreClearIndex:return{0xb816,2};case S::RestoreReadIndex:return{0xb818,2};case S::RestoreReadEnabled:return{0xb81a,2};case S::RestoreAdmitEnabled:return{0xb81c,2};case S::RestoreReadOld:return{0xb81e,2};case S::RestoreLocateParameter:return{0xb820,2};case S::RestorePublishValue:return{0xb822,2};case S::RestoreAdvanceSaved:return{0xb824,2};case S::RestoreTick:return{0xb826,2};case S::RestoreAdmit:return{0xb828,2};case S::RestoreReleaseSelection:return{0xb82a,2};case S::RestorePublishSelection:return{0xb82c,6};case S::RestoreClearPrevious:return{0xb832,2};case S::RestorePublishPrevious:return{0xb834,6};case S::RestoreFinish:return{0xb83a,2};case S::BackupReadEdit:return{0xb8ac,8};case S::BackupBindSaved:return{0xb8b4,6};case S::BackupCount:return{0xb8ba,4};case S::BackupRead:return{0xb8be,2};case S::BackupPublish:return{0xb8c0,2};case S::BackupAdvance:return{0xb8c2,2};case S::BackupTick:return{0xb8c4,2};case S::BackupAdmit:return{0xb8c6,2};case S::BackupFinish:return{0xb8c8,2};case S::CopyReadEdit:return{0xb8ca,8};case S::CopyBindSaved:return{0xb8d2,6};case S::CopyCount:return{0xb8d8,4};case S::CopyRead:return{0xb8dc,2};case S::CopyPublish:return{0xb8de,2};case S::CopyAdvance:return{0xb8e0,2};case S::CopyTick:return{0xb8e2,2};case S::CopyAdmit:return{0xb8e4,2};case S::CopyFinish:return{0xb8e6,2};default:throw std::runtime_error("Invalid edit scene stage");}}}void setEditScenePartialForTests(EditScenePartialTest f){partialTest=f;}
bool EditSceneCalls::step(ControllerCpu&h){using namespace controllerDetail;auto pc=unsigned(h.getPC());auto&v=h.regs;std::optional<Call>*selected=nullptr;for(auto&c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}std::optional<K>kind;switch(pc){case 0xb786:kind=K::Activate;break;case 0xb79e:kind=K::Apply;break;case 0xb7de:kind=K::Restore;break;case 0xb8ac:kind=K::Backup;break;case 0xb8ca:kind=K::Copy;break;default:break;}if(kind&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto&c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Edit scene capacity exceeded");selected->emplace(*kind,v[5].rl!=0,h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto&call=**selected;auto&t=call.task;if(t.awaitingChild())t.resumeChild();auto stage=t.stage();auto begin=h.cycles;RuntimeBoundary clocks(h,pc,point(t).length);uint32_t readData=0;auto timing=[&](S stage){uint32_t at=0;unsigned width=0;bool store=false;switch(stage){case S::ActivateReadBank:at=0x1679e;width=1;store=false;break;case S::ApplyReadEdit:at=0x1748e;width=4;store=false;break;case S::ApplyReadPatch:at=0x167bc;width=4;store=false;break;case S::ApplyReadIndex:at=t.table()+t.index()*2;width=1;store=false;break;case S::ApplyReadValue:at=t.table()+t.index()*2+1;width=1;store=false;break;case S::ApplyReadOld:at=t.target();width=1;store=false;break;case S::RestoreReadPrevious:at=0x167a0;width=1;store=false;break;case S::RestoreReadSelected:at=0x1679e;width=1;store=false;break;case S::RestoreReadEdit:at=0x1748e;width=4;store=false;break;case S::RestoreReadPatch:at=0x167bc;width=4;store=false;break;case S::RestoreReadIndex:at=t.table()+t.index()*2;width=1;store=false;break;case S::RestoreReadEnabled:at=t.table()+t.index()*2+1;width=1;store=false;break;case S::RestoreReadOld:at=0x17495+t.index();width=1;store=false;break;case S::BackupReadEdit:at=0x1748e;width=4;store=false;break;case S::BackupRead:at=t.table()+t.index();width=1;store=false;break;case S::CopyReadEdit:at=0x1748e;width=4;store=false;break;case S::CopyRead:at=(0x1734e)+t.index();width=1;store=false;break;case S::ActivatePublishPrevious:at=0x167a0;width=1;store=true;break;case S::ApplyPublishValue:at=t.target();width=1;store=true;break;case S::ApplyPublishOld:at=0x17495+t.index();width=1;store=true;break;case S::RestorePublishSelected:at=0x1679e;width=1;store=true;break;case S::RestorePublishValue:at=t.target();width=1;store=true;break;case S::RestorePublishSelection:at=0x1679e;width=1;store=true;break;case S::RestorePublishPrevious:at=0x167a0;width=1;store=true;break;case S::BackupPublish:at=(0x1734e)+t.index();width=1;store=true;break;case S::CopyPublish:at=t.table()+t.index();width=1;store=true;break;default:return 0u;}int last=store?h.lastwrite:h.lastread;unsigned cost=0;for(unsigned i=0;i<width;++i){auto addr=(at+i)&0xffffff;cost+=addr>=0x400000||(last&1)||addr!=(unsigned(last)|1u)?3u:0u;last=int(addr);}return cost;};auto cost=t.nextClocks(timing(stage));auto read=[&](S stage)->uint32_t{switch(stage){case S::ActivateReadBank:return uint8_t(h.read8(0x1679e));case S::ApplyReadEdit:return uint32_t(h.read32(0x1748e));case S::ApplyReadPatch:return uint32_t(h.read32(0x167bc));case S::ApplyReadIndex:return uint8_t(h.read8(t.table()+t.index()*2));case S::ApplyReadValue:return uint8_t(h.read8(t.table()+t.index()*2+1));case S::ApplyReadOld:return uint8_t(h.read8(t.target()));case S::RestoreReadPrevious:return uint8_t(h.read8(0x167a0));case S::RestoreReadSelected:return uint8_t(h.read8(0x1679e));case S::RestoreReadEdit:return uint32_t(h.read32(0x1748e));case S::RestoreReadPatch:return uint32_t(h.read32(0x167bc));case S::RestoreReadIndex:return uint8_t(h.read8(t.table()+t.index()*2));case S::RestoreReadEnabled:return uint8_t(h.read8(t.table()+t.index()*2+1));case S::RestoreReadOld:return uint8_t(h.read8(0x17495+t.index()));case S::BackupReadEdit:return uint32_t(h.read32(0x1748e));case S::BackupRead:return uint8_t(h.read8(t.table()+t.index()));case S::CopyReadEdit:return uint32_t(h.read32(0x1748e));case S::CopyRead:return uint8_t(h.read8((0x1734e)+t.index()));default:throw std::runtime_error("Invalid edit scene read");}};auto capture=[&](S s){return readData=read(s);};auto write=[&](S stage,uint32_t data){switch(stage){case S::ActivatePublishPrevious:h.write8(int8_t(data),0x167a0);break;case S::ApplyPublishValue:h.write8(int8_t(data),t.target());break;case S::ApplyPublishOld:h.write8(int8_t(data),0x17495+t.index());break;case S::RestorePublishSelected:h.write8(int8_t(data),0x1679e);break;case S::RestorePublishValue:h.write8(int8_t(data),t.target());break;case S::RestorePublishSelection:h.write8(int8_t(data),0x1679e);break;case S::RestorePublishPrevious:h.write8(int8_t(data),0x167a0);break;case S::BackupPublish:h.write8(int8_t(data),(0x1734e)+t.index());break;case S::CopyPublish:h.write8(int8_t(data),t.table()+t.index());break;default:throw std::runtime_error("Invalid edit scene write");}};if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("Edit scene deadline mismatch");switch(stage){
case S::ActivateReadBank:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::ActivatePublishPrevious:{value(h,v[0].rl);break;}
case S::ActivateAdmitBank:{;break;}
case S::ActivateLoad:{clocks.call(0xb674);if(t.child()==C::Apply&&t.savePrevious()!=(v[5].rl!=0))throw std::runtime_error("Typed edit scene save request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit scene call timing");return true;break;}
case S::ActivateSave:{clocks.move(v[5].rl,0x1);break;}
case S::ActivateApply:{clocks.call(0xb79e);if(t.child()==C::Apply&&t.savePrevious()!=(v[5].rl!=0))throw std::runtime_error("Typed edit scene save request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit scene call timing");return true;break;}
case S::ActivateFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit scene return timing");selected->reset();++completed;return true;break;}
case S::ApplyReadEdit:{clocks.move(v[0].er,int32_t(readData));break;}
case S::ApplySkipHeader:{v[0].er=h.add32(v[0].er,0x10);break;}
case S::ApplyReadPatch:{clocks.move(v[1].er,int32_t(readData));break;}
case S::ApplyBindSaved:{clocks.move(v[4].er,0x17495);break;}
case S::ApplyCount:{clocks.move(v[3].rl,0x10);break;}
case S::ApplyClearIndex:{v[2].er=h.sub32(v[2].er,v[2].er);break;}
case S::ApplyReadIndex:{clocks.move(v[2].rl,int8_t(readData));v[0].er+=1;h.clockI(2);break;}
case S::ApplyReadValue:{clocks.move(v[3].rh,int8_t(readData));v[0].er+=1;h.clockI(2);break;}
case S::ApplyAdmitValue:{;break;}
case S::ApplyDecodeValue:{clocks.inc(v[3].rh,-1);break;}
case S::ApplyLocateParameter:{v[2].er=h.add32(v[2].er,v[1].er);break;}
case S::ApplyReadOld:{clocks.move(v[5].rh,int8_t(readData));break;}
case S::ApplyPublishValue:{value(h,v[3].rh);break;}
case S::ApplyObserveSave:{clocks.move(v[5].rl,v[5].rl);break;}
case S::ApplyAdmitSave:{;break;}
case S::ApplyPublishOld:{value(h,v[5].rh);break;}
case S::ApplyAdvanceSaved:{v[4].er=int32_t(uint32_t(v[4].er)+0x1);break;}
case S::ApplyTick:{clocks.inc(v[3].rl,-1);break;}
case S::ApplyAdmit:{;break;}
case S::ApplyNotify:{clocks.call(0x7354);if(t.child()==C::Apply&&t.savePrevious()!=(v[5].rl!=0))throw std::runtime_error("Typed edit scene save request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit scene call timing");return true;break;}
case S::ApplyFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit scene return timing");selected->reset();++completed;return true;break;}
case S::RestoreReadPrevious:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::RestoreAdmitPrevious:{;break;}
case S::RestoreReadSelected:{clocks.move(v[0].rh,int8_t(readData));break;}
case S::RestoreRetainSelection:{clocks.push(v[0].r);call.depth+=2;break;}
case S::RestorePublishSelected:{value(h,v[0].rl);break;}
case S::RestoreLoad:{clocks.call(0xb674);if(t.child()==C::Apply&&t.savePrevious()!=(v[5].rl!=0))throw std::runtime_error("Typed edit scene save request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit scene call timing");return true;break;}
case S::RestoreReadEdit:{clocks.move(v[0].er,int32_t(readData));break;}
case S::RestoreSkipHeader:{v[0].er=h.add32(v[0].er,0x10);break;}
case S::RestoreReadPatch:{clocks.move(v[1].er,int32_t(readData));break;}
case S::RestoreBindSaved:{clocks.move(v[4].er,0x17495);break;}
case S::RestoreCount:{clocks.move(v[3].rl,0x10);break;}
case S::RestoreClearIndex:{v[2].er=h.sub32(v[2].er,v[2].er);break;}
case S::RestoreReadIndex:{clocks.move(v[2].rl,int8_t(readData));v[0].er+=1;h.clockI(2);break;}
case S::RestoreReadEnabled:{clocks.move(v[3].rh,int8_t(readData));v[0].er+=1;h.clockI(2);break;}
case S::RestoreAdmitEnabled:{;break;}
case S::RestoreReadOld:{clocks.move(v[3].rh,int8_t(readData));break;}
case S::RestoreLocateParameter:{v[2].er=h.add32(v[2].er,v[1].er);break;}
case S::RestorePublishValue:{value(h,v[3].rh);break;}
case S::RestoreAdvanceSaved:{v[4].er=int32_t(uint32_t(v[4].er)+0x1);break;}
case S::RestoreTick:{clocks.inc(v[3].rl,-1);break;}
case S::RestoreAdmit:{;break;}
case S::RestoreReleaseSelection:{clocks.pop(v[0].r);call.depth-=2;break;}
case S::RestorePublishSelection:{value(h,v[0].rh);break;}
case S::RestoreClearPrevious:{clocks.move(v[0].rl,0x0);break;}
case S::RestorePublishPrevious:{value(h,v[0].rl);break;}
case S::RestoreFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit scene return timing");selected->reset();++completed;return true;break;}
case S::BackupReadEdit:{clocks.move(v[0].er,int32_t(readData));break;}
case S::BackupBindSaved:{clocks.move(v[1].er,0x1734e);break;}
case S::BackupCount:{clocks.move(v[2].e,0x40);break;}
case S::BackupRead:{clocks.move(v[2].rl,int8_t(readData));v[0].er+=1;h.clockI(2);break;}
case S::BackupPublish:{value(h,v[2].rl);break;}
case S::BackupAdvance:{clocks.inc(v[1].er,1);break;}
case S::BackupTick:{clocks.inc(v[2].e,-1);break;}
case S::BackupAdmit:{;break;}
case S::BackupFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit scene return timing");selected->reset();++completed;return true;break;}
case S::CopyReadEdit:{clocks.move(v[0].er,int32_t(readData));break;}
case S::CopyBindSaved:{clocks.move(v[1].er,0x1734e);break;}
case S::CopyCount:{clocks.move(v[2].e,0x40);break;}
case S::CopyRead:{clocks.move(v[2].rl,int8_t(readData));v[1].er+=1;h.clockI(2);break;}
case S::CopyPublish:{value(h,v[2].rl);break;}
case S::CopyAdvance:{clocks.inc(v[0].er,1);break;}
case S::CopyTick:{clocks.inc(v[2].e,-1);break;}
case S::CopyAdmit:{;break;}
case S::CopyFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit scene return timing");selected->reset();++completed;return true;break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("Edit scene bus timing");h.pc=h.makepc(point(t).location);return true;}}
