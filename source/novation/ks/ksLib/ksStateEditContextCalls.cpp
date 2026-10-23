#include "ksStateEditContextCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateEditContextPartialTest partialTest=nullptr;using T=StateEditContextTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,46> points{{{0x5a46,4},{0x5a4a,2},{0x5a4c,6},{0x5a52,8},{0x5a5a,2},{0x5a5c,2},{0x5a5e,6},{0x5a64,2},{0x5a66,2},{0x5a68,2},{0x5a6a,2},{0x5a6c,8},{0x5a74,2},{0x5a76,4},{0x5a7a,2},{0x5a7c,4},{0x5a80,2},{0x5a82,8},{0x5a8a,4},{0x5a8e,2},{0x5a90,2},{0x5a92,2},{0x5a94,2},{0x5a96,8},{0x5a9e,2},{0x5aa0,8},{0x5aa8,2},{0x5aaa,2},{0x5aac,8},{0x5ab4,2},{0x5ab6,2},{0x5ab8,2},{0x5aba,4},{0x5abe,2},{0x5ac0,2},{0x5ac2,4},{0x5ac6,4},{0x5aca,2},{0x5acc,2},{0x5ace,2},{0x5ad0,6},{0x5ad6,4},{0x5ada,2},{0x5adc,2},{0x5ade,2},{0x5ae0,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){return at<0x400000&&!(previous&1)&&at==(unsigned(previous)|1u)?0:3;}
}
void setStateEditContextPartialForTests(StateEditContextPartialTest value){partialTest=value;}
bool StateEditContextCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x5a46:kind=T::Kind::Bind;break;case 0x5a7c:kind=T::Kind::Clear;break;case 0x5a90:kind=T::Kind::Display;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateEditContext capacity exceeded");slot->emplace(*kind,h.getSP(),uint32_t(h.regs[1].er));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint8_t(r[0].rl));const auto stage=task.stage();
 const auto store=byteClocks(0,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field,uint32_t ref)->uint32_t{switch(field){case T::Read::ContextKind:return uint8_t(h.read8(ref+71586));case T::Read::Screen:return uint8_t(h.read8(0x16785));case T::Read::EditBase:return uint32_t(h.read32(0x167a6));case T::Read::PatchBase:return uint32_t(h.read32(0x167bc));case T::Read::Value:return uint8_t(h.read8(ref));case T::Read::ValueKind:return uint8_t(h.read8(ref+68974));}return 0;};auto write=[&](T::Write,uint32_t v){f.store32(0x167a6,int32_t(v));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateEditContext admission differs");
 switch(stage){case S::SaveContextValue:f.push(r[0].er);break;
case S::SaveContextIndex:f.push(r[1].r);break;
case S::BindGlobalBase:f.move(r[0].er,0x168fc);break;
case S::ReadContextKind:f.move(r[1].rl,task.kind());break;
case S::CompareContextKind:h.sub8(r[1].rl,0x7e);break;
case S::ReadScreen:f.move(r[1].rl,task.screen());break;
case S::CompareCalibrationScreen:h.sub8(r[1].rl,0x9);break;
case S::ClearContextBase:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::RestoreContextIndex:f.pop(r[1].r);break;
case S::RestoreContextValue:f.pop(r[0].er);break;
case S::ReturnContext:f.ret();break;
case S::SaveClearValue:f.push(r[0].er);break;
case S::ClearBase:r[0].er=h.sub32(r[0].er,r[0].er);break;
case S::RestoreClearValue:f.pop(r[0].er);break;
case S::ReturnClear:f.ret();break;
case S::NormalizeValueIndex:r[1].r&=0xff;value(h,r[1].r);break;
case S::NormalizeValueLong:r[1].er&=0xffff;value(h,r[1].er);break;
case S::RetainValueIndex:f.move(r[3].er,r[1].er);break;
case S::ReadEditBase:f.move(r[0].er,task.base());break;
case S::ReadPatchBase:f.move(r[0].er,task.base());break;
case S::LocateValue:r[0].er=h.add32(r[0].er,r[1].er);break;
case S::ReadValue:f.move(r[0].rl,task.value());break;
case S::ReadValueKind:f.move(r[1].rl,task.kind());break;
case S::BindComparedValue:f.move(r[1].rh,r[0].rl);break;
case S::SaveComparedValue:f.push(r[1].r);break;
case S::SaveComparedIndex:f.push(r[3].er);break;
case S::BindValuePosition:f.move(r[6].rl,0xffffff91);break;
case S::DisplayCurrentValue:f.call(0x5ae2);break;
case S::RestoreComparedIndex:f.pop(r[1].er);break;
case S::ReadReferenceValue:f.call(0x57e2);break;
case S::RestoreComparedValue:f.pop(r[1].r);break;
case S::CompareReferenceValue:h.sub8(r[1].rh,r[0].rl);break;
case S::BindMatchText:f.move(r[6].er,0x14f68);break;
case S::PrintMatchText:f.call(0x9b8);break;
case S::BindReferencePosition:f.move(r[6].rl,0xffffffd1);break;
case S::DisplayReferenceValue:f.call(0x5ae2);break;
case S::ReturnValue:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateEditContext clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
