#include "ksDspBootStreamCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
#include <utility>
namespace ks {
namespace {
DspBootStreamPartialTest partialTest=nullptr;
struct Point {unsigned location,length;};
Point point(const DspBootStreamTask& task){
    using S=DspBootStreamTask::Stage;
    switch(task.stage()){
    case S::ReadStatus:return {task.retry()?0x23dcu:0x23ceu,8};
    case S::MaskStatus:return {task.retry()?0x23e4u:0x23d6u,2};
    case S::AdmitStatus:return {task.retry()?0x23e6u:0x23d8u,2};
    case S::ResetRetries:return {0x23da,2};
    case S::DecrementRetries:return {0x23e8,2};
    case S::AdmitRetry:return {0x23ea,2};
    case S::DispatchFault:return {0x23ec,4};
    case S::AwaitFault:case S::ResumeFault:return {0x23f0,2};
    case S::ReadHigh:return {0x23f2,2};
    case S::WriteHigh:return {0x23f4,8};
    case S::ReadMiddle:return {0x23fc,2};
    case S::WriteMiddle:return {0x23fe,8};
    case S::ReadLow:return {0x2406,2};
    case S::WriteLow:return {0x2408,8};
    case S::Count:return {0x2410,2};
    case S::Continue:return {0x2412,2};
    case S::Complete:return {0x2414,2};
    }
    throw std::runtime_error("Invalid native KS boot-stream stage");
}
}
bool DspBootStreamCalls::sourceSpanValid(uint32_t source,uint16_t words){
    const uint32_t begin=source&0xffffff;
    const uint64_t size=3ull*(words?uint32_t(words):65536u);
    // Board-mapped ROM/Flash/SRAM spans whose data accesses cost three clocks.
    // Never admit control IO or an unmapped/crossing range as firmware bytes.
    if(begin<0x200000)return size<=0x200000-begin;
    for(auto range:{std::pair<uint32_t,uint32_t>{0x800000,0x880000},{0xa00000,0xc00000},{0xffdc00,0xfffc00}})
        if(begin>=range.first && begin<range.second)return size<=range.second-begin;
    return false;
}
void setDspBootStreamPartialForTests(DspBootStreamPartialTest fn){partialTest=fn;}
bool DspBootStreamCalls::step(ControllerCpu& h){
    using namespace controllerDetail;using S=DspBootStreamTask::Stage;
    const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
    for(auto& c:calls)if(c && c->callerToken==h.getSP()){selected=&c;break;}
    // The entry is also the next-word continuation: never restart a live job.
    if(pc==0x23ce && !selected && (h.regs[5].er==0x400000 || h.regs[5].er==0x600000)
        && sourceSpanValid(uint32_t(h.regs[4].er),uint16_t(h.regs[3].e))){
        for(auto& c:calls)if(!c){selected=&c;break;}
        if(!selected)throw std::runtime_error("Native KS boot-stream capacity exceeded");
        selected->emplace(uint16_t(h.regs[3].e),unsigned(h.regs[5].er),uint32_t(h.regs[4].er),h.getSP());++started;
    }
    if(!selected || point((*selected)->task).location!=pc){
        if(selected)selected->reset();
        if(partialTest && partialTest(h)){++partialEntries;return true;}
        return false;
    }
    auto& c=**selected;auto& task=c.task;
    if(task.stage()==S::AwaitFault)task.resumeFault();
    const auto stage=task.stage();const auto p=point(task);Boundary clocks(h,pc,p.length);auto& r=h.regs;
    if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
    const int16_t oldRetries=int16_t(task.retries()),oldWords=int16_t(uint16_t(task.remainingWords()));
    const unsigned cost=task.nextBoundaryClocks();
    const auto progress=task.advanceBefore(cost,[&]{return h.read8(c.dsp+4);},
        [&](uint32_t offset){return h.read8(int32_t(c.source+offset));},
        [&](unsigned offset,uint8_t byte){h.write8(int8_t(byte),c.dsp+offset);value(h,int8_t(byte));});
    if(progress.clocks!=cost || progress.boundaries!=1)throw std::runtime_error("Native KS boot-stream clock mismatch");
    switch(stage){
    case S::ReadStatus:r[0].rl=int8_t(task.status());value(h,r[0].rl);break;
    case S::MaskStatus:r[0].rl=int8_t(task.status()&2);value(h,r[0].rl);break;
    case S::ResetRetries:r[0].e=h.sub16(r[0].e,r[0].e);break;
    case S::DecrementRetries:{
        const int result=int(oldRetries)-1;r[0].e=int16_t(task.retries());value(h,result);
        flag(h,2,(uint32_t(oldRetries)&~uint32_t(result))&0x8000);break;
    }
    case S::DispatchFault:clocks.call(0x2304);h.pc=h.makepc(clocks.next);return true;
    case S::ReadHigh:case S::ReadMiddle:case S::ReadLow:
        r[0].rl=int8_t(task.byte());value(h,r[0].rl);r[4].er=int32_t(c.source+task.bytesConsumed());h.clockI(2);break;
    case S::Count:{
        const int result=int(oldWords)-1;r[3].e=int16_t(uint16_t(task.remainingWords()));value(h,result);
        flag(h,2,(uint32_t(oldWords)&~uint32_t(result))&0x8000);break;
    }
    default:break;
    }
    h.pc=h.makepc(point(task).location);return true;
}
}
