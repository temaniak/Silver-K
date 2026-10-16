#include "ksStatePresetAddressCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StatePresetAddressTask;using S=T::Stage;StatePresetAddressPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,27> points{{{0x63a4,6},{0x63aa,4},{0x63ae,4},{0x63b2,2},{0x63b4,6},{0x63ba,2},{0x63bc,2},{0x63be,6},{0x63c4,2},{0x63c6,2},{0x63c8,2},{0x63ca,2},{0x63cc,2},{0x63ce,2},{0x63d0,2},{0x63d2,6},{0x63d8,2},{0x63da,2},{0x63dc,6},{0x63e2,4},{0x63e6,6},{0x63ec,4},{0x63f0,2},{0x63f2,6},{0x63f8,2},{0x63fa,2},{0x63fc,2}}};Point point(const T& t){return points[unsigned(t.awaitingIndex()?t.resumeStage():t.stage())];}}
void setStatePresetAddressPartialForTests(StatePresetAddressPartialTest f){partialTest=f;}
bool StatePresetAddressCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x63a4||pc==0x63bc){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native preset-address capacity exceeded");selected->emplace(pc==0x63a4?T::Kind::Current:T::Kind::Overlay,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingIndex())task.resumeIndex(uint16_t(r[0].r));const auto stage=task.stage();const unsigned storeClocks=(h.lastwrite&1)||0x167ad!=(h.lastwrite|1)?3:0;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what)->uint32_t{return observed=uint8_t(h.read8(what==T::Read::Bank?0x167ac:0x167ad));};auto write=[&](T::Write,uint8_t v){f.store8(0x167ad,v);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native preset-address admission differs");switch(stage){case S::SetCurrentSource:{f.move(r[0].er,0x167ac);break;}
case S::ReadCurrentIndex:{f.call(0x6198);break;}
case S::SetCurrentStride:{f.move(r[0].e,0x100);break;}
case S::ScaleCurrent:{r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20);break;}
case S::AddCurrentBase:{r[0].er=h.add32(r[0].er,0x820000);break;}
case S::ReturnCurrent:{if(uint32_t(r[0].er)!=task.address())throw std::runtime_error("Native preset address differs");f.ret();break;}
case S::RetainProgram:{f.push(r[1].r);break;}
case S::SetOverlaySource:{f.move(r[0].er,0x167ac);break;}
case S::ReadBank:{f.move(r[1].rl,observed);break;}
case S::CompareBank:{h.sub8(r[1].rl,0x4);break;}
case S::AdmitBank:{;break;}
case S::RestoreUnavailableProgram:{f.pop(r[1].r);break;}
case S::MarkUnavailable:{f.move(r[0].rl,0x0);break;}
case S::ReturnUnavailable:{f.ret();break;}
case S::SetAlias:{f.move(r[1].rh,0x31);break;}
case S::ReadProgram:{f.move(r[1].rl,observed);break;}
case S::CompareAlias:{h.sub8(r[1].rl,r[1].rh);break;}
case S::AdmitAlias:{;break;}
case S::SetAliasProgram:{;break;}
case S::ReadAliasIndex:{f.call(0x6198);break;}
case S::RestoreAliasProgram:{;break;}
case S::SetAliasStride:{f.move(r[0].e,0x100);break;}
case S::ScaleAlias:{r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20);break;}
case S::AddAliasBase:{r[0].er=h.add32(r[0].er,0x820000);break;}
case S::RestoreProgram:{f.pop(r[1].r);break;}
case S::MarkAvailable:{f.move(r[0].er,r[0].er);break;}
case S::ReturnAlias:{if(uint32_t(r[0].er)!=task.address())throw std::runtime_error("Native preset address differs");f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native preset-address clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingIndex())call.token=h.getSP();h.pc=h.makepc(task.awaitingIndex()?f.next:point(task).location);return true;}}
