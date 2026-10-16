#include "ksInternalNoteCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace{InternalNotePartialTest partialTest=nullptr;using T=InternalNoteTask;using K=T::Kind;using S=T::Stage;using C=T::Child;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){case S::OnReadNote:return {0xa882,2};case S::OnReadVelocity:return {0xa884,2};case S::OnAdmitVelocity:return {0xa886,2};case S::OnRetainNote:return {0xa888,2};case S::OnBindPart:return {0xa88a,4};case S::OnSetBitmap:return {0xa88e,2};case S::OnHandoff:return {0xa890,4};case S::OffReadNote:return {0xa894,2};case S::OffReadVelocity:return {0xa896,2};case S::OffBindPart:return {0xa898,4};case S::OffClearBitmap:return {0xa89c,2};case S::OffHandoff:return {0xa89e,4};case S::SetClearGroup:return {0xa8a2,2};case S::SetCaptureNote:return {0xa8a4,2};case S::SetQuarterGroup:return {0xa8a6,2};case S::SetGroup:return {0xa8a8,2};case S::SetBindGroup:return {0xa8aa,2};case S::SetLocateGroup:return {0xa8ac,6};case S::SetModify:return {0xa8b2,4};case S::SetFinish:return {0xa8b6,2};case S::ClearClearGroup:return {0xa8b8,2};case S::ClearCaptureNote:return {0xa8ba,2};case S::ClearQuarterGroup:return {0xa8bc,2};case S::ClearGroup:return {0xa8be,2};case S::ClearBindGroup:return {0xa8c0,2};case S::ClearLocateGroup:return {0xa8c2,6};case S::ClearModify:return {0xa8c8,4};case S::ClearFinish:return {0xa8cc,2};case S::EmptyBindPart:return {0xa8ce,4};case S::EmptyBindData:return {0xa8d2,2};case S::EmptyLocateData:return {0xa8d4,6};case S::EmptySetCount:return {0xa8da,4};case S::EmptyPublish:return {0xa8de,2};case S::EmptyTickCount:return {0xa8e0,2};case S::EmptyAdmitCount:return {0xa8e2,2};case S::EmptyFinish:return {0xa8e4,2};case S::ReleaseBindPart:return {0xa8e6,4};case S::ReleaseReadPolicy:return {0xa8ea,8};case S::ReleaseMaskPolicy:return {0xa8f2,2};case S::ReleaseAdmitPolicy:return {0xa8f4,2};case S::ReleaseBindData:return {0xa8f6,2};case S::ReleaseLocateData:return {0xa8f8,6};case S::ReleaseSetCount:return {0xa8fe,2};case S::ReleaseSetVelocity:return {0xa900,2};case S::ReleaseClearGroup:return {0xa902,2};case S::ReleaseReadByte:return {0xa904,2};case S::ReleaseAdmitByte:return {0xa906,2};case S::ReleaseSetBits:return {0xa908,2};case S::ReleaseCaptureNote:return {0xa90a,2};case S::ReleaseShiftByte:return {0xa90c,2};case S::ReleaseAdmitBit:return {0xa90e,2};case S::ReleaseMidi:return {0xa910,4};case S::ReleaseTickNote:return {0xa914,2};case S::ReleaseTickBit:return {0xa916,2};case S::ReleaseAdmitBits:return {0xa918,2};case S::ReleaseNextGroup:return {0xa91a,2};case S::ReleaseTickCount:return {0xa91c,2};case S::ReleaseAdmitCount:return {0xa91e,2};case S::ReleaseClearAll:return {0xa920,2};case S::ReleaseFinish:return {0xa922,2};case S::HandoffOn:return {0x3f48,0};case S::HandoffOff:return {0x3bc6,0};default:throw std::runtime_error("Invalid internal note stage");}}uint8_t binding(uint32_t a){if(a<0x160d2||a>=0x160d2 + 256*334||(a-0x160d2)%334)throw std::runtime_error("Invalid internal note part");return uint8_t((a-0x160d2)/334);}}
void setInternalNotePartialForTests(InternalNotePartialTest fn){partialTest=fn;}
bool InternalNoteCalls::step(ControllerCpu& h){using namespace controllerDetail;const auto pc=unsigned(h.getPC());auto& v=h.regs;std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}const bool root=pc==0xa882||pc==0xa894||pc==0xa8a2||pc==0xa8b8||pc==0xa8ce||pc==0xa8e6;
 if(root&&(pc==0xa8a2||pc==0xa8b8)&&partialTest){const auto a=uint32_t(v[0].er);if(a<0x160d2||a>=0x160d2 + 256*334||(a-0x160d2)%334){if(partialTest(h)){++partialEntries;return true;}}}
 if(root){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Internal note capacity exceeded");const auto kind=pc==0xa882?K::On:pc==0xa894?K::Off:pc==0xa8a2?K::Set:pc==0xa8b8?K::Clear:pc==0xa8ce?K::Empty:K::Release;selected->emplace(kind,(kind==K::Set||kind==K::Clear)?binding(uint32_t(v[0].er)):0,uint8_t(v[5].rl),uint32_t(v[4].er),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& t=call.task;if(t.awaitingChild()){if(t.child()==C::Part)t.resumePart(binding(uint32_t(v[0].er)));else t.resumeChild();}
 const auto stage=t.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(t).length);const auto base=0x160d2 + unsigned(t.part())*334;uint32_t readData=0;
 auto publicationAddress=[&](S stage)->unsigned{switch(stage){case S::SetModify:return base+224+t.group();case S::ClearModify:return base+224+t.group();case S::EmptyPublish:return base+224+t.cursor()-1;default:throw std::runtime_error("Invalid internal note publication");}};auto timing=[&](S stage){const auto at=publicationAddress(stage);return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=t.nextClocks(t.publication()?timing(stage):3);
 auto read=[&](S stage)->uint32_t{switch(stage){case S::OnReadNote:return uint8_t(h.read8(t.payload()));case S::OnReadVelocity:return uint8_t(h.read8(t.payload()));case S::OffReadNote:return uint8_t(h.read8(t.payload()));case S::OffReadVelocity:return uint8_t(h.read8(t.payload()));case S::SetModify:return uint8_t(h.read8(base+224+t.group()));case S::ClearModify:return uint8_t(h.read8(base+224+t.group()));case S::ReleaseReadPolicy:return uint8_t(h.read8(base+327));case S::ReleaseReadByte:return uint8_t(h.read8(base+224+t.cursor()));default:throw std::runtime_error("Invalid internal note read");}};auto capture=[&](S stage){return readData=read(stage);};auto write=[&](S stage,uint32_t data){switch(stage){case S::SetModify:h.write8(int8_t(data),base+224+t.group());break;case S::ClearModify:h.write8(int8_t(data),base+224+t.group());break;case S::EmptyPublish:h.write8(int8_t(data),base+224+t.cursor());break;default:throw std::runtime_error("Invalid internal note write");}};if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("Internal note deadline mismatch");switch(stage){
case S::OnReadNote:{clocks.move(v[5].rl,int8_t(readData));v[4].er+=1;h.clockI(2);break;}
case S::OnReadVelocity:{clocks.move(v[5].rh,int8_t(readData));break;}
case S::OnAdmitVelocity:{;break;}
case S::OnRetainNote:{clocks.push(v[5].r);call.depth+=2;break;}
case S::OnBindPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note dispatch timing");return true;break;}
case S::OnSetBitmap:{clocks.call(0xa8a2);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note dispatch timing");return true;break;}
case S::OnHandoff:{h.clockI(2);break;}
case S::OffReadNote:{clocks.move(v[5].rl,int8_t(readData));v[4].er+=1;h.clockI(2);break;}
case S::OffReadVelocity:{clocks.move(v[5].rh,int8_t(readData));break;}
case S::OffBindPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note dispatch timing");return true;break;}
case S::OffClearBitmap:{clocks.call(0xa8b8);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note dispatch timing");return true;break;}
case S::OffHandoff:{h.clockI(2);break;}
case S::SetClearGroup:{v[1].er=h.sub32(v[1].er,v[1].er);break;}
case S::SetCaptureNote:{clocks.move(v[1].rl,v[5].rl);break;}
case S::SetQuarterGroup:{clocks.shift(v[1].rl,2,false);break;}
case S::SetGroup:{clocks.shift(v[1].rl,1,false);break;}
case S::SetBindGroup:{v[1].er=h.add32(v[1].er,v[0].er);break;}
case S::SetLocateGroup:{v[1].er=h.add32(v[1].er,0xe0);break;}
case S::SetModify:{;break;}
case S::SetFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note return timing");selected->reset();++completed;return true;break;}
case S::ClearClearGroup:{v[1].er=h.sub32(v[1].er,v[1].er);break;}
case S::ClearCaptureNote:{clocks.move(v[1].rl,v[5].rl);break;}
case S::ClearQuarterGroup:{clocks.shift(v[1].rl,2,false);break;}
case S::ClearGroup:{clocks.shift(v[1].rl,1,false);break;}
case S::ClearBindGroup:{v[1].er=h.add32(v[1].er,v[0].er);break;}
case S::ClearLocateGroup:{v[1].er=h.add32(v[1].er,0xe0);break;}
case S::ClearModify:{;break;}
case S::ClearFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note return timing");selected->reset();++completed;return true;break;}
case S::EmptyBindPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note dispatch timing");return true;break;}
case S::EmptyBindData:{clocks.move(v[1].er,v[0].er);break;}
case S::EmptyLocateData:{v[1].er=h.add32(v[1].er,0xf0);break;}
case S::EmptySetCount:{clocks.move(v[2].r,0x10);break;}
case S::EmptyPublish:{v[1].er-=1;value(h,v[2].rh);h.clockI(2);break;}
case S::EmptyTickCount:{clocks.inc(v[2].rl,-1);break;}
case S::EmptyAdmitCount:{;break;}
case S::EmptyFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note return timing");selected->reset();++completed;return true;break;}
case S::ReleaseBindPart:{clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note dispatch timing");return true;break;}
case S::ReleaseReadPolicy:{clocks.move(v[1].rl,int8_t(readData));break;}
case S::ReleaseMaskPolicy:{v[1].rl&=0x60;value(h,v[1].rl);break;}
case S::ReleaseAdmitPolicy:{;break;}
case S::ReleaseBindData:{clocks.move(v[1].er,v[0].er);break;}
case S::ReleaseLocateData:{v[1].er=h.add32(v[1].er,0xe0);break;}
case S::ReleaseSetCount:{clocks.move(v[2].rl,0x10);break;}
case S::ReleaseSetVelocity:{clocks.move(v[5].rh,0x40);break;}
case S::ReleaseClearGroup:{clocks.move(v[3].rl,0x0);break;}
case S::ReleaseReadByte:{clocks.move(v[3].rh,int8_t(readData));v[1].er+=1;h.clockI(2);break;}
case S::ReleaseAdmitByte:{;break;}
case S::ReleaseSetBits:{clocks.move(v[2].rh,0x8);break;}
case S::ReleaseCaptureNote:{clocks.move(v[5].rl,v[3].rl);break;}
case S::ReleaseShiftByte:{clocks.shift(v[3].rh,1,false);break;}
case S::ReleaseAdmitBit:{;break;}
case S::ReleaseMidi:{clocks.call(0x2a58);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note dispatch timing");return true;break;}
case S::ReleaseTickNote:{clocks.inc(v[5].rl,1);break;}
case S::ReleaseTickBit:{clocks.inc(v[2].rh,-1);break;}
case S::ReleaseAdmitBits:{;break;}
case S::ReleaseNextGroup:{v[3].rl=h.add8(v[3].rl,0x8);break;}
case S::ReleaseTickCount:{clocks.inc(v[2].rl,-1);break;}
case S::ReleaseAdmitCount:{;break;}
case S::ReleaseClearAll:{clocks.call(0xa8ce);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note dispatch timing");return true;break;}
case S::ReleaseFinish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Internal note return timing");selected->reset();++completed;return true;break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("Internal note bus timing");h.pc=h.makepc(point(t).location);if(t.handoff()){selected->reset();++handedOff;}return true;}}
