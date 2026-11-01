#include "ksStateScreenEditCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateScreenEditPartialTest partialTest=nullptr;using T=StateScreenEditTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,57> points{{{0x59a6,8},{0x59ae,8},{0x59b6,2},{0x59b8,2},{0x59ba,4},{0x59be,8},{0x59c6,2},{0x59c8,8},{0x59d0,2},{0x59d2,4},{0x59d6,4},{0x59da,2},{0x59dc,2},{0x59de,2},{0x59e0,2},{0x59e2,2},{0x59e4,2},{0x59e6,2},{0x59e8,2},{0x59ea,2},{0x59ec,2},{0x59ee,2},{0x59f0,2},{0x59f2,2},{0x59f4,2},{0x59f6,2},{0x59f8,2},{0x59fa,2},{0x59fc,2},{0x59fe,2},{0x5a00,2},{0x5a02,2},{0x5a04,2},{0x5a06,2},{0x5a08,2},{0x5a0a,2},{0x5a0c,2},{0x5a0e,2},{0x5a10,2},{0x5a12,2},{0x5a14,2},{0x5a16,2},{0x5a18,2},{0x5a1a,2},{0x5a1c,2},{0x5a1e,2},{0x5a20,2},{0x5a22,2},{0x5a24,2},{0x5a26,2},{0x5a28,4},{0x5a2c,4},{0x5a30,2},{0x5a32,6},{0x5a38,8},{0x5a40,4},{0x5a44,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){return at<0x400000&&!(previous&1)&&at==(unsigned(previous)|1u)?0:3;}
}
void setStateScreenEditPartialForTests(StateScreenEditPartialTest value){partialTest=value;}
bool StateScreenEditCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x59a6:kind=T::Kind::Edit;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateScreenEdit capacity exceeded");slot->emplace(*kind,h.getSP(),uint8_t(h.regs[6].rl));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();
 const auto store=byteClocks(0,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field,uint32_t ref)->uint32_t{if(field==T::Read::Byte)return uint8_t(h.read8(ref));return uint32_t(h.read32(field==T::Read::Descriptor?0x167f0:field==T::Read::EditBase?0x167a6:0x167bc));};auto write=[](int,unsigned){};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateScreenEdit admission differs");
 switch(stage){case S::ReadDescriptor:f.move(r[0].er,task.descriptor());break;
case S::ReadParameter:f.move(r[1].rl,task.parameter());break;
case S::NormalizeParameterWord:r[1].r&=0xff;value(h,r[1].r);break;
case S::NormalizeParameterLong:r[1].er&=0xffff;value(h,r[1].er);break;
case S::BindEditContext:f.call(0x5a46);break;
case S::ReadEditBase:f.move(r[2].er,task.base());break;
case S::ReadPatchBase:f.move(r[2].er,task.base());break;
case S::LocateParameter:r[1].er=h.add32(r[1].er,r[2].er);break;
case S::ReadMinimum:f.move(r[2].rl,task.minimum());break;
case S::ReadMaximum:f.move(r[2].rh,task.maximum());break;
case S::ReadParameterValue:f.move(r[6].rh,task.value());break;
case S::CompareBitMode:h.sub8(r[2].rl,0xffffff80);break;
case S::CompareTriMode:h.sub8(r[2].rl,0xffffff81);break;
case S::AddDelta:r[6].rh=h.add8(r[6].rh,r[6].rl);break;
case S::TestDelta:f.move(r[6].rl,r[6].rl);break;
case S::CompareMaximum:h.sub8(r[6].rh,r[2].rh);break;
case S::ClampMaximum:f.move(r[6].rh,r[2].rh);break;
case S::CompareMinimum:h.sub8(r[6].rh,r[2].rl);break;
case S::ClampMinimum:f.move(r[6].rh,r[2].rl);break;
case S::TestBitDelta:f.move(r[6].rl,r[6].rl);break;
case S::SetBit:r[6].rh|=(1u<<(uint8_t(r[2].rh)&7));break;
case S::ClearBit:r[6].rh&=~(1u<<(uint8_t(r[2].rh)&7));break;
case S::TestTriDelta:f.move(r[6].rl,r[6].rl);break;
case S::CompareTriMiddle:h.sub8(r[6].rh,0x3f);break;
case S::SetTriHigh:f.move(r[6].rh,0x7f);break;
case S::SetTriMiddle:f.move(r[6].rh,0x3f);break;
case S::CompareTriZero:h.sub8(r[6].rh,0x0);break;
case S::CompareTriHigh:h.sub8(r[6].rh,0x7f);break;
case S::SetTriZero:f.move(r[6].rh,0x0);break;
case S::SetTriLow:f.move(r[6].rh,0x7f);break;
case S::PublishValue:f.call(0x6ae6);break;
case S::RefreshLabel:f.call(0x5976);break;
case S::ClearContext:f.call(0x5a7c);break;
case S::BindPanelSettings:f.move(r[6].er,0x168fc);break;
case S::ReadPanelLevel:f.move(r[6].rl,task.level());break;
case S::ApplyPanelLevel:f.call(0x1522);break;
case S::ReturnEdit:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateScreenEdit clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
