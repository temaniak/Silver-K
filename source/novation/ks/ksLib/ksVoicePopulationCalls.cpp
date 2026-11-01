#include "ksVoicePopulationCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
VoicePopulationPartialTest partialTest=nullptr;
using S=VoicePopulationTask::Stage;using W=VoicePopulationTask::Write;
struct Point {unsigned location,length;};
Point point(const VoicePopulationTask& task){switch(task.stage()){
 case S::DispatchVoice:return {0xa524,4};case S::AwaitVoice:case S::DispatchPart:return {0xa528,4};case S::AwaitPart:case S::WritePatch:return {0xa52c,10};
 case S::ReadNote:return {0xa536,6};case S::WriteNote:return {0xa53c,8};case S::ReadTranspose:return {0xa544,8};case S::ShiftTranspose:return {0xa54c,2};
 case S::TransposeNote:return {0xa54e,2};case S::WriteTransposed:return {0xa550,8};case S::ClearHeld:return {0xa558,2};case S::WriteHeld:return {0xa55a,8};
 case S::ReadUnison:return {0xa562,8};case S::WriteUnison:return {0xa56a,8};case S::ReadVelocity:return {0xa572,6};case S::ReadCurve:return {0xa578,8};
 case S::VelocityDefaults:return {0xa580,6};case S::ReadPower:return {0xa586,8};case S::PowerBias:return {0xa58e,2};case S::DispatchVelocity:return {0xa590,2};
 case S::AwaitVelocity:case S::WriteVelocity:return {0xa592,8};case S::InitialAge:return {0xa59a,6};case S::WriteAge:return {0xa5a0,10};
 case S::ReadPart:return {0xa5aa,6};case S::WritePart:return {0xa5b0,8};case S::DispatchMirror:return {0xa5b8,4};case S::AwaitMirror:case S::ReadExtra:return {0xa5bc,8};
 case S::WriteExtra:return {0xa5c4,8};case S::DispatchSelect:return {0xa5cc,4};case S::AwaitSelect:case S::PitchOffset:return {0xa5d0,4};case S::DispatchPitch:return {0xa5d4,4};
 case S::AwaitPitch:case S::PitchBias:return {0xa5d8,2};case S::ReadGlideNote:return {0xa5da,8};case S::Difference:return {0xa5e2,2};case S::AdmitDifference:return {0xa5e4,2};
 case S::Absolute:return {0xa5e6,2};case S::WriteDistance:return {0xa5e8,8};case S::ClearPending:return {0xa5f0,2};case S::WritePending:return {0xa5f2,8};case S::Complete:return {0xa5fa,2};
 }throw std::runtime_error("Invalid native population stage");}
uint8_t indexFrom(uint32_t pointer,uint32_t base){if(pointer<base||pointer>=base+20*256||(pointer-base)%20)throw std::runtime_error("Native population child handle invalid");return uint8_t((pointer-base)/20);}
unsigned outputAddress(const VoicePopulationTask& task,W what){const unsigned slot=0x17560 + 20*task.slot(),part=0x174ae + 20*task.controlPart();
 switch(what){case W::PatchReference:return slot;case W::Note:return slot+5;case W::Transposed:return slot+13;case W::Held:return part+17;
 case W::Unison:return slot+12;case W::Velocity:return slot+6;case W::Age:return slot+8;case W::Part:return slot+4;case W::Extra:return slot+18;case W::Distance:return slot+7;case W::Pending:return slot+17;}return 0;}
}
void setVoicePopulationPartialForTests(VoicePopulationPartialTest fn){partialTest=fn;}
bool VoicePopulationCalls::step(ControllerCpu& h){
 using namespace controllerDetail;using C=VoicePopulationTask::Child;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0xa524){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native population capacity exceeded");selected->emplace(uint32_t(h.regs[6].er),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;const auto child=task.child();if(child!=C::None){uint8_t result=0;
  if(child==C::Voice)result=indexFrom(uint32_t(h.regs[3].er),0x17560);else if(child==C::Part)result=indexFrom(uint32_t(h.regs[0].er),0x174ae);
  else if(child==C::Velocity)result=uint8_t(h.regs[1].rh);else if(child==C::Pitch)result=uint8_t(h.regs[4].rh);task.resumeChild(result);}
 const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task).length);auto& v=h.regs;
 if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
 const auto oldTranspose=task.transpose(),oldPower=task.power(),oldPitch=task.pitch(),oldDistance=task.distance();
 auto timing=[&](W what){const auto address=outputAddress(task,what);return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};
 const auto cost=task.nextClocks(task.bytePublication()?timing(task.publication()):3u);
 auto read=[&](VoicePopulationTask::Read what)->uint32_t{using R=VoicePopulationTask::Read;const unsigned slot=0x17560 + 20*task.slot(),part=0x174ae + 20*task.controlPart();
  switch(what){case R::Note:return uint8_t(h.read8(0x17528));case R::Transpose:return uint8_t(h.read8(part+14));case R::Unison:return uint8_t(h.read8(part+18));
  case R::Velocity:return uint8_t(h.read8(0x17529));case R::Curve:return uint8_t(h.read8(part+16));case R::Power:return uint8_t(h.read8(0x169f1));
  case R::Part:return uint8_t(h.read8(0x17526));case R::Extra:return uint8_t(h.read8(int32_t(call.patch+173)));case R::GlideNote:return uint8_t(h.read8(slot+13));}return 0;};
 auto write=[&](W what,uint32_t data){const auto address=outputAddress(task,what);if(what==W::PatchReference)h.write32(int32_t(call.patch),address);else if(what==W::Age)h.write32(int32_t(data),address);else h.write8(int8_t(data),address);};
 const auto progress=task.advanceBefore(cost,read,write,timing);if(progress.clocks!=cost||progress.boundaries!=1)throw std::runtime_error("Native population admission mismatch");
 auto dispatch=[&](unsigned target){clocks.call(target);h.pc=h.makepc(clocks.next);if(h.cycles-before!=cost)throw std::runtime_error("Native population dispatch clock mismatch");};
 switch(stage){
 case S::DispatchVoice:dispatch(0xa656);return true;case S::DispatchPart:dispatch(0xa66a);return true;
 case S::WritePatch:value(h,int32_t(call.patch));break;
 case S::ReadNote:v[1].rl=int8_t(task.note());value(h,v[1].rl);break;case S::WriteNote:value(h,int8_t(task.note()));break;
 case S::ReadTranspose:v[2].rl=int8_t(task.transpose());value(h,v[2].rl);break;
 case S::ShiftTranspose:h.add8(int8_t(oldTranspose),-64);v[2].rl=int8_t(task.transpose());break;
 case S::TransposeNote:h.add8(int8_t(task.note()),int8_t(task.transpose()));v[1].rl=int8_t(task.transposed());break;
 case S::WriteTransposed:value(h,int8_t(task.transposed()));break;case S::ClearHeld:v[1].rl=h.sub8(v[1].rl,v[1].rl);break;case S::WriteHeld:value(h,0);break;
 case S::ReadUnison:v[1].rl=int8_t(task.unison());value(h,v[1].rl);break;case S::WriteUnison:value(h,int8_t(task.unison()));break;
 case S::ReadVelocity:v[1].rh=int8_t(task.velocity());value(h,v[1].rh);break;case S::ReadCurve:v[2].rl=int8_t(task.curve());value(h,v[2].rl);break;
 case S::VelocityDefaults:v[0].er=0x168fc;value(h,v[0].er);break;case S::ReadPower:v[2].rh=int8_t(task.power());value(h,v[2].rh);break;
 case S::PowerBias:h.add8(int8_t(oldPower),2);v[2].rh=int8_t(task.power());break;case S::DispatchVelocity:dispatch(0xa5fc);return true;
 case S::WriteVelocity:value(h,int8_t(task.velocity()));break;case S::InitialAge:v[0].er=int32_t(task.initialAge());value(h,v[0].er);break;
 case S::WriteAge:value(h,int32_t(task.initialAge()));break;case S::ReadPart:v[0].rl=int8_t(task.part());value(h,v[0].rl);break;case S::WritePart:value(h,int8_t(task.part()));break;
 case S::DispatchMirror:dispatch(0xa67e);return true;case S::ReadExtra:v[0].rl=int8_t(task.extra());value(h,v[0].rl);break;case S::WriteExtra:value(h,int8_t(task.extra()));break;
 case S::DispatchSelect:dispatch(0x77f4);return true;case S::PitchOffset:v[4].r=int16_t(task.pitchOffset());value(h,v[4].r);break;
 case S::DispatchPitch:dispatch(0x784c);return true;case S::PitchBias:h.add8(int8_t(oldPitch),60);v[4].rh=int8_t(task.pitch());break;
 case S::ReadGlideNote:v[4].rl=int8_t(task.glideNote());value(h,v[4].rl);break;case S::Difference:h.sub8(int8_t(task.glideNote()),int8_t(task.pitch()));v[4].rl=int8_t(task.distance());break;
 case S::Absolute:h.sub8(0,int8_t(oldDistance));v[4].rl=int8_t(task.distance());break;case S::WriteDistance:value(h,int8_t(task.distance()));break;
 case S::ClearPending:v[0].rl=0;value(h,0);break;case S::WritePending:value(h,0);break;default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native population bus clock mismatch");h.pc=h.makepc(point(task).location);return true;
}
}
