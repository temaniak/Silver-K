#include "ksKeyPriorityCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
KeyPriorityPartialTest partialTest=nullptr;
struct Point {unsigned location,length;};
Point point(const KeyCompactTask& task){using S=KeyCompactTask::Stage;
 switch(task.stage()){
 case S::RetainScratch:return {0x3024,2};case S::DispatchPart:return {0x3026,4};case S::AwaitPart:case S::ReadCount:return {0x302a,8};
 case S::AdmitCount:return {0x3032,2};case S::BindList:return {0x3034,2};case S::LocateList:return {0x3036,6};
 case S::ReadKey:return {0x303c,2};case S::CompareKey:return {0x303e,2};case S::AdmitMatch:return {0x3040,2};
 case S::SearchDecrement:return {0x3042,2};case S::AdmitSearchMore:return {0x3044,2};case S::NoMatch:return {0x3046,2};
 case S::MatchDecrement:return {0x3048,2};case S::AdmitShift:return {0x304a,2};case S::ReadShift:return {0x304c,2};case S::WriteShift:return {0x304e,4};
 case S::ShiftDecrement:return {0x3052,2};case S::AdmitShiftMore:return {0x3054,2};case S::ReloadCount:return {0x3056,8};case S::DecrementCount:return {0x305e,2};
 case S::PublishOrdered:return {0x3060,8};case S::PublishPhysical:return {0x3068,8};case S::ReleaseScratch:return {0x3070,2};case S::Finish:return {0x3072,2};
 default:throw std::runtime_error("Invalid native key compaction stage");
 }
}
Point point(const MonoPriorityTask& task){using S=MonoPriorityTask::Stage;
 switch(task.stage()){
 case S::RetainPart:return {0x3118,4};case S::RetainIndex:return {0x311c,4};case S::RetainMap:return {0x3120,4};case S::RetainDistance:return {0x3124,2};case S::RetainCandidate:return {0x3126,2};
 case S::DispatchPart:return {0x3128,4};case S::AwaitPart:case S::BindMap:return {0x312c,2};case S::LocateMap:return {0x312e,6};
 case S::DispatchUpper:return {0x3134,2};case S::AwaitUpper:case S::CaptureUpper:return {0x3136,2};case S::CopyUpperDistance:return {0x3138,2};case S::MeasureUpper:return {0x313a,2};
 case S::DispatchLower:return {0x313c,4};case S::AwaitLower:case S::CaptureVelocityKey:return {0x3140,2};case S::CopyLowerDistance:return {0x3142,2};case S::CaptureLower:return {0x3144,2};case S::MeasureLower:return {0x3146,2};
 case S::ObserveUpper:return {0x3148,2};case S::AdmitUpper:return {0x314a,2};case S::ObserveLower:return {0x314c,2};case S::AdmitLower:return {0x314e,2};case S::CompareDistances:return {0x3150,2};case S::AdmitDistance:return {0x3152,2};
 case S::SelectUpper:return {0x3154,2};case S::ObserveResult:return {0x3156,2};case S::AdmitResult:return {0x3158,2};case S::NormalizeKeyWord:return {0x315a,2};case S::NormalizeKey:return {0x315c,2};case S::LocateVelocity:return {0x315e,2};case S::ReadVelocity:return {0x3160,8};
 case S::ReleaseCandidate:return {0x3168,2};case S::ReleaseDistance:return {0x316a,2};case S::ReleaseMap:return {0x316c,4};case S::ReleaseIndex:return {0x3170,4};case S::ReleasePart:return {0x3174,4};case S::Finish:return {0x3178,2};
 default:throw std::runtime_error("Invalid native mono priority stage");
 }
}
unsigned bindPart(uint32_t address){if(address<0x160d2||address>=0x160d2 + 256*334||(address-0x160d2)%334)throw std::runtime_error("Invalid native key priority part binding");return (address-0x160d2)/334;}
}
void setKeyPriorityPartialForTests(KeyPriorityPartialTest fn){partialTest=fn;}
bool KeyPriorityCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());
 auto partial=[&]{if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;};
 auto& v=h.regs;
 if(pc>=0x3024&&pc<=0x3072){using T=KeyCompactTask;using S=T::Stage;
  std::optional<Call<T>>* selected=nullptr;for(auto& c:compacts)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
  if(pc==0x3024){if(!selected)for(auto& c:compacts)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native key compaction capacity exceeded");selected->emplace(h.getSP());++compactStarted;}
  if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
  auto& call=**selected;auto& task=call.task;if(task.awaitingPart())task.resumePart(uint8_t(bindPart(uint32_t(v[0].er))));
  const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const unsigned base=0x160d2 + unsigned(task.part())*334;
  auto timing=[&](unsigned offset){const unsigned address=base+offset;return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};
  const auto cost=task.nextClocks(task.publication()?timing(task.publicationOffset()):3);
  auto read=[&](T::Read field,unsigned index)->unsigned{switch(field){case T::Read::Count:return uint8_t(h.read8(base+307));case T::Read::Key:return uint16_t(h.read16(base+240+index*2));default:return uint8_t(v[5].rl);}};
  auto write=[&](T::Write field,unsigned index,unsigned data){if(field==T::Write::Shift)h.write16(int16_t(data),base+240+index*2);else h.write8(int8_t(data),base+index);};
  if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Native key compaction deadline mismatch");
  switch(stage){
  case S::RetainScratch:clocks.push(v[3].r);call.depth+=2;break;
  case S::DispatchPart:clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native key compaction dispatch clocks differ");return true;
  case S::ReadCount:case S::ReloadCount:v[2].rl=int8_t(task.remaining());value(h,v[2].rl);break;
  case S::BindList:v[1].er=int32_t(base);value(h,v[1].er);break;
  case S::LocateList:v[1].er=h.add32(v[1].er,240);break;
  case S::ReadKey:case S::ReadShift:v[3].r=int16_t(task.key());value(h,v[3].r);v[1].er=int32_t(base+240+task.position()*2);h.clockI(2);break;
  case S::CompareKey:h.sub8(v[3].rl,v[5].rl);break;
  case S::SearchDecrement:case S::MatchDecrement:case S::ShiftDecrement:case S::DecrementCount:clocks.inc(v[2].rl,-1);break;
  case S::WriteShift:value(h,int16_t(task.key()));break;
  case S::PublishOrdered:case S::PublishPhysical:value(h,int8_t(task.remaining()));break;
  case S::ReleaseScratch:clocks.pop(v[3].r);call.depth-=2;break;
  case S::Finish:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native key compaction return clocks differ");selected->reset();++compactCompleted;return true;
  default:break;
  }
  if(h.cycles-begin!=cost)throw std::runtime_error("Native key compaction bus clocks differ");h.pc=h.makepc(point(task).location);return true;
 }
 if(pc>=0x3118&&pc<=0x3178){using T=MonoPriorityTask;using S=T::Stage;
  std::optional<Call<T>>* selected=nullptr;for(auto& c:monos)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
  if(pc==0x3118){if(!selected)for(auto& c:monos)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native mono priority capacity exceeded");selected->emplace(h.getSP());++monoStarted;}
  if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
  auto& call=**selected;auto& task=call.task;if(task.awaitingPart())task.resumePart(uint8_t(bindPart(uint32_t(v[0].er))));if(task.awaitingUpper())task.resumeUpper();if(task.awaitingLower())task.resumeLower();
  const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const unsigned base=0x160d2 + unsigned(task.part())*334;const auto cost=task.nextClocks();
  auto read=[&](T::Read field,unsigned index)->unsigned{switch(field){case T::Read::Candidate:return uint8_t(v[4].rl);case T::Read::Velocity:return uint8_t(h.read8(base+64+index));default:return uint8_t(v[5].rl);}};
  if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native mono priority deadline mismatch");
  switch(stage){
  case S::RetainPart:clocks.push(v[0].er);call.depth+=4;break;case S::RetainIndex:clocks.push(v[1].er);call.depth+=4;break;
  case S::RetainMap:clocks.push(v[3].er);call.depth+=4;break;case S::RetainDistance:clocks.push(v[2].r);call.depth+=2;break;case S::RetainCandidate:clocks.push(v[4].r);call.depth+=2;break;
  case S::DispatchPart:case S::DispatchUpper:case S::DispatchLower:clocks.call(stage==S::DispatchPart?0x4000:stage==S::DispatchUpper?0x317a:0x31ac);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native mono priority dispatch clocks differ");return true;
  case S::BindMap:v[3].er=int32_t(base);value(h,v[3].er);break;case S::LocateMap:v[3].er=h.add32(v[3].er,192);break;
  case S::CaptureUpper:v[5].rh=int8_t(task.upper());value(h,v[5].rh);break;case S::CopyUpperDistance:v[2].rh=int8_t(task.upperDistance());value(h,v[2].rh);break;
  case S::MeasureUpper:v[2].rh=h.sub8(v[2].rh,v[5].rl);break;
  case S::CaptureVelocityKey:v[1].rl=int8_t(task.velocityKey());value(h,v[1].rl);break;case S::CopyLowerDistance:v[2].rl=int8_t(task.lowerDistance());value(h,v[2].rl);break;
  case S::CaptureLower:v[5].rl=int8_t(task.replacement());value(h,v[5].rl);break;case S::MeasureLower:v[2].rl=h.sub8(v[2].rl,v[5].rl);break;
  case S::ObserveUpper:value(h,v[5].rh);break;case S::ObserveLower:case S::ObserveResult:value(h,v[5].rl);break;
  case S::CompareDistances:h.sub8(v[2].rh,v[2].rl);break;case S::SelectUpper:v[5].rl=int8_t(task.replacement());value(h,v[5].rl);break;
  case S::NormalizeKeyWord:v[1].r&=255;value(h,v[1].r);break;case S::NormalizeKey:v[1].er&=65535;value(h,v[1].er);break;case S::LocateVelocity:v[1].er=h.add32(v[1].er,int32_t(base));break;
  case S::ReadVelocity:v[5].rh=int8_t(task.velocity());value(h,v[5].rh);break;
  case S::ReleaseCandidate:clocks.pop(v[4].r);call.depth-=2;break;case S::ReleaseDistance:clocks.pop(v[2].r);call.depth-=2;break;
  case S::ReleaseMap:clocks.pop(v[3].er);call.depth-=4;break;case S::ReleaseIndex:clocks.pop(v[1].er);call.depth-=4;break;case S::ReleasePart:clocks.pop(v[0].er);call.depth-=4;break;
  case S::Finish:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native mono priority return clocks differ");selected->reset();++monoCompleted;return true;
  default:break;
  }
  if(h.cycles-begin!=cost)throw std::runtime_error("Native mono priority bus clocks differ");h.pc=h.makepc(point(task).location);return true;
 }
 return false;
}
}
