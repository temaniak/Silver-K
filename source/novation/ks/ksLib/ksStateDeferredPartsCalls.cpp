#include "ksStateDeferredPartsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateDeferredPartsTask;using S=T::Stage;StateDeferredPartsPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,33> points{{{0x737a,6},{0x7380,2},{0x7382,2},{0x7384,6},{0x738a,2},{0x738c,6},{0x7392,2},{0x7394,6},{0x739a,2},{0x739c,2},{0x739e,6},{0x73a4,2},{0x73a6,2},{0x73a8,2},{0x73aa,2},{0x73ac,2},{0x73ae,6},{0x73b4,6},{0x73ba,2},{0x73bc,2},{0x73be,2},{0x73c0,4},{0x73c4,2},{0x73c6,2},{0x73c8,2},{0x73ca,2},{0x73cc,2},{0x73ce,2},{0x73d0,2},{0x73d2,2},{0x73d4,6},{0x73da,6},{0x73e0,2}}};Point point(const T& t){return points[unsigned(t.awaitingRefresh()?S::RestoreProcessed:t.stage())];}unsigned address(S s){switch(s){case S::WriteDelay:case S::WriteDelayCount:return 0x1679b;case S::WritePending:return 0x1679a;case S::WritePartNumber:return 0x17526;default:return 0x167ba;}}}
void setStateDeferredPartsPartialForTests(StateDeferredPartsPartialTest f){partialTest=f;}
bool StateDeferredPartsCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x737a){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native deferred-part capacity exceeded");selected->emplace(h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingRefresh())task.resumeRefresh();const auto stage=task.stage();const unsigned target=address(stage);const unsigned storeClocks=(h.lastwrite&1)||int(target)!=(h.lastwrite|1)?3:0;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what){return observed=uint8_t(h.read8(what==T::Read::Delay?0x1679b:what==T::Read::Pending?0x1679a:0x167ba));};auto write=[&](T::Write,uint8_t value){f.store8(target,value);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native deferred-part admission differs");switch(stage){case S::ReadDelay:{f.move(r[0].rl,observed);break;}
case S::AdmitDelay:{;break;}
case S::DecrementDelay:{f.inc(r[0].rl,-1);break;}
case S::WriteDelay:{;break;}
case S::SkipWork:{;break;}
case S::ReadPending:{f.move(r[0].rl,observed);break;}
case S::AdmitPending:{;break;}
case S::ReadSelected:{f.move(r[1].rl,observed);break;}
case S::RetainSelected:{f.push(r[1].r);break;}
case S::ClearWorking:{r[1].r=h.sub16(r[1].r,r[1].r);break;}
case S::WritePending:{;break;}
case S::SetBitCount:{f.move(r[0].rh,0x5);break;}
case S::ClearProcessed:{f.move(r[2].rl,0x0);break;}
case S::ShiftMask:{f.shift(r[0].rl,1,false);break;}
case S::AdmitBit:{;break;}
case S::CountProcessed:{f.inc(r[2].rl,1);break;}
case S::WritePartKind:{;break;}
case S::WritePartNumber:{;break;}
case S::RetainPending:{f.push(r[0].r);break;}
case S::RetainPart:{f.push(r[1].r);break;}
case S::RetainProcessed:{f.push(r[2].r);break;}
case S::RefreshPart:{f.call(0x9662);break;}
case S::RestoreProcessed:{f.pop(r[2].r);break;}
case S::RestorePart:{f.pop(r[1].r);break;}
case S::RestorePending:{f.pop(r[0].r);break;}
case S::SetLaterKind:{f.move(r[1].rh,0x1);break;}
case S::AdvancePart:{f.inc(r[1].rl,1);break;}
case S::DecrementBits:{f.inc(r[0].rh,-1);break;}
case S::AdmitBits:{;break;}
case S::RestoreSelected:{f.pop(r[1].r);break;}
case S::WriteSelected:{;break;}
case S::WriteDelayCount:{;break;}
case S::Return:{f.ret();break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native deferred-part clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingRefresh())call.token=h.getSP();h.pc=h.makepc(task.awaitingRefresh()?f.next:point(task).location);return true;}}
