#include "ksStatePatchLoadCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StatePatchLoadTask;using S=T::Stage;StatePatchLoadPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,27> points{{{0x7134,2},{0x7136,6},{0x713c,2},{0x713e,2},{0x7140,2},{0x7142,2},{0x7144,4},{0x7148,2},{0x714a,2},{0x714c,8},{0x7154,4},{0x7158,2},{0x715a,2},{0x715c,2},{0x715e,2},{0x7160,2},{0x7162,4},{0x7166,4},{0x716a,6},{0x7170,4},{0x7174,4},{0x7178,4},{0x717c,4},{0x7180,4},{0x7184,2},{0x7186,8},{0x718e,2}}};Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStatePatchLoadPartialForTests(StatePatchLoadPartialTest f){partialTest=f;}
bool StatePatchLoadCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if((pc==0x7134||pc==0x714c)&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native patch load capacity exceeded");selected->emplace(pc==0x7134?T::Entry::Selected:T::Entry::Provided,h.getSP(),uint32_t(h.regs[0].er));++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild()){if(task.child()==T::Child::Source)call.source=uint32_t(r[0].er);if(task.child()==T::Child::Context)call.context=uint32_t(r[0].er);task.resumeChild();}const auto stage=task.stage();const uint32_t address=stage==S::WritePending?call.context+310:call.target+task.byteOffset();const unsigned physical=address&0xffffff;const unsigned storeClocks=physical<0x400000?((h.lastwrite&1)||int(physical)!=(h.lastwrite|1)?3:0):3;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{if(what==T::Read::Target)return observed=call.target=uint32_t(h.read32(0x167bc));if(what==T::Read::Byte)return observed=uint8_t(h.read8(call.source+task.byteOffset()));return observed=uint8_t(h.read8(0x16798));};auto write=[&](T::Write,uint8_t v){f.store8(address,v);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native load admission differs");switch(stage){case S::DispatchSource:{f.call(0x7190);break;}
case S::ReadSourcePart:{f.move(r[1].rl,observed);break;}
case S::PreviousSourcePart:{f.inc(r[1].rl,-1);break;}
case S::AdmitSourcePart:{;break;}
case S::ClearSourcePart:{f.move(r[1].rl,0x0);break;}
case S::NormalizeSourcePart:{r[1].r&=0xff;value(h,r[1].r);break;}
case S::SetPartStride:{f.move(r[1].e,0x100);break;}
case S::ScaleSourcePart:{r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[1].e));h.clockI(20);break;}
case S::AddSourcePart:{r[0].er=h.add32(r[0].er,r[1].er);call.source+=task.sourcePartOffset();break;}
case S::ReadTarget:{f.move(r[1].er,observed);break;}
case S::SetCount:{f.move(r[2].e,0xd1);break;}
case S::ReadByte:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::WriteByte:{;break;}
case S::AdvanceTarget:{f.inc(r[1].er,1);break;}
case S::DecrementCount:{f.inc(r[2].e,-1);break;}
case S::AdmitCount:{;break;}
case S::InvalidateLive:{f.call(0x6aca);break;}
case S::ExpandFields:{f.call(0x6726);break;}
case S::ReadPart:{f.move(r[6].rl,observed);break;}
case S::RefreshPart:{f.call(0x9f28);break;}
case S::RefreshVoices:{f.call(0x936e);break;}
case S::RefreshSerial:{f.call(0x3074);break;}
case S::RefreshPerformance:{f.call(0xa106);break;}
case S::SelectContext:{f.call(0x4000);break;}
case S::ClearPending:{f.move(r[6].rl,0x0);break;}
case S::WritePending:{;break;}
case S::ReturnLoad:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native load clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
