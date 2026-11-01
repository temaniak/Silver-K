#include "ksPartNoteCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
PartNotePartialTest partialTest=nullptr;using T=PartNoteTask;using S=T::Stage;using K=T::Kind;
struct Point{unsigned location,length;};
Point point(const T& t){const bool on=t.kind()==K::On,off=t.releasing();switch(t.stage()){
 case S::OnReadLocal:return {0x3426,6};case S::OnAdmitLocal:return {0x342c,2};case S::OnReadPart:return {0x342e,6};case S::OnReadActive:return {0x3434,6};case S::OnComparePart:return {0x343a,2};case S::OnAdmitPart:return {0x343c,2};case S::OnSkipPreview:return {0x343e,2};case S::OnReadPreviewGate:return {0x3440,6};case S::OnAdmitPreview:return {0x3446,2};case S::OnReadPreviewNote:return {0x3448,2};case S::OnRetainEvent:return {0x344a,4};case S::OnDispatchPreview:return {0x344e,4};case S::OnAwaitPreview:case S::OnReleaseEvent:return {0x3452,4};
 case S::ReadNote:return {on?0x3456u:0x33b8u,2};case S::DispatchControls:return {on?0x3458u:0x33bau,4};case S::AwaitControls:case S::ReadLow:return {on?0x345cu:0x33beu,8};case S::ReadHigh:return {on?0x3464u:0x33c6u,8};case S::CompareLow:return {on?0x346cu:0x33ceu,2};case S::AdmitLow:return {on?0x346eu:0x33d0u,2};case S::CompareHigh:return {on?0x3470u:0x33d2u,2};case S::AdmitHigh:return {on?0x3472u:0x33d4u,2};
 case S::OnReadVelocity:return {0x3474,4};case S::OnAdmitVelocity:return {0x3478,4};case S::OnDispatchState:return {0x347c,4};case S::OnAwaitState:case S::OnDispatchPart:return {0x3480,4};case S::OnAwaitPart:return {0x3484,8};
 case S::OffDispatchState:return {0x33d6,4};case S::OffAwaitState:case S::OffDispatchPart:return {0x33da,4};case S::OffAwaitPart:return {0x33de,8};
 case S::ReadPolicy:return {off?0x33deu:0x3484u,8};case S::ObserveArp:return {off?0x33e6u:0x348cu,2};case S::AdmitArp:return {off?0x33e8u:0x348eu,2};case S::MaskArp:return {off?0x33eau:0x3490u,2};case S::CompareArp:return {off?0x33ecu:0x3492u,2};case S::AdmitArpMode:return {off?0x33eeu:0x3494u,2};
 case S::OffRetainNote:return {0x33f0,2};case S::OffDispatchPriorityPart:return {0x33f2,4};case S::OffAwaitPriorityPart:case S::OffReadCount:return {0x33f6,8};case S::OffCompareCount:return {0x33fe,2};case S::OffAdmitCount:return {0x3400,2};case S::OffReadLast:return {0x3402,8};case S::OffCompareLast:return {0x340a,2};case S::OffAdmitLast:return {0x340c,2};case S::OffDispatchReplacement:return {0x340e,4};case S::OffAwaitReplacement:case S::OffObserveReplacement:return {0x3412,2};case S::OffAdmitReplacement:return {0x3414,2};case S::OffDispatchStart:return {0x3416,4};case S::OffAwaitStart:case S::OffReleaseReturn:return {0x341a,2};case S::OffFinishReplacement:return {0x341c,2};
 case S::OffReleaseOriginal:return {0x341e,2};case S::OffDispatchRelease:return {0x3420,4};case S::OffAwaitRelease:case S::OffFinish:return {0x3424,2};case S::OnDispatchStart:return {0x3496,4};case S::OnAwaitStart:case S::OnFinish:return {0x349a,2};default:throw std::runtime_error("Invalid native part note stage");
}}
uint8_t bind(uint32_t address,unsigned base,unsigned stride){if(address<base||address>=base+256*stride||(address-base)%stride)throw std::runtime_error("Invalid native part-note binding");return uint8_t((address-base)/stride);}
}
void setPartNotePartialForTests(PartNotePartialTest fn){partialTest=fn;}
bool PartNoteCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;
 std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 if(pc==0x33b8||pc==0x3426){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native part-note capacity exceeded");selected->emplace(pc==0x3426?K::On:K::Off,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;if(task.awaitingControls())task.resumeControls(bind(uint32_t(v[0].er),0x174ae,20));if(task.awaitingPart())task.resumePart(bind(uint32_t(v[0].er),0x160d2,334));if(task.awaitingChild())task.resumeChild();
 const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const auto cost=task.nextClocks();
 const unsigned params=0x160d2 + unsigned(task.part())*334,controls=0x174ae + unsigned(task.controlPart())*20;
 auto read=[&](T::Read field)->unsigned{switch(field){
 case T::Read::Local:return uint8_t(h.read8(0xffec14));case T::Read::Part:return uint8_t(h.read8(0x17526));case T::Read::ActivePart:return uint8_t(h.read8(0x16798));case T::Read::PreviewGate:return uint8_t(h.read8(0x15c15));
 case T::Read::EventNote:return uint8_t(h.read8(uint32_t(v[4].er)));case T::Read::EventVelocity:return uint8_t(h.read8(uint32_t(v[4].er)+1));
 case T::Read::MessageWord:return uint16_t(v[5].r);case T::Read::Low:return uint8_t(h.read8(controls+12));case T::Read::High:return uint8_t(h.read8(controls+13));
 case T::Read::Policy:return uint8_t(h.read8(params+327));case T::Read::Count:return uint8_t(h.read8(params+313));case T::Read::Last:return uint8_t(h.read8(params+314));default:return uint8_t(v[5].rl);
 }};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native part note deadline mismatch");
 switch(stage){
 case S::OnReadLocal:v[0].rl=int8_t(task.local());value(h,v[0].rl);break;case S::OnReadPart:v[0].rl=int8_t(task.selectedPart());value(h,v[0].rl);break;case S::OnReadActive:v[0].rh=int8_t(task.activePart());value(h,v[0].rh);break;case S::OnComparePart:h.sub8(v[0].rh,v[0].rl);break;
 case S::OnReadPreviewGate:v[0].rl=int8_t(task.previewGate());value(h,v[0].rl);break;case S::OnReadPreviewNote:v[6].rl=int8_t(task.previewNote());value(h,v[6].rl);break;
 case S::OnRetainEvent:clocks.push(v[4].er);call.depth+=4;break;case S::OnReleaseEvent:clocks.pop(v[4].er);call.depth-=4;break;
 case S::OnDispatchPreview:case S::DispatchControls:case S::OnDispatchState:case S::OnDispatchPart:case S::OffDispatchState:case S::OffDispatchPart:case S::OffDispatchPriorityPart:case S::OffDispatchReplacement:case S::OffDispatchStart:case S::OffDispatchRelease:case S::OnDispatchStart:{
  const unsigned target=stage==S::OnDispatchPreview?0x4d90:stage==S::DispatchControls?0xa66a:stage==S::OnDispatchState?0x2efc:stage==S::OffDispatchState?0x2f92:stage==S::OffDispatchReplacement?0x3118:stage==S::OffDispatchStart||stage==S::OnDispatchStart?0x3642:stage==S::OffDispatchRelease?0x92c0:0x4000;
  clocks.call(target);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native part note dispatch clocks differ");return true;}
 case S::ReadNote:v[5].rl=int8_t(task.note());value(h,v[5].rl);break;case S::ReadLow:v[1].rl=int8_t(task.low());value(h,v[1].rl);break;case S::ReadHigh:v[1].rh=int8_t(task.high());value(h,v[1].rh);break;case S::CompareLow:h.sub8(v[5].rl,v[1].rl);break;case S::CompareHigh:h.sub8(v[5].rl,v[1].rh);break;
 case S::OnReadVelocity:v[5].rh=int8_t(task.velocity());value(h,v[5].rh);break;case S::OnAdmitVelocity:h.clockI(2);break;
 case S::ReadPolicy:v[1].rl=int8_t(task.policy());value(h,v[1].rl);break;case S::ObserveArp:flag(h,4,!(task.policy()&4));break;case S::MaskArp:v[1].rl=int8_t(task.maskedPolicy());value(h,v[1].rl);break;case S::CompareArp:h.sub8(v[1].rl,0x60);break;
 case S::OffRetainNote:clocks.push(v[5].r);call.depth+=2;break;case S::OffReadCount:v[1].rl=int8_t(task.count());value(h,v[1].rl);break;case S::OffCompareCount:h.sub8(v[1].rl,1);break;case S::OffReadLast:v[0].rl=int8_t(task.last());value(h,v[0].rl);break;case S::OffCompareLast:h.sub8(v[5].rl,v[0].rl);break;
 case S::OffObserveReplacement:value(h,v[5].rl);break;case S::OffReleaseReturn:case S::OffReleaseOriginal:clocks.pop(v[5].r);call.depth-=2;break;
 case S::OffFinish:case S::OffFinishReplacement:case S::OnFinish:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native part note return clocks differ");selected->reset();++completed;return true;default:break;
 }
 if(h.cycles-begin!=cost)throw std::runtime_error("Native part note bus clocks differ");h.pc=h.makepc(point(task).location);return true;
}
}
