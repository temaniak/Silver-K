#pragma once
#include "kstationControllerForegroundRuntime.h"
#include <cstdio>
#include <cstdlib>
namespace request_equivalence_test {
using Runtime=kstation::ControllerForegroundRuntime;
inline bool equal(const Runtime::Request&a,const Runtime::Request&b){return
  a.kind==b.kind&&
  a.child==b.child&&
  a.slot==b.slot&&
  a.callback==b.callback&&
  a.pumpChild==b.pumpChild&&
  a.byte==b.byte&&
  a.handler==b.handler&&
  a.channel==b.channel&&
  a.payload==b.payload&&
  a.immediate==b.immediate&&
  a.status==b.status&&
  a.internal==b.internal&&
  a.noteChild==b.noteChild&&
  a.note==b.note&&
  a.velocity==b.velocity&&
  a.noteService==b.noteService&&
  a.triggerValue==b.triggerValue&&
  a.patch==b.patch&&
  a.partAddress==b.partAddress&&
  a.tailBuild==b.tailBuild&&
  a.buildChild==b.buildChild&&
  a.command==b.command&&
  a.word==b.word&&
  a.allocationSelected==b.allocationSelected&&
  a.voiceSlot==b.voiceSlot&&
  a.allocation==b.allocation&&
  a.activationChild==b.activationChild&&
  a.activationVoice==b.activationVoice&&
  a.activationDspAddress==b.activationDspAddress&&
  a.buildDsp==b.buildDsp&&
  a.buildDspAddress==b.buildDspAddress&&
  a.releaseChild==b.releaseChild&&
  a.releaseVoice==b.releaseVoice&&
  a.releaseDspWrite==b.releaseDspWrite&&
  a.releaseDspAddress==b.releaseDspAddress&&
  a.expressionService==b.expressionService&&
  a.expressionValue==b.expressionValue&&
  a.expressionPartAddress==b.expressionPartAddress&&
  a.parameterDsp==b.parameterDsp&&
  a.parameterContinuation==b.parameterContinuation&&
  a.parameterWord==b.parameterWord&&
  a.parameterGroup==b.parameterGroup&&
  a.parameterDspKind==b.parameterDspKind&&
  a.parameterDspAddress==b.parameterDspAddress&&
  a.longContinuation==b.longContinuation&&
  a.refresh.child==b.refresh.child&&
  a.refresh.part==b.refresh.part&&
  a.refresh.voice==b.refresh.voice&&
  a.refresh.patch==b.refresh.patch;}
struct Counts{unsigned long long yes=0,no=0;unsigned long long kinds[9]{};~Counts(){std::fprintf(stderr,"request-equivalence: pending=%llu none=%llu kinds=",yes,no);for(auto n:kinds)std::fprintf(stderr," %llu",n);std::fputc('\n',stderr);}};
inline Counts counts;
inline void check(const Runtime&runtime,const Runtime::Request&old){
 const auto again=runtime.request();const auto kind=static_cast<unsigned>(old.kind);
 if(kind>=9||runtime.hasPendingRequest()!=(old.kind!=Runtime::RequestKind::None)||!equal(old,again)||(old.kind==Runtime::RequestKind::None&&!equal(old,Runtime::Request{}))){std::fprintf(stderr,"request equivalence failed kind=%u\n",kind);std::abort();}
 ++counts.kinds[kind];if(old.kind!=Runtime::RequestKind::None)++counts.yes;else ++counts.no;
}
}
