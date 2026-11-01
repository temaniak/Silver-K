#include "ksStateStoredValueCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateStoredValuePartialTest partialTest=nullptr;using T=StateStoredValueTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,29> points{{{0x57e2,2},{0x57e4,2},{0x57e6,2},{0x57e8,2},{0x57ea,6},{0x57f0,2},{0x57f2,2},{0x57f4,2},{0x57f6,2},{0x57f8,2},{0x57fa,4},{0x57fe,2},{0x5800,4},{0x5804,2},{0x5806,2},{0x5808,8},{0x5810,2},{0x5812,2},{0x5814,2},{0x5816,2},{0x5818,2},{0x581a,2},{0x581c,2},{0x581e,2},{0x5820,2},{0x5822,2},{0x5824,2},{0x5826,2},{0x5828,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateStoredValuePartialForTests(StateStoredValuePartialTest value){partialTest=value;}
bool StateStoredValueCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x57e2:kind=T::Kind::Read;break;case 0x5816:kind=T::Kind::Classify;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateStoredValue capacity exceeded");slot->emplace(*kind,h.getSP(),uint32_t(h.regs[1].er));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint32_t(r[0].er),bool(h.ccr&4));const auto stage=task.stage();
 auto accessCost=[&](uint32_t ref,unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){const unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};unsigned store=3;switch(stage){case S::ReadMultiBase:store=accessCost(uint32_t(0x167c0),4,-1);break;case S::ReadStoredField:store=accessCost(uint32_t(r[0].er),1,-1);break;default:break;} const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref){return field==T::Read::Multi?uint32_t(h.read32(0x167c0)):uint32_t(uint8_t(h.read8(ref)));};auto write=[](int,int){};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateStoredValue admission differs");
 switch(stage){case S::CompareGlobalField:h.sub8(r[1].rl,0xffffffde);break;
case S::CompareUnusedField:h.sub8(r[1].rl,0xffffffea);break;
case S::BindGlobalStorage:f.move(r[0].er,0x81fe16);break;
case S::ClearUnusedValue:f.move(r[0].rl,0x0);break;
case S::ClassifyPatchField:f.call(0x5816);break;
case S::ResolveAlternatePatch:f.call(0x63bc);break;
case S::ResolvePatch:f.call(0x63a4);break;
case S::CompareMultiField:h.sub8(r[1].rl,0xffffffd1);break;
case S::ReadMultiBase:f.move(r[0].er,task.base());break;
case S::LocateStoredField:r[0].er=h.add32(r[0].er,r[1].er);break;
case S::ReadStoredField:f.move(r[0].rl,task.value());break;
case S::ReturnStoredValue:f.ret();break;
case S::CompareSpecialHigh:h.sub8(r[1].rl,0xffffffd4);break;
case S::CompareSpecialSentinel:h.sub8(r[1].rl,0x7e);break;
case S::CompareSpecialLow:h.sub8(r[1].rl,0x54);break;
case S::CompareSpecialEnd:h.sub8(r[1].rl,0x7c);break;
case S::MarkSpecial:h.sub8(r[1].rl,r[1].rl);break;
case S::ReturnSpecial:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateStoredValue clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
