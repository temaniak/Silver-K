#include "ksStateSingleLoadCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateSingleLoadTask;using S=T::Stage;StateSingleLoadPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,39> points{{{0x63fe,8},{0x6406,4},{0x640a,6},{0x6410,2},{0x6412,4},{0x6416,2},{0x6418,2},{0x641a,2},{0x641c,2},{0x641e,2},{0x6420,2},{0x6422,2},{0x6424,6},{0x642a,8},{0x6432,6},{0x6438,4},{0x643c,2},{0x643e,2},{0x6440,2},{0x6442,2},{0x6444,2},{0x6446,2},{0x6448,2},{0x644a,2},{0x644c,2},{0x644e,4},{0x6452,4},{0x6456,8},{0x645e,8},{0x6466,6},{0x646c,2},{0x646e,6},{0x6474,6},{0x647a,6},{0x6480,6},{0x6486,4},{0x648a,4},{0x648e,4},{0x6492,2}}};Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStateSingleLoadPartialForTests(StateSingleLoadPartialTest f){partialTest=f;}
bool StateSingleLoadCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x63fe){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native single load capacity exceeded");selected->emplace(h.getSP(),uint32_t(h.regs[0].er));++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild()){const bool overlay=!(h.ccr&4);if(task.child()==T::Child::Overlay)call.overlay=uint32_t(r[0].er);task.resumeChild(overlay);}const auto stage=task.stage();const uint32_t address=stage==S::WriteByte?call.target+task.byteOffset():stage==S::WriteOverlayByte?call.overlayTarget+84+task.overlayOffset():stage==S::WriteOverlayExtra?call.overlayTarget+126:stage==S::WriteCategory?0x167da:stage==S::WriteGate?0x167a3:stage==S::WriteBank?0x1679e :0x167a0;const unsigned physical=address&0xffffff;const unsigned storeClocks=physical<0x400000?((h.lastwrite&1)||int(physical)!=(h.lastwrite|1)?3:0):3;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{if(what==T::Read::Target)return observed=call.target=uint32_t(h.read32(0x167bc));if(what==T::Read::OverlayTarget)return observed=call.overlayTarget=uint32_t(h.read32(0x167bc));if(what==T::Read::LateTarget)return observed=call.lateTarget=uint32_t(h.read32(0x167bc));return observed=uint8_t(h.read8(what==T::Read::Mode?0x167ba:what==T::Read::Byte?call.source+task.byteOffset():what==T::Read::OverlayByte?call.overlay+84+task.overlayOffset():what==T::Read::OverlayExtra?call.overlay+126:what==T::Read::Category?call.lateTarget+176:0x16798));};auto write=[&](T::Write,uint8_t value){f.store8(address,value);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native single load admission differs");switch(stage){case S::ReadTarget:{f.move(r[1].er,observed);break;}
case S::SetCount:{f.move(r[2].e,0xd1);break;}
case S::ReadMode:{f.move(r[2].rl,observed);break;}
case S::AdmitMode:{;break;}
case S::SetPartCount:{f.move(r[2].e,0xb3);break;}
case S::ReadByte:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::WriteByte:{;break;}
case S::AdvanceTarget:{f.inc(r[1].er,1);break;}
case S::DecrementCount:{f.inc(r[2].e,-1);break;}
case S::AdmitCount:{;break;}
case S::ResolveOverlay:{f.call(0x63bc);break;}
case S::AdmitOverlay:{;break;}
case S::AddOverlaySource:{r[0].er=h.add32(r[0].er,0x54);break;}
case S::ReadOverlayTarget:{f.move(r[1].er,observed);break;}
case S::AddOverlayTarget:{r[1].er=h.add32(r[1].er,0x54);break;}
case S::SetOverlayCount:{f.move(r[2].e,0x29);break;}
case S::ReadOverlayByte:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::WriteOverlayByte:{;break;}
case S::AdvanceOverlayTarget:{f.inc(r[1].er,1);break;}
case S::DecrementOverlayCount:{f.inc(r[2].e,-1);break;}
case S::AdmitOverlayCount:{;break;}
case S::SkipOverlaySource:{r[0].er=int32_t(uint32_t(r[0].er)+0x1);break;}
case S::SkipOverlayTarget:{r[1].er=int32_t(uint32_t(r[1].er)+0x1);break;}
case S::ReadOverlayExtra:{f.move(r[2].rl,observed);break;}
case S::WriteOverlayExtra:{;break;}
case S::InvalidateLive:{f.call(0x6aca);break;}
case S::ExpandFields:{f.call(0x6726);break;}
case S::ReadLateTarget:{f.move(r[1].er,observed);break;}
case S::ReadCategory:{f.move(r[0].rl,observed);break;}
case S::WriteCategory:{;break;}
case S::ClearFlags:{f.move(r[0].rl,0x0);break;}
case S::WriteGate:{;break;}
case S::WriteBank:{;break;}
case S::WriteMode:{;break;}
case S::ReadPart:{f.move(r[6].rl,observed);break;}
case S::RefreshPart:{f.call(0x9f28);break;}
case S::RefreshPerformance:{f.call(0xa106);break;}
case S::RefreshPanel:{f.call(0x6c3c);break;}
case S::ReturnLoad:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native single load clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
