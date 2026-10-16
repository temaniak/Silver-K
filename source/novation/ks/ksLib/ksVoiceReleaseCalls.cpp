#include "ksVoiceReleaseCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
VoiceReleasePartialTest partialTest=nullptr;
using S=VoiceReleaseTask::Stage;using M=VoiceReleaseTask::Mode;using W=VoiceReleaseTask::Write;
struct Point {unsigned location,length;};
Point point(const VoiceReleaseTask& task){
 const unsigned mode=task.mode()==M::Note?0:task.wholePart()?1:2;
 switch(mode){
 case 0:switch(task.stage()){
 case S::NoteInput:return {0x92c0,2};
 case S::ReadPart:return {0x92c2,6};
 case S::ZeroIndex:return {0x92c8,2};
 case S::PublishInitial:return {0x92ca,6};
 case S::ReadCount:return {0x92d0,6};
 case S::RetainRemaining:return {0x92d6,2};
 case S::RetainMatch:return {0x92d8,2};
 case S::DispatchVoice:return {0x92da,4};
 case S::ReadAge:return {0x92de,8};
 case S::AdmitAge:return {0x92e6,2};
 case S::ReadOwner:return {0x92e8,8};
 case S::CompareOwner:return {0x92f0,2};
 case S::AdmitOwner:return {0x92f2,2};
 case S::ReadNote:return {0x92f4,8};
 case S::CompareNote:return {0x92fc,2};
 case S::AdmitNote:return {0x92fe,2};
 case S::ReadExtra:return {0x9300,8};
 case S::AdmitExtra:return {0x9308,2};
 case S::ReadPatchEarly:return {0x930a,10};
 case S::ReadFlags:return {0x9314,8};
 case S::TestBypass:return {0x931c,2};
 case S::AdmitBypass:return {0x931e,2};
 case S::DispatchParameters:return {0x9320,4};
 case S::ReadPedal:return {0x9324,8};
 case S::AdmitPedal:return {0x932c,2};
 case S::WritePending:return {0x932e,8};
 case S::SkipHeld:return {0x9336,2};
 case S::MaskAge:return {0x9338,2};
 case S::MarkAge:return {0x933a,2};
 case S::CaptureAge:return {0x933c,2};
 case S::SetLowAge:return {0x933e,4};
 case S::WriteAge:return {0x9342,10};
 case S::DispatchSelect:return {0x934c,4};
 case S::DispatchGate:return {0x9350,4};
 case S::ReadIndex:return {0x9354,6};
 case S::TickIndex:return {0x935a,2};
 case S::PublishIndex:return {0x935c,6};
 case S::RestoreMatch:return {0x9362,2};
 case S::RestoreRemaining:return {0x9364,2};
 case S::TickRemaining:return {0x9366,2};
 case S::AdmitRemaining:return {0x9368,4};
 case S::Complete:return {0x936c,2};
 case S::AwaitVoice:return {0x92de,8};
 case S::AwaitParameters:return {0x9324,8};
 case S::AwaitSelect:return {0x9350,4};
 case S::AwaitCut:return {0x9350,4};
 case S::AwaitGate:return {0x9354,6};
 case S::AwaitClear:return {0x936c,2};
 default:break;}break;
 case 1:switch(task.stage()){
 case S::FastCut:return {0x936e,4};
 case S::FastSkip:return {0x9372,2};
 case S::DefaultCut:return {0x9374,2};
 case S::ReadPart:return {0x9376,6};
 case S::ZeroIndex:return {0x937c,2};
 case S::PublishInitial:return {0x937e,6};
 case S::ReadCount:return {0x9384,6};
 case S::RetainRemaining:return {0x938a,2};
 case S::RetainMatch:return {0x938c,2};
 case S::RetainPolicy:return {0x938e,2};
 case S::DispatchVoice:return {0x9390,4};
 case S::ReadAge:return {0x9394,8};
 case S::AdmitAge:return {0x939c,2};
 case S::ReadOwner:return {0x939e,8};
 case S::CompareOwner:return {0x93a6,2};
 case S::AdmitOwner:return {0x93a8,2};
 case S::MaskAge:return {0x93aa,2};
 case S::MarkAge:return {0x93ac,2};
 case S::CaptureAge:return {0x93ae,2};
 case S::SetLowAge:return {0x93b0,4};
 case S::WriteAge:return {0x93b4,10};
 case S::ReadPatchLate:return {0x93be,10};
 case S::DispatchSelect:return {0x93c8,4};
 case S::InspectPolicy:return {0x93cc,2};
 case S::RetainInspectedPolicy:return {0x93ce,2};
 case S::AdmitPolicy:return {0x93d0,2};
 case S::SetCutWord:return {0x93d2,4};
 case S::CutOffset:return {0x93d6,4};
 case S::DispatchCut:return {0x93da,4};
 case S::DispatchGate:return {0x93de,4};
 case S::ReadIndex:return {0x93e2,6};
 case S::TickIndex:return {0x93e8,2};
 case S::PublishIndex:return {0x93ea,6};
 case S::RestorePolicy:return {0x93f0,2};
 case S::RestoreMatch:return {0x93f2,2};
 case S::RestoreRemaining:return {0x93f4,2};
 case S::TickRemaining:return {0x93f6,2};
 case S::AdmitRemaining:return {0x93f8,2};
 case S::DispatchClear:return {0x93fa,4};
 case S::Complete:return {0x93fe,2};
 case S::AwaitVoice:return {0x9394,8};
 case S::AwaitSelect:return {0x93cc,2};
 case S::AwaitCut:return {0x93de,4};
 case S::AwaitGate:return {0x93e2,6};
 case S::AwaitClear:return {0x93fe,2};
 default:break;}break;
 case 2:switch(task.stage()){
 case S::ReadPart:return {0x9400,6};
 case S::ZeroIndex:return {0x9406,2};
 case S::PublishInitial:return {0x9408,6};
 case S::ReadCount:return {0x940e,6};
 case S::RetainRemaining:return {0x9414,2};
 case S::RetainMatch:return {0x9416,2};
 case S::DispatchVoice:return {0x9418,4};
 case S::ReadAge:return {0x941c,8};
 case S::AdmitAge:return {0x9424,2};
 case S::ReadOwner:return {0x9426,8};
 case S::CompareOwner:return {0x942e,2};
 case S::AdmitOwner:return {0x9430,2};
 case S::ReadPending:return {0x9432,8};
 case S::AdmitPending:return {0x943a,2};
 case S::ClearPending:return {0x943c,2};
 case S::WriteClearPending:return {0x943e,8};
 case S::MaskAge:return {0x9446,2};
 case S::MarkAge:return {0x9448,2};
 case S::CaptureAge:return {0x944a,2};
 case S::SetLowAge:return {0x944c,4};
 case S::WriteAge:return {0x9450,10};
 case S::ReadPatchLate:return {0x945a,10};
 case S::DispatchSelect:return {0x9464,4};
 case S::DispatchGate:return {0x9468,4};
 case S::ReadIndex:return {0x946c,6};
 case S::TickIndex:return {0x9472,2};
 case S::PublishIndex:return {0x9474,6};
 case S::RestoreMatch:return {0x947a,2};
 case S::RestoreRemaining:return {0x947c,2};
 case S::TickRemaining:return {0x947e,2};
 case S::AdmitRemaining:return {0x9480,2};
 case S::Complete:return {0x9482,2};
 case S::AwaitVoice:return {0x941c,8};
 case S::AwaitSelect:return {0x9468,4};
 case S::AwaitCut:return {0x9468,4};
 case S::AwaitGate:return {0x946c,6};
 case S::AwaitClear:return {0x9482,2};
 default:break;}break;
 }throw std::runtime_error("Invalid native release stage");}
uint8_t indexFrom(uint32_t value,unsigned base,unsigned stride){if(value<base||value>=base+stride*256||(value-base)%stride)throw std::runtime_error("Invalid native release child binding");return uint8_t((value-base)/stride);}
unsigned outputAddress(const VoiceReleaseTask& task,W what){if(what==W::Index)return 0x174a7;return 0x17560 + unsigned(task.slot())*20+(what==W::Age?8:17);}
}
void setVoiceReleasePartialForTests(VoiceReleasePartialTest fn){partialTest=fn;}
bool VoiceReleaseCalls::step(ControllerCpu& h){using namespace controllerDetail;using C=VoiceReleaseTask::Child;
 const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 if(pc==0x92c0||pc==0x936e||pc==0x9374||pc==0x9400||(pc==0x9376&&!selected)){
  if(selected)++restarted;
  if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native release capacity exceeded");
  const M mode=pc==0x92c0?M::Note:pc==0x936e?M::FastPart:pc==0x9374?M::Part:pc==0x9400?M::Pedal:M::ProvidedPart;
  selected->emplace(mode,uint8_t(h.regs[5].rl),uint16_t(h.regs[2].r),h.getSP());++started;
 }
 if(!selected||point((*selected)->task).location!=pc){if(selected){selected->reset();++abandoned;}if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& v=h.regs;const auto child=task.child();if(child!=C::None){uint8_t result=0;
  if(child==C::Voice)result=indexFrom(uint32_t(v[3].er),0x17560,20);else if(child==C::Parameters)result=indexFrom(uint32_t(v[0].er),0x160d2,334);task.resumeChild(result);}
 const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task).length);
 if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
 auto timing=[&](W what){const auto address=outputAddress(task,what);return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};
 const auto cost=task.nextClocks(task.publication()?timing(task.publicationKind()):3u);
 auto read=[&](VoiceReleaseTask::Read what)->uint32_t{using R=VoiceReleaseTask::Read;const unsigned slot=0x17560 + unsigned(task.slot())*20;
  switch(what){case R::Part:return uint8_t(h.read8(0x17526));case R::Count:return uint8_t(h.read8(0x174a6));
  case R::Age:return uint16_t(h.read16(slot+8));case R::Owner:return uint8_t(h.read8(slot+4));case R::Note:return uint8_t(h.read8(slot+5));
  case R::Extra:return uint8_t(h.read8(slot+18));case R::PatchReference:call.patch=uint32_t(h.read32(slot));return 0;
  case R::Flags:return uint8_t(h.read8(int32_t(call.patch+88)));case R::Pedal:return uint8_t(h.read8(0x160d2 + unsigned(task.parameterPart())*334+309));
  case R::Pending:return uint8_t(h.read8(slot+17));case R::Index:return uint8_t(h.read8(0x174a7));}return 0;};
 auto write=[&](W what,uint32_t data){const auto address=outputAddress(task,what);if(what==W::Age)h.write32(int32_t(data),address);else h.write8(int8_t(data),address);};
 if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Native release admission mismatch");
 auto dispatch=[&](unsigned target){clocks.call(target);h.pc=h.makepc(clocks.next);if(h.cycles-before!=cost)throw std::runtime_error("Native release dispatch clock mismatch");};
 switch(stage){case S::NoteInput:v[1].rh=int8_t(task.note());value(h,v[1].rh);break;
 case S::DefaultCut:v[2].r=h.sub16(v[2].r,v[2].r);break;case S::FastCut:v[2].r=1;value(h,v[2].r);break;
 case S::ReadPart:v[1].rl=int8_t(task.part());value(h,v[1].rl);break;case S::ZeroIndex:v[0].rl=0;value(h,0);break;
 case S::PublishInitial:value(h,0);break;case S::ReadCount:v[0].rl=int8_t(task.remaining());value(h,v[0].rl);break;
 case S::RetainRemaining:clocks.push(v[0].r);call.depth+=2;break;case S::RetainMatch:clocks.push(v[1].r);call.depth+=2;break;
 case S::RetainPolicy:clocks.push(v[2].r);call.depth+=2;break;case S::DispatchVoice:dispatch(0xa656);return true;
 case S::ReadAge:v[2].r=int16_t(task.age());value(h,v[2].r);break;
 case S::ReadOwner:v[0].rl=int8_t(task.owner());value(h,v[0].rl);break;case S::CompareOwner:h.sub8(v[1].rl,v[0].rl);break;
 case S::ReadNote:v[0].rl=int8_t(task.slotNote());value(h,v[0].rl);break;case S::CompareNote:h.sub8(v[1].rh,v[0].rl);break;
 case S::ReadExtra:v[0].rl=int8_t(task.extra());value(h,v[0].rl);break;
 case S::ReadPatchEarly:case S::ReadPatchLate:v[6].er=int32_t(call.patch);value(h,v[6].er);break;
 case S::ReadFlags:v[0].rl=int8_t(task.flags());value(h,v[0].rl);break;case S::TestBypass:flag(h,4,!(task.flags()&4));break;
 case S::DispatchParameters:dispatch(0x4000);return true;case S::ReadPedal:v[0].rl=int8_t(task.pedal());value(h,v[0].rl);break;
 case S::WritePending:value(h,int8_t(task.pedal()));break;case S::ReadPending:v[0].rl=int8_t(task.pending());value(h,v[0].rl);break;
 case S::ClearPending:v[0].rl=0;value(h,0);break;case S::WriteClearPending:value(h,0);break;
 case S::MaskAge:v[2].rh&=0x3f;value(h,v[2].rh);break;case S::MarkAge:v[2].rl|=1;value(h,v[2].rl);break;
 case S::CaptureAge:v[2].e=v[2].r;value(h,v[2].e);break;case S::SetLowAge:v[2].r=-1;value(h,v[2].r);break;
 case S::WriteAge:value(h,int32_t(task.releaseAge()));break;case S::DispatchSelect:dispatch(0x77f4);return true;
 case S::InspectPolicy:clocks.pop(v[0].r);call.depth-=2;break;case S::RetainInspectedPolicy:clocks.push(v[0].r);call.depth+=2;break;
 case S::SetCutWord:v[1].e=0x1000;value(h,v[1].e);break;case S::CutOffset:v[4].r=0x72;value(h,v[4].r);break;
 case S::DispatchCut:dispatch(0x785a);return true;case S::DispatchGate:dispatch(0x8fc0);return true;
 case S::ReadIndex:v[0].rl=int8_t(task.index());value(h,v[0].rl);break;case S::TickIndex:clocks.inc(v[0].rl,1);break;
 case S::PublishIndex:value(h,int8_t(task.index()));break;case S::RestorePolicy:clocks.pop(v[2].r);call.depth-=2;break;
 case S::RestoreMatch:clocks.pop(v[1].r);call.depth-=2;break;case S::RestoreRemaining:clocks.pop(v[0].r);call.depth-=2;break;
 case S::TickRemaining:clocks.inc(v[0].rl,-1);break;case S::AdmitRemaining:if(task.mode()==M::Note)h.clockI(2);break;
 case S::DispatchClear:dispatch(0x30b8);return true;default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native release bus clocks differ");h.pc=h.makepc(point(task).location);return true;
}
}
