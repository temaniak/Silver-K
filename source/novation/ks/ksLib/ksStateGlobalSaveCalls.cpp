#include "ksStateGlobalSaveCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateGlobalSaveTask;using S=T::Stage;StateGlobalSavePartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,30> points{{{0x69e2,6},{0x69e8,6},{0x69ee,8},{0x69f6,2},{0x69f8,6},{0x69fe,8},{0x6a06,6},{0x6a0c,8},{0x6a14,8},{0x6a1c,6},{0x6a22,2},{0x6a24,8},{0x6a2c,8},{0x6a34,6},{0x6a3a,6},{0x6a40,4},{0x6a44,2},{0x6a46,2},{0x6a48,2},{0x6a4a,2},{0x6a4c,2},{0x6a4e,4},{0x6a52,2},{0x6a54,2},{0x6a56,2},{0x6a58,2},{0x6a5a,2},{0x6a5c,6},{0x6a62,2},{0x6a64,4}}};Point point(const T& t){return t.awaitingProgram()?Point{0x6662,0}:points[unsigned(t.stage())];}unsigned address(const T& t){switch(t.stage()){case S::WritePart:return 0x168fc+238;case S::WriteSingleBank:return 0x168fc+236;case S::WriteSingleProgram:return 0x168fc+237;case S::WriteMultiBank:return 0x168fc+234;case S::WriteMultiProgram:return 0x168fc+235;default:return 0x167fc+t.byteOffset();}}}
void setStateGlobalSavePartialForTests(StateGlobalSavePartialTest f){partialTest=f;}
bool StateGlobalSaveCalls::retireReturns(ControllerCpu& h){bool retired=false;for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingProgram()&&h.getPC()==0x6662){c->task.retireProgram();c.reset();++completed;retired=true;}return retired;}
bool StateGlobalSaveCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x69e2){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native global-save capacity exceeded");selected->emplace(h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;const auto stage=task.stage();const auto physical=address(task);const unsigned storeClocks=(h.lastwrite&1)||int(physical)!=(h.lastwrite|1)?3:0;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what)->uint32_t{switch(what){case T::Read::Enabled:return observed=uint8_t(h.read8(0x168fc+239));case T::Read::Part:return observed=uint8_t(h.read8(0x167ba));case T::Read::Single:return observed=uint16_t(h.read16(0x167ae));case T::Read::Multi:return observed=uint16_t(h.read16(0x167aa));default:return observed=uint8_t(h.read8(0x169e6+task.byteOffset()));}};auto write=[&](T::Write,uint8_t value){f.store8(physical,value);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native global-save admission differs");switch(stage){case S::SetPage:{f.move(r[6].er,0x81ff00);break;}
case S::SetGlobal:{f.move(r[0].er,0x168fc);break;}
case S::ReadEnabled:{f.move(r[1].rl,observed);break;}
case S::AdmitEnabled:{;break;}
case S::ReadPart:{f.move(r[1].rl,observed);break;}
case S::WritePart:{;break;}
case S::ReadSingle:{f.move(r[1].r,observed);break;}
case S::WriteSingleBank:{;break;}
case S::WriteSingleProgram:{;break;}
case S::ReadMulti:{f.move(r[1].r,observed);break;}
case S::SetMultiBank:{f.move(r[1].rh,0x1);break;}
case S::WriteMultiBank:{;break;}
case S::WriteMultiProgram:{;break;}
case S::SetSource:{f.move(r[0].er,0x169e6);break;}
case S::SetScratch:{f.move(r[1].er,0x167fc);break;}
case S::SetCopyCount:{f.move(r[2].e,0x15);break;}
case S::ReadByte:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::WriteByte:{;break;}
case S::AdvanceCopy:{f.inc(r[1].er,1);break;}
case S::DecrementCopy:{f.inc(r[2].e,-1);break;}
case S::AdmitCopy:{;break;}
case S::SetZeroCount:{f.move(r[2].e,0xeb);break;}
case S::ClearByte:{f.move(r[2].rl,0x0);break;}
case S::WriteZero:{;break;}
case S::AdvanceZero:{f.inc(r[1].er,1);break;}
case S::DecrementZero:{f.inc(r[2].e,-1);break;}
case S::AdmitZero:{;break;}
case S::SetScratchSource:{f.move(r[1].er,0x167fc);break;}
case S::CaptureSource:{f.move(r[5].er,r[1].er);break;}
case S::TailProgram:{h.clockI(2);break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native global-save clocks differ");h.pc=h.makepc(point(task).location);return true;}}
