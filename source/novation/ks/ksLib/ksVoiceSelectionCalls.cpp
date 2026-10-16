#include "ksVoiceSelectionCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
VoiceSelectionPartialTest partialTest=nullptr;
using S=VoiceSelectionTask::Stage;using K=VoiceSelectionTask::Search;
struct Point {unsigned location,length;};
Point point(const VoiceSelectionTask& task){
 const auto search=task.search();const unsigned k=search==K::Owner?0:search==K::NewestOwner?1:search==K::Free?2:3;
 auto select=[&](unsigned a,unsigned b,unsigned c,unsigned d,unsigned length){const unsigned points[]{a,b,c,d};return Point{points[k],length};};
 switch(task.stage()){
 case S::DispatchPart:return {0xa338,4};case S::AwaitPart:case S::ReadMode:return {0xa33c,8};
 case S::ShiftModeTwo:return {0xa344,2};case S::ShiftModeOne:return {0xa346,2};case S::MaskMode:return {0xa348,2};
 case S::ReadHeld:return {0xa34a,8};case S::ReadUnison:return {0xa352,8};case S::ReadPart:return {0xa35a,6};case S::CaptureOwner:return {0xa360,2};
 case S::ResetCandidate:return {0xa362,2};case S::PublishInitialIndex:return {0xa364,6};case S::ReadCount:return {0xa36a,6};case S::InitializeAge:return {0xa370,6};
 case S::CompareMode:return {0xa376,2};case S::AdmitMode:return {0xa378,4};case S::TestHeld:return {0xa37c,2};case S::AdmitHeld:return {0xa37e,2};
 case S::SetHeldAllocation:return {0xa380,2};case S::JumpHeldAllocation:return {0xa382,4};case S::SetReuseAllocation:return {0xa386,2};
 case S::PublishReuseAllocation:return {0xa388,6};case S::SetOwnerAllocation:return {0xa38e,2};
 case S::CompareOtherMode:return {0xa44e,2};case S::AdmitOtherMode:return {0xa450,2};case S::SetPolyAllocation:return {0xa452,2};case S::PublishFreeAllocation:return {0xa454,6};
 case S::ReadIncoming:return {0xa496,6};case S::SetNoteAllocation:return {0xa49c,2};case S::PublishNoteAllocation:return {0xa49e,6};
 case S::DispatchVoice:return select(0xa390,0xa3e8,0xa45a,0xa4a4,4);
 case S::AwaitVoice:case S::ReadAge:return select(0xa394,0xa3ec,0xa45e,0xa4a8,10);
 case S::ReadOwner:return select(0xa39e,0xa3f6,0,0xa4b6,8);case S::ReadSlotUnison:return select(0xa3a6,0xa3fe,0,0xa4be,8);
 case S::CompareOwner:return select(0xa3ae,0xa406,0,0xa4c6,2);case S::AdmitOwner:return select(0xa3b0,0xa408,0,0xa4c8,2);
 case S::ResetNewestAge:return {0xa3e6,2};case S::TestAgeHigh:return {search==K::Free?0xa468u:0xa4b2u,2};
 case S::AdmitAgeHigh:return {search==K::Free?0xa46au:0xa4b4u,search==K::Free?4u:2u};
 case S::CaptureHighAge:return {0xa40a,2};case S::MaskActiveAge:return {0xa40c,2};case S::AdmitActiveAge:return {0xa40e,4};
 case S::ReadNote:return {0xa4ca,8};case S::CompareNote:return {0xa4d2,2};case S::AdmitNote:return {0xa4d4,2};
 case S::SetRetrigger:return {0xa4d6,2};case S::PublishRetrigger:return {0xa4d8,6};case S::JumpRetrigger:return {0xa4de,2};
 case S::CompareAge:return select(0xa3b2,0xa412,0xa46e,0xa4e0,2);case S::AdmitAge:return select(0xa3b4,0xa414,0xa470,0xa4e2,2);
 case S::CopyAge:return select(0xa3b6,0xa416,0xa472,0xa4e4,2);case S::ReadCandidate:return select(0xa3b8,0xa418,0xa474,0xa4e6,6);
 case S::ReadIndex:return select(0xa3be,0xa41e,0xa47a,0xa4ec,6);case S::IndexTick:return select(0xa3c4,0xa424,0xa480,0xa4f2,2);
 case S::PublishIndex:return select(0xa3c6,0xa426,0xa482,0xa4f4,6);case S::RemainingTick:return select(0xa3cc,0xa42c,0xa488,0xa4fa,2);
 case S::RemainingGate:return select(0xa3ce,0xa42e,0xa48a,0xa4fc,2);case S::PublishCandidate:return select(0xa3d0,0xa430,0xa48c,0xa4fe,6);
 case S::PublishOwnerAllocation:return {0xa3d6,6};case S::TestBestHigh:return select(0xa3dc,0xa436,0,0xa504,2);
 case S::AdmitBestHigh:return select(0xa3de,0xa438,0,0xa506,search==K::Owner?4:2);
 case S::JumpPopulation:return {search==K::Owner?0xa3e2u:0xa44au,4};case S::SetNewestThree:return {0xa43a,2};
 case S::CompareHighAge:return {0xa43c,4};case S::AdmitHighAge:return {0xa440,2};case S::SetNewestTwo:return {0xa442,2};case S::PublishNewestAllocation:return {0xa444,6};
 case S::JumpRelease:return {0xa492,4};case S::DispatchSelect:return {0xa508,4};case S::AwaitSelect:case S::ReleaseWord:return {0xa50c,4};
 case S::ReleaseAddressOne:return {0xa510,4};case S::DispatchWriteOne:return {0xa514,4};case S::AwaitWriteOne:case S::ReleaseAddressTwo:return {0xa518,4};
 case S::DispatchWriteTwo:return {0xa51c,4};case S::AwaitWriteTwo:case S::DispatchGate:return {0xa520,4};case S::AwaitGate:case S::Complete:return {0xa524,4};
 }throw std::runtime_error("Invalid native voice-selection stage");
}
Point point(const VoiceGateOffTask& task){using G=VoiceGateOffTask::Stage;
 switch(task.stage()){case G::Value:return {0x8fc0,2};case G::Offset:return {0x8fc2,4};case G::Dispatch:return {0x8fc6,4};case G::Await:case G::Complete:return {0x8fca,2};}
 throw std::runtime_error("Invalid native gate-off stage");}
uint8_t indexFrom(uint32_t pointer,uint32_t base){if(pointer<base||pointer>=base+20*256||(pointer-base)%20)throw std::runtime_error("Native voice child handle invalid");return uint8_t((pointer-base)/20);}
}
void setVoiceSelectionPartialForTests(VoiceSelectionPartialTest fn){partialTest=fn;}
bool VoiceSelectionCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto partial=[&]{if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;};
 if(pc>=0x8fc0&&pc<=0x8fca){
  using G=VoiceGateOffTask::Stage;std::optional<Gate>* selected=nullptr;for(auto& c:gates)if(c&&c->token==h.getSP()){selected=&c;break;}
  if(pc==0x8fc0){if(!selected)for(auto& c:gates)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native gate-off capacity exceeded");selected->emplace(h.getSP());++gateStarted;}
  if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
  auto& task=(*selected)->task;if(task.stage()==G::Await)task.resumeChild();const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task).length);
  if(stage==G::Complete){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++gateCompleted;
   for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.child()==VoiceSelectionTask::Child::Gate){if(unsigned(h.getPC())!=0xa524)throw std::runtime_error("Native selection gate returned to wrong caller");c->task.resumeChild();c.reset();++completed;break;}return true;}
  const auto cost=task.nextClocks();if(!task.advanceBefore(cost))throw std::runtime_error("Native gate-off admission mismatch");
  switch(stage){case G::Value:h.regs[1].er=h.sub32(h.regs[1].er,h.regs[1].er);break;case G::Offset:h.regs[4].r=int16_t(task.offset());value(h,h.regs[4].r);break;
  case G::Dispatch:clocks.call(0x785a);h.pc=h.makepc(clocks.next);if(h.cycles-before!=cost)throw std::runtime_error("Native gate dispatch clock mismatch");return true;default:break;}
  h.pc=h.makepc(point(task).location);if(h.cycles-before!=cost)throw std::runtime_error("Native gate-off clock mismatch");return true;
 }
 std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0xa338){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native selection capacity exceeded");selected->emplace(uint32_t(h.regs[6].er),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
 auto& call=**selected;auto& task=call.task;
 const auto child=task.child();if(child!=VoiceSelectionTask::Child::None){uint8_t result=0;if(child==VoiceSelectionTask::Child::Part)result=indexFrom(uint32_t(h.regs[0].er),0x174ae);else if(child==VoiceSelectionTask::Child::Voice)result=indexFrom(uint32_t(h.regs[3].er),0x17560);task.resumeChild(result);}
 const auto stage=task.stage();const auto before=h.cycles;const auto oldMode=task.modeBits(),oldIndex=task.index(),oldRemaining=task.remaining();Boundary clocks(h,pc,point(task).length);auto& v=h.regs;
 auto timing=[&](VoiceSelectionTask::Write){return (h.lastwrite&1)||0x174a7u!=(unsigned(h.lastwrite)|1u)?3u:0u;};
 const auto cost=task.nextClocks(timing(VoiceSelectionTask::Write::VoiceIndex));
 auto read=[&](VoiceSelectionTask::Read what)->uint32_t{using R=VoiceSelectionTask::Read;const unsigned part=0x174ae + 20*task.controlPart(),slot=0x17560 + 20*task.slot();
  switch(what){case R::PatchMode:return uint8_t(h.read8(int32_t(call.patch)));case R::Held:return uint8_t(h.read8(part+17));case R::Unison:return uint8_t(h.read8(part+18));
  case R::Part:return uint8_t(h.read8(0x17526));case R::Count:return uint8_t(h.read8(0x174a6));case R::Age:return h.read32(slot+8);
  case R::OwnerPart:return uint8_t(h.read8(slot+4));case R::OwnerUnison:return uint8_t(h.read8(slot+12));case R::SlotNote:return uint8_t(h.read8(slot+5));
  case R::VoiceIndex:return uint8_t(h.read8(0x174a7));case R::IncomingNote:return uint8_t(h.read8(0x17528));}return 0;};
 auto write=[&](VoiceSelectionTask::Write what,uint8_t data){h.write8(int8_t(data),what==VoiceSelectionTask::Write::VoiceIndex?0x174a7:0x1752a);};
 const auto progress=task.advanceBefore(cost,read,write,timing);if(progress.clocks!=cost||progress.boundaries!=1)throw std::runtime_error("Native selection admission mismatch");
 auto dispatch=[&](unsigned target){clocks.call(target);h.pc=h.makepc(clocks.next);if(h.cycles-before!=cost)throw std::runtime_error("Native selection dispatch clock mismatch");};
 auto changeByte=[&](int8_t& output,uint8_t old,uint8_t next,int delta){const int prior=int8_t(old),result=prior+delta;output=int8_t(next);value(h,result);flag(h,2,(delta>0?(~uint32_t(prior)&uint32_t(result)):(uint32_t(prior)&~uint32_t(result)))&0x80);};
 switch(stage){
 case S::AdmitMode:case S::JumpHeldAllocation:case S::AdmitActiveAge:case S::JumpPopulation:case S::JumpRelease:h.clockI(2);break;
 case S::AdmitAgeHigh:if(task.search()==K::Free)h.clockI(2);break;
 case S::AdmitBestHigh:if(task.search()==K::Owner)h.clockI(2);break;
 case S::DispatchPart:dispatch(0xa66a);return true;case S::ReadMode:v[4].rl=int8_t(task.modeBits());value(h,v[4].rl);break;
 case S::ShiftModeTwo:case S::ShiftModeOne:flag(h,1,oldMode&(stage==S::ShiftModeTwo?2:1));v[4].rl=int8_t(task.modeBits());value(h,v[4].rl);break;
 case S::MaskMode:v[4].rl=int8_t(task.modeBits());value(h,v[4].rl);break;case S::ReadHeld:v[4].rh=int8_t(task.held());value(h,v[4].rh);break;
 case S::ReadUnison:v[5].rh=int8_t(task.unison());value(h,v[5].rh);break;case S::ReadPart:v[5].rl=int8_t(task.part());value(h,v[5].rl);break;
 case S::CaptureOwner:v[5].e=int16_t(task.ownerTag());value(h,v[5].e);break;case S::ResetCandidate:v[2].rh=h.sub8(v[2].rh,v[2].rh);break;
 case S::PublishInitialIndex:value(h,0);break;case S::ReadCount:v[2].rl=int8_t(task.remaining());value(h,v[2].rl);break;
 case S::InitializeAge:v[1].er=int32_t(task.bestAge());value(h,v[1].er);break;case S::CompareMode:h.sub8(int8_t(task.modeBits()),1);break;
 case S::TestHeld:value(h,int8_t(task.held()));break;
 case S::SetHeldAllocation:case S::SetReuseAllocation:case S::SetPolyAllocation:case S::SetNoteAllocation:case S::SetRetrigger:v[0].rl=int8_t(task.allocation());value(h,v[0].rl);break;
 case S::SetOwnerAllocation:case S::SetNewestThree:case S::SetNewestTwo:v[4].rl=int8_t(task.allocation());value(h,v[4].rl);break;
 case S::PublishReuseAllocation:case S::PublishFreeAllocation:case S::PublishNoteAllocation:case S::PublishRetrigger:case S::PublishOwnerAllocation:case S::PublishNewestAllocation:value(h,int8_t(task.allocation()));break;
 case S::CompareOtherMode:h.sub8(int8_t(task.modeBits()),2);break;case S::ReadIncoming:v[4].rl=int8_t(task.incoming());value(h,v[4].rl);break;
 case S::DispatchVoice:dispatch(0xa656);return true;case S::ReadAge:v[0].er=int32_t(task.age());value(h,v[0].er);break;
 case S::ReadOwner:v[5].rl=int8_t(task.slotPart());value(h,v[5].rl);break;case S::ReadSlotUnison:v[5].rh=int8_t(task.slotUnison());value(h,v[5].rh);break;
 case S::CompareOwner:h.sub16(int16_t(task.slotOwnerTag()),int16_t(task.ownerTag()));break;case S::ResetNewestAge:v[1].er=h.sub32(v[1].er,v[1].er);break;
 case S::TestAgeHigh:value(h,int16_t(task.age()>>16));break;case S::CaptureHighAge:v[5].r=int16_t(task.age()>>16);value(h,v[5].r);break;
 case S::MaskActiveAge:v[5].rh=int8_t((task.age()>>24)&0xc0);value(h,v[5].rh);break;case S::ReadNote:v[5].rl=int8_t(task.note());value(h,v[5].rl);break;
 case S::CompareNote:h.sub8(int8_t(task.incoming()),int8_t(task.note()));break;case S::CompareAge:h.sub32(int32_t(task.age()),int32_t(task.bestAge()));break;
 case S::CopyAge:v[1].er=int32_t(task.bestAge());value(h,v[1].er);break;case S::ReadCandidate:v[2].rh=int8_t(task.candidate());value(h,v[2].rh);break;
 case S::ReadIndex:v[0].rl=int8_t(task.index());value(h,v[0].rl);break;case S::IndexTick:changeByte(v[0].rl,oldIndex,task.index(),1);break;
 case S::PublishIndex:value(h,int8_t(task.index()));break;case S::RemainingTick:changeByte(v[2].rl,oldRemaining,task.remaining(),-1);break;
 case S::PublishCandidate:value(h,int8_t(task.candidate()));break;case S::TestBestHigh:value(h,int16_t(task.bestAge()>>16));break;
 case S::CompareHighAge:h.sub16(int16_t(task.bestAge()>>16),0x100);break;
 case S::DispatchSelect:dispatch(0x77f4);return true;case S::ReleaseWord:v[1].e=0x2000;value(h,v[1].e);break;
 case S::ReleaseAddressOne:v[4].r=int16_t(task.firstReleaseOffset());value(h,v[4].r);break;case S::ReleaseAddressTwo:v[4].r=int16_t(task.secondReleaseOffset());value(h,v[4].r);break;
 case S::DispatchWriteOne:case S::DispatchWriteTwo:dispatch(0x785a);return true;case S::DispatchGate:dispatch(0x8fc0);return true;default:break;
 }
 if(h.cycles-before!=cost)throw std::runtime_error("Native selection bus clock mismatch");h.pc=h.makepc(point(task).location);
 if(task.complete()){selected->reset();++completed;}return true;
}
}
