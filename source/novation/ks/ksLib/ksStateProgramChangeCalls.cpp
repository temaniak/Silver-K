#include "ksStateProgramChangeCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateProgramChangeTask;using S=T::Stage;StateProgramChangePartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,64> points{{{0x62b2,2},{0x62b4,2},{0x62b6,2},{0x62b8,2},{0x62ba,2},{0x62bc,6},{0x62c2,6},{0x62c8,2},{0x62ca,6},{0x62d0,2},{0x62d2,2},{0x62d4,4},{0x62d8,4},{0x62dc,4},{0x62e0,4},{0x62e4,2},{0x62e6,6},{0x62ec,8},{0x62f4,2},{0x62f6,2},{0x62f8,4},{0x62fc,4},{0x6300,2},{0x6302,6},{0x6308,6},{0x630e,2},{0x6310,2},{0x6312,2},{0x6314,6},{0x631a,4},{0x631e,6},{0x6324,4},{0x6328,6},{0x632e,2},{0x6330,2},{0x6332,6},{0x6338,2},{0x633a,6},{0x6340,8},{0x6348,2},{0x634a,2},{0x634c,4},{0x6350,6},{0x6356,2},{0x6358,2},{0x635a,6},{0x6360,2},{0x6362,6},{0x6368,2},{0x636a,4},{0x636e,6},{0x6374,4},{0x6378,2},{0x637a,6},{0x6380,2},{0x6382,2},{0x6384,2},{0x6386,4},{0x638a,4},{0x638e,4},{0x6392,2},{0x6394,8},{0x639c,6},{0x63a2,2}}};Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStateProgramChangePartialForTests(StateProgramChangePartialTest f){partialTest=f;}
bool StateProgramChangeCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x62b2||pc==0x62e6||pc==0x633a||pc==0x6382){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native program-change capacity exceeded");selected->emplace(pc==0x62b2?T::Kind::ReceivedMulti:pc==0x62e6?T::Kind::CommitSingle:pc==0x633a?T::Kind::CommitMulti:T::Kind::LoadSingle,uint16_t(h.regs[5].r),h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();unsigned address=0;switch(stage){case S::WriteReceivedProgram:address=0x167ab;break;case S::WriteMultiMode:address=0x167ba;break;case S::WriteOverlay:address=0x167a2;break;case S::WriteRememberedSingle:address=0x167a4;break;case S::WriteClearedSelection:address=0x16798;break;case S::WriteRestoredSelection:address=0x16798;break;case S::WritePatchReady:address=r[0].er+310;break;case S::WritePanelReady:address=0x16783;break; default:break;}address&=0xffffff;const unsigned storeClocks=address<0x400000?((h.lastwrite&1)||int(address)!=(h.lastwrite|1)?3:0):3;const auto cost=task.nextClocks(stage==S::WriteRememberedSingle?3:storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what)->uint32_t{switch(what){case T::Read::PreviousMode:return observed=uint8_t(h.read8(0x167ba));
case T::Read::SingleLimit:return observed=uint8_t(h.read8(0x168fc+241));
case T::Read::SingleBank:return observed=uint8_t(h.read8(0x167ac));
case T::Read::Overlay:return observed=uint8_t(h.read8(0x167a2));
case T::Read::SingleProgram:return observed=uint8_t(h.read8(0x167ad));
case T::Read::SingleWord:return observed=uint16_t(h.read16(0x167ac));
case T::Read::MultiLimit:return observed=uint8_t(h.read8(0x168fc+241));
case T::Read::Selection:return observed=uint8_t(h.read8(0x16798));
case T::Read::MultiBank:return observed=uint8_t(h.read8(0x167aa));
case T::Read::MultiProgram:return observed=uint8_t(h.read8(0x167ab));}return 0;};auto write=[&](T::Write what,uint16_t value){switch(what){case T::Write::ReceivedProgram:f.store8(0x167ab,value);break;
case T::Write::MultiMode:f.store8(0x167ba,value);break;
case T::Write::Overlay:f.store8(0x167a2,value);break;
case T::Write::RememberedSingle:f.store16(0x167a4,value);break;
case T::Write::Selection:f.store8(0x16798,value);break;
case T::Write::PatchReady:f.store8(r[0].er+310,value);break;
case T::Write::PanelReady:f.store8(0x16783,value);break;}};if(task.advanceBefore(cost,read,write,stage==S::WriteRememberedSingle?3:storeClocks)!=cost)throw std::runtime_error("Native program-change admission differs");switch(stage){case S::MaskReceivedBank:{r[5].rh&=0x3f;value(h,r[5].rh);break;}
case S::CompareReceivedBank:{h.sub8(r[5].rh,0x1);break;}
case S::AdmitReceivedBank:{;break;}
case S::CompareReceivedProgram:{h.sub8(r[5].rl,0x64);break;}
case S::AdmitReceivedProgram:{;break;}
case S::WriteReceivedProgram:{;break;}
case S::ReadPreviousMode:{f.move(r[0].rh,observed);break;}
case S::SetMultiMode:{f.move(r[0].rl,0x1);break;}
case S::WriteMultiMode:{;break;}
case S::ComparePreviousMode:{h.sub8(r[0].rh,r[0].rl);break;}
case S::AdmitPreviousMode:{;break;}
case S::ClearSinglePanel:{f.call(0x466e);break;}
case S::ResetMultiPanel:{f.call(0x48dc);break;}
case S::LoadReceivedMulti:{f.call(0x6ff4);break;}
case S::RefreshReceivedDisplay:{f.call(0x5340);break;}
case S::ReturnReceived:{f.ret();break;}
case S::SetSingleGlobals:{f.move(r[0].er,0x168fc);break;}
case S::ReadSingleLimit:{f.move(r[0].rl,observed);break;}
case S::CompareSingleLimit:{h.sub8(r[0].rl,0x3f);break;}
case S::AdmitSingleLimit:{;break;}
case S::LoadCurrentSingle:{f.call(0x6382);break;}
case S::RefreshSingleContext:{f.call(0x43c2);break;}
case S::SetSingleMessage:{f.move(r[6].rl,0x20);break;}
case S::ReadSingleBank:{f.move(r[6].rh,observed);break;}
case S::ReadOverlay:{f.move(r[0].rl,observed);break;}
case S::AdmitOverlay:{;break;}
case S::MarkOverlay:{r[6].rh|=0x20;value(h,r[6].rh);break;}
case S::ClearOverlay:{f.move(r[0].rl,0x0);break;}
case S::WriteOverlay:{;break;}
case S::SendSingleBank:{f.call(0x2c06);break;}
case S::ReadSingleProgram:{f.move(r[6].rl,observed);break;}
case S::SendSingleProgram:{f.call(0x2c1e);break;}
case S::ReadSingleWord:{f.move(r[0].r,observed);break;}
case S::CompareSingleBank:{h.sub8(r[0].rh,0x4);break;}
case S::AdmitSingleBank:{;break;}
case S::WriteRememberedSingle:{;break;}
case S::ReturnSingle:{f.ret();break;}
case S::SetMultiGlobals:{f.move(r[0].er,0x168fc);break;}
case S::ReadMultiLimit:{f.move(r[0].rl,observed);break;}
case S::CompareMultiLimit:{h.sub8(r[0].rl,0x3f);break;}
case S::AdmitMultiLimit:{;break;}
case S::LoadCurrentMulti:{f.call(0x6ff4);break;}
case S::ReadSelection:{f.move(r[0].rl,observed);break;}
case S::RetainSelection:{f.push(r[0].r);break;}
case S::ClearSelection:{f.move(r[0].rl,0x0);break;}
case S::WriteClearedSelection:{;break;}
case S::SetMultiMessage:{f.move(r[6].rl,0x20);break;}
case S::ReadMultiBank:{f.move(r[6].rh,observed);break;}
case S::MarkMulti:{r[6].rh|=0x40;value(h,r[6].rh);break;}
case S::SendMultiBank:{f.call(0x2c06);break;}
case S::ReadMultiProgram:{f.move(r[6].rl,observed);break;}
case S::SendMultiProgram:{f.call(0x2c1e);break;}
case S::RestoreSelection:{f.pop(r[0].r);break;}
case S::WriteRestoredSelection:{;break;}
case S::ReturnMulti:{f.ret();break;}
case S::GetSingleAddress:{f.call(0x63a4);break;}
case S::LoadSingleAddress:{f.call(0x63fe);break;}
case S::BindSingleVoices:{f.call(0x9fe4);break;}
case S::RefreshSingleVoices:{f.call(0x936e);break;}
case S::PrepareSingleEngine:{f.call(0x4000);break;}
case S::ClearReady:{f.move(r[6].rl,0x0);break;}
case S::WritePatchReady:{;break;}
case S::WritePanelReady:{;break;}
case S::ReturnLoad:{f.ret();break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native program-change clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
