#include "ksMemoryDiagnosticsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
MemoryDiagnosticsPartialTest partialTest=nullptr;using T=MemoryDiagnosticsTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,61> points{{{0x1ace,6},{0x1ad4,4},{0x1ad8,6},{0x1ade,6},{0x1ae4,2},{0x1ae6,2},{0x1ae8,2},{0x1aea,2},{0x1aec,6},{0x1af2,4},{0x1af6,2},{0x1af8,6},{0x1afe,4},{0x1b02,2},{0x1b04,4},{0x1b08,2},{0x1b0a,4},{0x1b0e,2},{0x1b10,4},{0x1b14,4},{0x1b18,2},{0x1b1a,2},{0x1b1c,2},{0x1b1e,2},{0x1b20,2},{0x1b22,2},{0x1b24,2},{0x1b26,2},{0x1b28,4},{0x1b2c,4},{0x1b30,2},{0x1b32,2},{0x1b34,2},{0x1b36,2},{0x1b38,4},{0x1b3c,2},{0x1b3e,4},{0x1b42,2},{0x1b44,4},{0x1b48,2},{0x1b4a,4},{0x1b4e,2},{0x1b50,2},{0x1b52,2},{0x1b54,2},{0x1b56,2},{0x1b58,2},{0x1b5a,2},{0x1b5c,2},{0x1b5e,2},{0x1b60,2},{0x1b62,2},{0x1b64,2},{0x1b66,2},{0x1b68,2},{0x1b6a,2},{0x1b6c,2},{0x1b6e,2},{0x1b70,2},{0x1b72,2},{0x230a,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setMemoryDiagnosticsPartialForTests(MemoryDiagnosticsPartialTest value){partialTest=value;}
bool MemoryDiagnosticsCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x1ace:kind=T::Kind::Memory;break;case 0x1b10:kind=T::Kind::Patterns;break;case 0x1b36:kind=T::Kind::Pair;break;case 0x1b50:kind=T::Kind::Pass;break;case 0x230a:kind=T::Kind::ControllerFault;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native MemoryDiagnostics capacity exceeded");slot->emplace(*kind,h.getSP(),uint32_t(h.regs[5].er),uint32_t(h.regs[6].er),uint16_t(h.regs[0].r),uint16_t(h.regs[0].e),uint16_t(h.regs[1].e),uint8_t(h.regs[1].rl),uint8_t(h.regs[1].rh));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint8_t(r[1].rl));const auto stage=task.stage();
 const unsigned ref=task.cursor()&0xffffff;const auto store=stage==S::ReadPassPattern?(ref<0x400000?3u+3u*(ref&1):6u):byteClocks(ref,h.lastwrite)+byteClocks(ref+1,int(ref));const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read,uint32_t ref)->uint16_t{return uint16_t(h.read16(ref));};auto write=[&](T::Write,uint32_t ref,uint16_t value){f.store16(ref,int16_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native MemoryDiagnostics admission differs");
 switch(stage){case S::BindMemoryNotice:f.move(r[6].er,0xba70);break;
case S::ShowMemoryNotice:f.call(0x9b8);break;
case S::BindMemoryBase:f.move(r[5].er,0xa00000);break;
case S::BindMemoryLength:f.move(r[6].er,0x10000);break;
case S::BindMemoryRounds:f.move(r[1].rh,0x2);break;
case S::TestMemory:f.call(0x1b10);break;
case S::TestMemoryFailure:flag(h,4,(uint8_t(r[1].rl)&(1u<<0x2))==0);break;
case S::BindMemorySuccess:f.move(r[6].er,0xba81);break;
case S::ShowMemorySuccess:f.call(0x9b8);break;
case S::ReturnMemorySuccess:f.ret();break;
case S::BindMemoryFailure:f.move(r[6].er,0xba8d);break;
case S::ShowMemoryFailure:f.call(0x9b8);break;
case S::BindFailureHigh:f.move(r[0].r,r[2].e);break;
case S::ShowFailureHigh:f.call(0xab8);break;
case S::BindFailureLow:f.move(r[0].r,r[2].r);break;
case S::ShowFailureLow:f.call(0xab8);break;
case S::BindPatternStart:f.move(r[0].r,0x0);break;
case S::BindPatternStride:f.move(r[0].e,0x1);break;
case S::ClearPatternMask:r[1].e=h.sub16(r[1].e,r[1].e);break;
case S::TestPatternPair:f.call(0x1b36);break;
case S::TestFirstFailure:flag(h,4,(uint8_t(r[1].rl)&(1u<<0x2))==0);break;
case S::InvertPatternMask:r[1].e=~r[1].e;value(h,r[1].e);break;
case S::TestInversePair:f.call(0x1b36);break;
case S::TestInverseFailure:flag(h,4,(uint8_t(r[1].rl)&(1u<<0x2))==0);break;
case S::NextPatternStart:r[0].r=h.add16(r[0].r,0xd05);break;
case S::NextPatternStride:r[0].e=h.add16(r[0].e,0x4d);break;
case S::DecrementPatternRounds:f.inc(r[1].rh,-1);break;
case S::ReturnPattern:f.ret();break;
case S::BindWritePass:f.move(r[1].rl,0x0);break;
case S::SaveWritePattern:f.push(r[0].er);break;
case S::WritePattern:f.call(0x1b50);break;
case S::RestoreWritePattern:f.pop(r[0].er);break;
case S::BindVerifyPass:f.move(r[1].rl,0x1);break;
case S::SaveVerifyPattern:f.push(r[0].er);break;
case S::VerifyPattern:f.call(0x1b50);break;
case S::RestoreVerifyPattern:f.pop(r[0].er);break;
case S::ReturnPatternPair:f.ret();break;
case S::BindPassCursor:f.move(r[2].er,r[5].er);break;
case S::BindPassCount:f.move(r[3].er,r[6].er);break;
case S::BindPassPattern:f.move(r[4].r,r[0].r);break;
case S::MixPassPattern:r[4].r^=r[1].e;value(h,r[4].r);break;
case S::TestPassMode:flag(h,4,(uint8_t(r[1].rl)&(1u<<0x0))==0);break;
case S::ReadPassPattern:f.move(r[4].e,task.actual());break;
case S::ComparePassPattern:h.sub16(r[4].r,r[4].e);break;
case S::MarkPassFailure:r[1].rl|=(1u<<0x2);break;
case S::ReturnPassFailure:f.ret();break;
case S::AdvancePassPattern:r[0].r=h.add16(r[0].r,r[0].e);break;
case S::AdvancePassCursor:f.inc(r[2].er,2);break;
case S::DecrementPassCount:f.inc(r[3].er,-1);break;
case S::ReturnPass:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native MemoryDiagnostics clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
