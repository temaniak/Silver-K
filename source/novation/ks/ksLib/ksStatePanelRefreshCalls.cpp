#include "ksStatePanelRefreshCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StatePanelRefreshTask;using S=T::Stage;StatePanelRefreshPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,52> points{{{0x6c3c,4},{0x6c40,2},{0x6c42,2},{0x6c44,4},{0x6c48,2},{0x6c4a,2},{0x6c4c,2},{0x6c4e,2},{0x6c50,4},{0x6c54,4},{0x6c58,4},{0x6c5c,4},{0x6c60,8},{0x6c68,8},{0x6c70,2},{0x6c72,4},{0x6c76,8},{0x6c7e,2},{0x6c80,2},{0x6c82,2},{0x6c84,4},{0x6c88,8},{0x6c90,2},{0x6c92,2},{0x6c94,2},{0x6c96,2},{0x6c98,4},{0x6c9c,6},{0x6ca2,2},{0x6ca4,4},{0x6ca8,6},{0x6cae,2},{0x6cb0,4},{0x6cb4,6},{0x6cba,2},{0x6cbc,4},{0x6cc0,6},{0x6cc6,2},{0x6cc8,4},{0x6ccc,6},{0x6cd2,2},{0x6cd4,4},{0x6cd8,6},{0x6cde,2},{0x6ce0,4},{0x6ce4,6},{0x6cea,2},{0x6cec,4},{0x6cf0,6},{0x6cf6,2},{0x6cf8,4},{0x6cfc,2}}};Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStatePanelRefreshPartialForTests(StatePanelRefreshPartialTest f){partialTest=f;}
bool StatePanelRefreshCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x6c3c){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native panel-refresh capacity exceeded");selected->emplace(h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what)->uint32_t{switch(what){case T::Read::Current:return observed=call.current=uint32_t(h.read32(0x167bc));
case T::Read::OscillatorMode:return observed=uint8_t(h.read8(call.current+226));
case T::Read::LfoMode:return observed=uint8_t(h.read8(call.current+88));
case T::Read::FilterMode:return observed=uint8_t(h.read8(call.current+88));
case T::Read::MaskFirst:return observed=uint8_t(h.read8(0x167d2));
case T::Read::MaskSecond:return observed=uint8_t(h.read8(0x167d3));
case T::Read::PartMode:return observed=uint8_t(h.read8(0x167ba));
case T::Read::PageMode:return observed=uint8_t(h.read8(0x16784));
case T::Read::EditMode:return observed=uint8_t(h.read8(0x1679e));
case T::Read::GlobalMode:return observed=uint8_t(h.read8(0x167a1));
case T::Read::AlternateMode:return observed=uint8_t(h.read8(0x167a3));
case T::Read::MenuMode:return observed=uint8_t(h.read8(0x16790));}return 0;};if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native panel-refresh admission differs");switch(stage){case S::SetCommonKey:{f.move(r[6].r,0x26);break;}
case S::SetCommonCount:{f.move(r[0].rl,0x9);break;}
case S::RetainCount:{f.push(r[0].r);break;}
case S::RefreshCommon:{f.call(0x4eb0);break;}
case S::AdvanceCommon:{f.inc(r[6].rl,1);break;}
case S::RestoreCount:{f.pop(r[0].r);break;}
case S::DecrementCount:{f.inc(r[0].rl,-1);break;}
case S::AdmitCommon:{;break;}
case S::SetFirstSpecial:{f.move(r[6].r,0x1);break;}
case S::RefreshFirstSpecial:{f.call(0x4eaa);break;}
case S::SetSecondSpecial:{f.move(r[6].r,0xe);break;}
case S::RefreshSecondSpecial:{f.call(0x4eaa);break;}
case S::ReadCurrent:{f.move(r[0].er,observed);break;}
case S::ReadOscillatorMode:{f.move(r[6].rh,observed);break;}
case S::SetOscillatorKey:{f.move(r[6].rl,0x2);break;}
case S::RefreshOscillator:{f.call(0x78c);break;}
case S::ReadLfoMode:{f.move(r[6].rh,observed);break;}
case S::ShiftLfo:{f.shift(r[6].rh,2,false);break;}
case S::MaskLfo:{r[6].rh&=0x1;value(h,r[6].rh);break;}
case S::SetLfoKey:{f.move(r[6].rl,0x6);break;}
case S::RefreshLfo:{f.call(0x78c);break;}
case S::ReadFilterMode:{f.move(r[6].rh,observed);break;}
case S::ShiftFilterFirst:{f.shift(r[6].rh,2,false);break;}
case S::ShiftFilterSecond:{f.shift(r[6].rh,2,false);break;}
case S::MaskFilter:{r[6].rh&=0x1;value(h,r[6].rh);break;}
case S::SetFilterKey:{f.move(r[6].rl,0xb);break;}
case S::RefreshFilter:{f.call(0x78c);break;}
case S::ReadMaskFirst:{f.move(r[6].rh,observed);break;}
case S::SetMaskFirstKey:{f.move(r[6].rl,0xc);break;}
case S::RefreshMaskFirst:{f.call(0x78c);break;}
case S::ReadMaskSecond:{f.move(r[6].rh,observed);break;}
case S::SetMaskSecondKey:{f.move(r[6].rl,0xd);break;}
case S::RefreshMaskSecond:{f.call(0x78c);break;}
case S::ReadPartMode:{f.move(r[6].rh,observed);break;}
case S::SetPartKey:{f.move(r[6].rl,0x11);break;}
case S::RefreshPart:{f.call(0x78c);break;}
case S::ReadPageMode:{f.move(r[6].rh,observed);break;}
case S::SetPageKey:{f.move(r[6].rl,0x10);break;}
case S::RefreshPage:{f.call(0x78c);break;}
case S::ReadEditMode:{f.move(r[6].rh,observed);break;}
case S::SetEditKey:{f.move(r[6].rl,0x3);break;}
case S::RefreshEdit:{f.call(0x78c);break;}
case S::ReadGlobalMode:{f.move(r[6].rh,observed);break;}
case S::SetGlobalKey:{f.move(r[6].rl,0xa);break;}
case S::RefreshGlobal:{f.call(0x78c);break;}
case S::ReadAlternateMode:{f.move(r[6].rh,observed);break;}
case S::SetAlternateKey:{f.move(r[6].rl,0x14);break;}
case S::RefreshAlternate:{f.call(0x78c);break;}
case S::ReadMenuMode:{f.move(r[6].rh,observed);break;}
case S::SetMenuKey:{f.move(r[6].rl,0x15);break;}
case S::RefreshMenu:{f.call(0x78c);break;}
case S::Return:{f.ret();break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native panel-refresh clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
