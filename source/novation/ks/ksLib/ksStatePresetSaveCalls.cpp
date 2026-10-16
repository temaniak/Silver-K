#include "ksStatePresetSaveCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StatePresetSaveTask;using S=T::Stage;StatePresetSavePartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,21> points{{{0x6494,4},{0x6498,8},{0x64a0,6},{0x64a6,4},{0x64aa,8},{0x64b2,6},{0x64b8,2},{0x64ba,2},{0x64bc,2},{0x64be,2},{0x64c0,2},{0x64c2,2},{0x64c4,2},{0x64c6,2},{0x64c8,4},{0x64cc,2},{0x64ce,6},{0x64d4,4},{0x64d8,6},{0x64de,4},{0x64e2,2}}};Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStatePresetSavePartialForTests(StatePresetSavePartialTest f){partialTest=f;}
bool StatePresetSaveCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x6494){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native preset-save capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();const unsigned storeClocks=(h.lastwrite&1)||0x167ad!=(h.lastwrite|1)?3:0;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what)->uint32_t{return observed=what==T::Read::Program?uint16_t(h.read16(0x167ac)):uint32_t(h.read32(0x167bc));};auto write=[&](T::Write what,uint16_t value){if(what==T::Write::Alias)f.store16(0x167ac,value);else f.store8(0x167ad,uint8_t(value));};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native save admission differs");switch(stage){case S::PackFields:{f.call(0x687e);break;}
case S::ReadCurrent:{f.move(r[1].er,observed);break;}
case S::SetProgramSource:{f.move(r[0].er,0x167ac);break;}
case S::StorePage:{f.call(0x6576);break;}
case S::ReadLateCurrent:{f.move(r[1].er,observed);break;}
case S::SetLateProgramSource:{f.move(r[0].er,0x167ac);break;}
case S::ReadProgram:{f.move(r[2].r,observed);break;}
case S::CompareBank:{h.sub8(r[2].rh,0x4);break;}
case S::AdmitBank:{;break;}
case S::CompareAlias:{h.sub8(r[2].rl,0x31);break;}
case S::AdmitAlias:{;break;}
case S::RetainProgram:{f.push(r[2].r);break;}
case S::SetAlias:{f.move(r[2].rl,0x31);break;}
case S::WriteAlias:{;break;}
case S::StoreAlias:{f.call(0x6576);break;}
case S::RestoreProgram:{f.pop(r[2].r);break;}
case S::WriteOriginal:{;break;}
case S::SetTimerTicks:{f.move(r[6].r,0xfa);break;}
case S::SetTimerCallback:{f.move(r[5].er,0x55e2);break;}
case S::ScheduleTimer:{f.call(0x40d8);break;}
case S::ReturnSave:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native save clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
