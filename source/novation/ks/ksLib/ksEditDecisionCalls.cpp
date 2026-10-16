#include "ksEditDecisionCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks{namespace{EditDecisionPartialTest partialTest=nullptr;using T=EditDecisionTask;using K=T::Kind;using S=T::Stage;using C=T::Child;struct Point{unsigned location,length;};Point point(const T&t){switch(t.stage()){case S::KnobReadOld:return{0xb554,6};case S::KnobDifference:return{0xb55a,2};case S::KnobAdmitSign:return{0xb55c,2};case S::KnobMagnitude:return{0xb55e,2};case S::KnobCompareThreshold:return{0xb560,2};case S::KnobAdmitThreshold:return{0xb562,4};case S::KnobPublishInput:return{0xb566,6};case S::KnobReadLock:return{0xb56c,6};case S::KnobAdmitLock:return{0xb572,2};case S::KnobScale:return{0xb574,2};case S::KnobMultiply:return{0xb576,2};case S::KnobReadMode:return{0xb578,6};case S::KnobAdmitMode:return{0xb57e,2};case S::KnobObserveBank:return{0xb580,2};case S::KnobAdmitBank:return{0xb582,2};case S::KnobPublishZeroBank:return{0xb584,6};case S::KnobPublishZeroMode:return{0xb58a,6};case S::KnobRefresh:return{0xb590,4};case S::SelectReadOldBank:return{0xb594,6};case S::SelectPublishBank:return{0xb59a,6};case S::SelectCompareBank:return{0xb5a0,2};case S::SelectAdmitChange:return{0xb5a2,2};case S::SelectObserveNewBank:return{0xb5a4,2};case S::SelectAdmitNonzero:return{0xb5a6,2};case S::SelectPublishMode:return{0xb5a8,6};case S::SelectRestore:return{0xb5ae,4};case S::SelectNotify:return{0xb5b2,4};case S::SelectSkipSelect:return{0xb5b6,2};case S::SelectObserveOldBank:return{0xb5b8,2};case S::SelectAdmitOldBank:return{0xb5ba,2};case S::SelectClearRow:return{0xb5bc,6};case S::SelectClearField:return{0xb5c2,6};case S::SelectMap:return{0xb5c8,4};case S::SelectReadMode:return{0xb5cc,6};case S::SelectAdmitMode:return{0xb5d2,4};case S::SelectRestoreScene:return{0xb5d6,4};case S::SelectActivateScene:return{0xb5da,4};case S::SelectRefresh:return{0xb5de,4};case S::SelectFinish:return{0xb5e2,2};case S::IncrementReadMode:return{0xb5e4,6};case S::IncrementAdmitMode:return{0xb5ea,2};case S::IncrementCompareText:return{0xb5ec,2};case S::IncrementAdmitText:return{0xb5ee,4};case S::IncrementFinish:return{0xb5f2,2};case S::IncrementReadRow:return{0xb5f4,6};case S::IncrementRow:return{0xb5fa,2};case S::IncrementCompareRow:return{0xb5fc,2};case S::IncrementAdmitRow:return{0xb5fe,2};case S::IncrementClamp:return{0xb600,2};case S::IncrementPublishRow:return{0xb602,6};case S::IncrementRefresh:return{0xb608,4};case S::DecrementReadMode:return{0xb60c,6};case S::DecrementAdmitMode:return{0xb612,2};case S::DecrementCompareText:return{0xb614,2};case S::DecrementAdmitText:return{0xb616,4};case S::DecrementFinish:return{0xb61a,2};case S::DecrementReadRow:return{0xb61c,6};case S::DecrementRow:return{0xb622,2};case S::DecrementAdmitRow:return{0xb624,2};case S::DecrementClamp:return{0xb626,2};case S::DecrementPublishRow:return{0xb628,6};case S::DecrementRefresh:return{0xb62e,4};case S::SubReadMode:return{0xb632,6};case S::SubAdmitMode:return{0xb638,2};case S::SubCompareText:return{0xb63a,2};case S::SubAdmitText:return{0xb63c,4};case S::SubFinish:return{0xb640,2};case S::SubReadField:return{0xb642,6};case S::SubAdd:return{0xb648,2};case S::SubAdmitNegative:return{0xb64a,2};case S::SubLast:return{0xb64c,2};case S::SubCompare:return{0xb64e,2};case S::SubAdmitRange:return{0xb650,2};case S::SubFirst:return{0xb652,2};case S::SubPublish:return{0xb654,6};case S::SubMap:return{0xb65a,2};case S::SubRefresh:return{0xb65c,4};case S::AdjustReadMode:return{0xb716,6};case S::AdjustAdmitMode:return{0xb71c,2};case S::AdjustCompareText:return{0xb71e,2};case S::AdjustAdmitText:return{0xb720,4};case S::AdjustClamp:return{0xb724,2};case S::AdjustPublishBank:return{0xb726,6};case S::AdjustPreviousBank:return{0xb72c,2};case S::AdjustStride:return{0xb72e,2};case S::AdjustScale:return{0xb730,2};case S::AdjustNormalize:return{0xb732,2};case S::AdjustBase:return{0xb734,6};case S::AdjustBindPreview:return{0xb73a,6};case S::AdjustCount:return{0xb740,4};case S::AdjustReadPreview:return{0xb744,2};case S::AdjustPublishPreview:return{0xb746,2};case S::AdjustAdvancePreview:return{0xb748,2};case S::AdjustTickPreview:return{0xb74a,2};case S::AdjustAdmitPreview:return{0xb74c,2};case S::AdjustRefresh:return{0xb74e,4};case S::AdjustReadRow:return{0xb752,6};case S::AdjustAdmitRow:return{0xb758,2};case S::AdjustSelectClamp:return{0xb75a,2};case S::AdjustSelectTail:return{0xb75c,4};case S::AdjustField:return{0xb760,4};case S::AdjustReadValue:return{0xb764,2};case S::AdjustNormalizeValue:return{0xb766,2};case S::AdjustSignDelta:return{0xb768,2};case S::AdjustAddDelta:return{0xb76a,2};case S::AdjustAdmitNegative:return{0xb76c,2};case S::AdjustZero:return{0xb76e,2};case S::AdjustSkipLimit:return{0xb770,2};case S::AdjustReadLimit:return{0xb772,2};case S::AdjustNormalizeLimit:return{0xb774,2};case S::AdjustCompareLimit:return{0xb776,2};case S::AdjustAdmitLimit:return{0xb778,2};case S::AdjustMaximum:return{0xb77a,2};case S::AdjustPublishValue:return{0xb77c,2};case S::AdjustNoSnapshot:return{0xb77e,2};case S::AdjustApply:return{0xb780,2};case S::AdjustValueRefresh:return{0xb782,4};case S::ToggleReadBank:return{0xb83c,6};case S::ToggleAdmitBank:return{0xb842,2};case S::TogglePublishZeroMode:return{0xb844,6};case S::ToggleZeroFinish:return{0xb84a,2};case S::ToggleReadMode:return{0xb84c,6};case S::ToggleAdmitMode:return{0xb852,2};case S::TogglePreviousMode:return{0xb854,2};case S::ToggleAdmitTextMode:return{0xb856,2};case S::ToggleSkipEnable:return{0xb858,2};case S::ToggleCheckAvailable:return{0xb85a,4};case S::ToggleAdmitAvailable:return{0xb85e,2};case S::ToggleModeOne:return{0xb860,2};case S::TogglePublishOne:return{0xb862,6};case S::ToggleZeroRow:return{0xb868,2};case S::TogglePublishRow:return{0xb86a,6};case S::ToggleBackup:return{0xb870,2};case S::ToggleSkipText:return{0xb872,2};case S::ToggleUnavailableFinish:return{0xb874,2};case S::ToggleModeTwo:return{0xb876,2};case S::TogglePublishTwo:return{0xb878,6};case S::ToggleText:return{0xb87e,4};case S::ToggleRefresh:return{0xb882,4};case S::ToggleLoad:return{0xb886,4};case S::ToggleCopy:return{0xb88a,2};case S::ToggleRestoreParameters:return{0xb88c,4};case S::ToggleCommit:return{0xb890,4};case S::ToggleZero:return{0xb894,2};case S::TogglePublishZero:return{0xb896,6};case S::TogglePublishZeroBank:return{0xb89c,6};case S::ToggleMessage:return{0xb8a2,6};case S::ToggleNotify:return{0xb8a8,4};default:throw std::runtime_error("Invalid edit decision stage");}}}void setEditDecisionPartialForTests(EditDecisionPartialTest f){partialTest=f;}
bool EditDecisionCalls::step(ControllerCpu&h){using namespace controllerDetail;auto pc=unsigned(h.getPC());auto&v=h.regs;std::optional<Call>*selected=nullptr;for(auto&c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}std::optional<K>kind;switch(pc){case 0xb554:kind=K::Knob;break;case 0xb594:kind=K::Select;break;case 0xb5e4:kind=K::Increment;break;case 0xb60c:kind=K::Decrement;break;case 0xb632:kind=K::Sub;break;case 0xb716:kind=K::Adjust;break;case 0xb83c:kind=K::Toggle;break;default:break;}if(kind&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto&c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Edit decision capacity exceeded");selected->emplace(*kind,uint8_t(v[6].rh),uint8_t(v[6].rl),uint8_t(v[0].rh),h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto&t=(**selected).task;if(t.awaitingChild()){switch(t.child()){case C::Clamp:t.resumeBank(uint8_t(v[0].rh));break;case C::Field:t.resumeField(uint32_t(v[0].er),uint32_t(v[2].er));break;case C::Available:t.resumeAvailable(!(h.ccr&4));break;default:t.resumeChild();break;}}auto stage=t.stage();auto begin=h.cycles;RuntimeBoundary clocks(h,pc,point(t).length);uint32_t readData=0;auto timing=[&](S stage){uint32_t at=0;unsigned width=0;bool store=false;switch(stage){case S::KnobReadOld:at=0x1679d;width=1;store=false;break;case S::KnobReadLock:at=0x16790;width=1;store=false;break;case S::KnobReadMode:at=0x1679f;width=1;store=false;break;case S::SelectReadOldBank:at=0x1679e;width=1;store=false;break;case S::SelectReadMode:at=0x1679f;width=1;store=false;break;case S::IncrementReadMode:at=0x1679f;width=1;store=false;break;case S::IncrementReadRow:at=0x17492;width=1;store=false;break;case S::DecrementReadMode:at=0x1679f;width=1;store=false;break;case S::DecrementReadRow:at=0x17492;width=1;store=false;break;case S::SubReadMode:at=0x1679f;width=1;store=false;break;case S::SubReadField:at=0x17493;width=1;store=false;break;case S::AdjustReadMode:at=0x1679f;width=1;store=false;break;case S::AdjustReadPreview:at=t.preview()+t.index();width=1;store=false;break;case S::AdjustReadRow:at=0x17492;width=1;store=false;break;case S::AdjustReadValue:at=t.field();width=1;store=false;break;case S::AdjustReadLimit:at=t.limit();width=1;store=false;break;case S::ToggleReadBank:at=0x1679e;width=1;store=false;break;case S::ToggleReadMode:at=0x1679f;width=1;store=false;break;case S::KnobPublishInput:at=0x1679d;width=1;store=true;break;case S::KnobPublishZeroBank:at=0x1679e;width=1;store=true;break;case S::KnobPublishZeroMode:at=0x1679f;width=1;store=true;break;case S::SelectPublishBank:at=0x1679e;width=1;store=true;break;case S::SelectPublishMode:at=0x1679f;width=1;store=true;break;case S::SelectClearRow:at=0x17492;width=1;store=true;break;case S::SelectClearField:at=0x17493;width=1;store=true;break;case S::IncrementPublishRow:at=0x17492;width=1;store=true;break;case S::DecrementPublishRow:at=0x17492;width=1;store=true;break;case S::SubPublish:at=0x17493;width=1;store=true;break;case S::AdjustPublishBank:at=0x1679e;width=1;store=true;break;case S::AdjustPublishPreview:at=0x1733c+t.index();width=1;store=true;break;case S::AdjustPublishValue:at=t.field();width=1;store=true;break;case S::TogglePublishZeroMode:at=0x1679f;width=1;store=true;break;case S::TogglePublishOne:at=0x1679f;width=1;store=true;break;case S::TogglePublishRow:at=0x17492;width=1;store=true;break;case S::TogglePublishTwo:at=0x1679f;width=1;store=true;break;case S::TogglePublishZero:at=0x1679f;width=1;store=true;break;case S::TogglePublishZeroBank:at=0x1679e;width=1;store=true;break;default:return 0u;}int last=store?h.lastwrite:h.lastread;unsigned cost=0;for(unsigned i=0;i<width;++i){auto addr=(at+i)&0xffffff;cost+=addr>=0x400000||(last&1)||addr!=(unsigned(last)|1u)?3u:0u;last=int(addr);}return cost;};auto cost=t.nextClocks(timing(stage));auto read=[&](S stage)->uint32_t{switch(stage){case S::KnobReadOld:return uint8_t(h.read8(0x1679d));case S::KnobReadLock:return uint8_t(h.read8(0x16790));case S::KnobReadMode:return uint8_t(h.read8(0x1679f));case S::SelectReadOldBank:return uint8_t(h.read8(0x1679e));case S::SelectReadMode:return uint8_t(h.read8(0x1679f));case S::IncrementReadMode:return uint8_t(h.read8(0x1679f));case S::IncrementReadRow:return uint8_t(h.read8(0x17492));case S::DecrementReadMode:return uint8_t(h.read8(0x1679f));case S::DecrementReadRow:return uint8_t(h.read8(0x17492));case S::SubReadMode:return uint8_t(h.read8(0x1679f));case S::SubReadField:return uint8_t(h.read8(0x17493));case S::AdjustReadMode:return uint8_t(h.read8(0x1679f));case S::AdjustReadPreview:return uint8_t(h.read8(t.preview()+t.index()));case S::AdjustReadRow:return uint8_t(h.read8(0x17492));case S::AdjustReadValue:return uint8_t(h.read8(t.field()));case S::AdjustReadLimit:return uint8_t(h.read8(t.limit()));case S::ToggleReadBank:return uint8_t(h.read8(0x1679e));case S::ToggleReadMode:return uint8_t(h.read8(0x1679f));default:throw std::runtime_error("Invalid edit decision read");}};auto capture=[&](S s){return readData=read(s);};auto write=[&](S stage,uint32_t data){switch(stage){case S::KnobPublishInput:h.write8(int8_t(data),0x1679d);break;case S::KnobPublishZeroBank:h.write8(int8_t(data),0x1679e);break;case S::KnobPublishZeroMode:h.write8(int8_t(data),0x1679f);break;case S::SelectPublishBank:h.write8(int8_t(data),0x1679e);break;case S::SelectPublishMode:h.write8(int8_t(data),0x1679f);break;case S::SelectClearRow:h.write8(int8_t(data),0x17492);break;case S::SelectClearField:h.write8(int8_t(data),0x17493);break;case S::IncrementPublishRow:h.write8(int8_t(data),0x17492);break;case S::DecrementPublishRow:h.write8(int8_t(data),0x17492);break;case S::SubPublish:h.write8(int8_t(data),0x17493);break;case S::AdjustPublishBank:h.write8(int8_t(data),0x1679e);break;case S::AdjustPublishPreview:h.write8(int8_t(data),0x1733c+t.index());break;case S::AdjustPublishValue:h.write8(int8_t(data),t.field());break;case S::TogglePublishZeroMode:h.write8(int8_t(data),0x1679f);break;case S::TogglePublishOne:h.write8(int8_t(data),0x1679f);break;case S::TogglePublishRow:h.write8(int8_t(data),0x17492);break;case S::TogglePublishTwo:h.write8(int8_t(data),0x1679f);break;case S::TogglePublishZero:h.write8(int8_t(data),0x1679f);break;case S::TogglePublishZeroBank:h.write8(int8_t(data),0x1679e);break;default:throw std::runtime_error("Invalid edit decision write");}};if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("Edit decision deadline mismatch");switch(stage){
case S::KnobReadOld:{clocks.move(v[6].rl,int8_t(readData));break;}
case S::KnobDifference:{v[6].rl=h.sub8(v[6].rl,v[6].rh);break;}
case S::KnobAdmitSign:{;break;}
case S::KnobMagnitude:{v[6].rl=h.sub8(0,v[6].rl);break;}
case S::KnobCompareThreshold:{h.sub8(v[6].rl,0x4);break;}
case S::KnobAdmitThreshold:{h.clockI(2);break;}
case S::KnobPublishInput:{value(h,v[6].rh);break;}
case S::KnobReadLock:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::KnobAdmitLock:{;break;}
case S::KnobScale:{clocks.move(v[0].rl,0x22);break;}
case S::KnobMultiply:{v[0].r=uint8_t(v[0].r)*uint32_t(uint8_t(v[6].rh));h.clockI(12);break;}
case S::KnobReadMode:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::KnobAdmitMode:{;break;}
case S::KnobObserveBank:{clocks.move(v[0].rh,v[0].rh);break;}
case S::KnobAdmitBank:{;break;}
case S::KnobPublishZeroBank:{value(h,v[0].rh);break;}
case S::KnobPublishZeroMode:{value(h,v[0].rh);break;}
case S::KnobRefresh:{h.clockI(2);break;}
case S::SelectReadOldBank:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::SelectPublishBank:{value(h,v[0].rh);break;}
case S::SelectCompareBank:{h.sub8(v[0].rh,v[0].rl);break;}
case S::SelectAdmitChange:{;break;}
case S::SelectObserveNewBank:{clocks.move(v[0].rh,v[0].rh);break;}
case S::SelectAdmitNonzero:{;break;}
case S::SelectPublishMode:{value(h,v[0].rh);break;}
case S::SelectRestore:{clocks.call(0xb7de);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::SelectNotify:{clocks.call(0x7354);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::SelectSkipSelect:{;break;}
case S::SelectObserveOldBank:{clocks.move(v[0].rl,v[0].rl);break;}
case S::SelectAdmitOldBank:{;break;}
case S::SelectClearRow:{value(h,v[0].rl);break;}
case S::SelectClearField:{value(h,v[0].rl);break;}
case S::SelectMap:{clocks.call(0xb660);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::SelectReadMode:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::SelectAdmitMode:{h.clockI(2);break;}
case S::SelectRestoreScene:{clocks.call(0xb7de);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::SelectActivateScene:{clocks.call(0xb786);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::SelectRefresh:{h.clockI(2);break;}
case S::SelectFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision return timing");selected->reset();++completed;return true;break;}
case S::IncrementReadMode:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::IncrementAdmitMode:{;break;}
case S::IncrementCompareText:{h.sub8(v[0].rl,0x2);break;}
case S::IncrementAdmitText:{h.clockI(2);break;}
case S::IncrementFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision return timing");selected->reset();++completed;return true;break;}
case S::IncrementReadRow:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::IncrementRow:{clocks.inc(v[0].rl,1);break;}
case S::IncrementCompareRow:{h.sub8(v[0].rl,0x5);break;}
case S::IncrementAdmitRow:{;break;}
case S::IncrementClamp:{clocks.move(v[0].rl,0x4);break;}
case S::IncrementPublishRow:{value(h,v[0].rl);break;}
case S::IncrementRefresh:{h.clockI(2);break;}
case S::DecrementReadMode:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::DecrementAdmitMode:{;break;}
case S::DecrementCompareText:{h.sub8(v[0].rl,0x2);break;}
case S::DecrementAdmitText:{h.clockI(2);break;}
case S::DecrementFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision return timing");selected->reset();++completed;return true;break;}
case S::DecrementReadRow:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::DecrementRow:{clocks.inc(v[0].rl,-1);break;}
case S::DecrementAdmitRow:{;break;}
case S::DecrementClamp:{clocks.move(v[0].rl,0x0);break;}
case S::DecrementPublishRow:{value(h,v[0].rl);break;}
case S::DecrementRefresh:{h.clockI(2);break;}
case S::SubReadMode:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::SubAdmitMode:{;break;}
case S::SubCompareText:{h.sub8(v[0].rl,0x2);break;}
case S::SubAdmitText:{h.clockI(2);break;}
case S::SubFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision return timing");selected->reset();++completed;return true;break;}
case S::SubReadField:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::SubAdd:{v[0].rl=h.add8(v[0].rl,v[6].rl);break;}
case S::SubAdmitNegative:{;break;}
case S::SubLast:{clocks.move(v[0].rl,0x3);break;}
case S::SubCompare:{h.sub8(v[0].rl,0x3);break;}
case S::SubAdmitRange:{;break;}
case S::SubFirst:{clocks.move(v[0].rl,0x0);break;}
case S::SubPublish:{value(h,v[0].rl);break;}
case S::SubMap:{clocks.call(0xb660);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::SubRefresh:{h.clockI(2);break;}
case S::AdjustReadMode:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::AdjustAdmitMode:{;break;}
case S::AdjustCompareText:{h.sub8(v[0].rl,0x2);break;}
case S::AdjustAdmitText:{h.clockI(2);break;}
case S::AdjustClamp:{clocks.call(0xb6fe);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::AdjustPublishBank:{value(h,v[0].rh);break;}
case S::AdjustPreviousBank:{clocks.inc(v[0].rh,-1);break;}
case S::AdjustStride:{clocks.move(v[0].rl,0x40);break;}
case S::AdjustScale:{v[0].r=uint8_t(v[0].r)*uint32_t(uint8_t(v[0].rh));h.clockI(12);break;}
case S::AdjustNormalize:{v[0].er&=0xffff;value(h,v[0].er);break;}
case S::AdjustBase:{v[0].er=h.add32(v[0].er,0x85ec00);break;}
case S::AdjustBindPreview:{clocks.move(v[1].er,0x1733c);break;}
case S::AdjustCount:{clocks.move(v[2].e,0x10);break;}
case S::AdjustReadPreview:{clocks.move(v[2].rl,int8_t(readData));v[0].er+=1;h.clockI(2);break;}
case S::AdjustPublishPreview:{value(h,v[2].rl);break;}
case S::AdjustAdvancePreview:{clocks.inc(v[1].er,1);break;}
case S::AdjustTickPreview:{clocks.inc(v[2].e,-1);break;}
case S::AdjustAdmitPreview:{;break;}
case S::AdjustRefresh:{h.clockI(2);break;}
case S::AdjustReadRow:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::AdjustAdmitRow:{;break;}
case S::AdjustSelectClamp:{clocks.call(0xb6fe);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::AdjustSelectTail:{h.clockI(2);break;}
case S::AdjustField:{clocks.call(0xb6e0);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::AdjustReadValue:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::AdjustNormalizeValue:{v[1].r&=0xff;value(h,v[1].r);break;}
case S::AdjustSignDelta:{v[6].r=int8_t(v[6].r);value(h,v[6].r);break;}
case S::AdjustAddDelta:{v[1].r=h.add16(v[1].r,v[6].r);break;}
case S::AdjustAdmitNegative:{;break;}
case S::AdjustZero:{clocks.move(v[1].rl,0x0);break;}
case S::AdjustSkipLimit:{;break;}
case S::AdjustReadLimit:{clocks.move(v[3].rl,int8_t(readData));break;}
case S::AdjustNormalizeLimit:{v[3].r&=0xff;value(h,v[3].r);break;}
case S::AdjustCompareLimit:{h.sub16(v[1].r,v[3].r);break;}
case S::AdjustAdmitLimit:{;break;}
case S::AdjustMaximum:{clocks.move(v[1].rl,v[3].rl);break;}
case S::AdjustPublishValue:{value(h,v[1].rl);break;}
case S::AdjustNoSnapshot:{clocks.move(v[5].rl,0x0);break;}
case S::AdjustApply:{clocks.call(0xb79e);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::AdjustValueRefresh:{h.clockI(2);break;}
case S::ToggleReadBank:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::ToggleAdmitBank:{;break;}
case S::TogglePublishZeroMode:{value(h,v[0].rl);break;}
case S::ToggleZeroFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision return timing");selected->reset();++completed;return true;break;}
case S::ToggleReadMode:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::ToggleAdmitMode:{;break;}
case S::TogglePreviousMode:{clocks.inc(v[0].rl,-1);break;}
case S::ToggleAdmitTextMode:{;break;}
case S::ToggleSkipEnable:{;break;}
case S::ToggleCheckAvailable:{clocks.call(0x66bc);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::ToggleAdmitAvailable:{;break;}
case S::ToggleModeOne:{clocks.move(v[0].rl,0x1);break;}
case S::TogglePublishOne:{value(h,v[0].rl);break;}
case S::ToggleZeroRow:{clocks.move(v[0].rl,0x0);break;}
case S::TogglePublishRow:{value(h,v[0].rl);break;}
case S::ToggleBackup:{clocks.call(0xb8ac);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::ToggleSkipText:{;break;}
case S::ToggleUnavailableFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision return timing");selected->reset();++completed;return true;break;}
case S::ToggleModeTwo:{clocks.move(v[0].rl,0x2);break;}
case S::TogglePublishTwo:{value(h,v[0].rl);break;}
case S::ToggleText:{clocks.call(0x756a);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::ToggleRefresh:{h.clockI(2);break;}
case S::ToggleLoad:{clocks.call(0xb674);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::ToggleCopy:{clocks.call(0xb8ca);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::ToggleRestoreParameters:{clocks.call(0x6eb0);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::ToggleCommit:{clocks.call(0xb696);if(t.child()==C::ApplyScene&&v[5].rl!=0)throw std::runtime_error("Typed edit nonsnapshot apply request differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision call timing");return true;break;}
case S::ToggleZero:{clocks.move(v[0].rl,0x0);break;}
case S::TogglePublishZero:{value(h,v[0].rl);break;}
case S::TogglePublishZeroBank:{value(h,v[0].rl);break;}
case S::ToggleMessage:{clocks.move(v[6].er,0x150fc);break;}
case S::ToggleNotify:{h.clockI(2);break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("Edit decision bus timing");if(t.handoff()){unsigned target=0;switch(t.route()){case T::Route::Refresh:target=0x5340;break;case T::Route::IncrementText:target=0x7548;break;case T::Route::DecrementText:target=0x75a0;break;case T::Route::SubText:target=0x757c;break;case T::Route::AdjustText:target=0x75be;break;case T::Route::SelectBank:if(t.bank()!=uint8_t(v[0].rh))throw std::runtime_error("Typed edit selection handoff differs");target=0xb594;break;case T::Route::NotifySaved:target=0x55b8;break;}h.pc=h.makepc(target);selected->reset();++handedOff;}else h.pc=h.makepc(point(t).location);return true;}}
