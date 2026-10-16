#include "ksDspWordCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
DspWordPartialTest partialTest=nullptr;
struct Point {unsigned offset,length;};
Point point(const DspWordTask& task){
    using S=DspWordTask::Stage;
    switch(task.stage()){
    case S::ReadStatus:return {task.retry()?0x0eu:0u,8};
    case S::MaskStatus:return {task.retry()?0x16u:8u,2};
    case S::AdmitStatus:return {task.retry()?0x18u:0x0au,2};
    case S::ResetRetries:return {0x0c,2};
    case S::DecrementRetries:return {0x1a,2};
    case S::AdmitRetry:return {0x1c,2};
    case S::DispatchFault:return {0x1e,4};
    case S::AwaitFault:case S::ResumeFault:return {0x22,2};
    case S::Prepare:return {0x24,2};
    case S::High:return {0x26,8};
    case S::Middle:return {0x2e,8};
    case S::Low:return {0x36,8};
    case S::AdvanceAddress:return {0x3e,2};
    case S::PublishAddress:return {0x40,6};
    case S::Complete:return {task.advancesAddress()?0x46u:0x3eu,2};
    }
    throw std::runtime_error("Invalid native KS word task stage");
}
}
void setDspWordPartialForTests(DspWordPartialTest fn){partialTest=fn;}
bool DspWordCalls::step(ControllerCpu& h){
    using namespace controllerDetail;using S=DspWordTask::Stage;
    const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
    for(auto& c:calls)if(c && c->callerToken==h.getSP()){selected=&c;break;}
    if((pc==0x24d8 || pc==0x2520) && (h.regs[5].er==0x400000 || h.regs[5].er==0x600000)){
        if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
        if(!selected)throw std::runtime_error("Native KS DSP-word capacity exceeded");
        selected->emplace(uint32_t(h.regs[1].er)>>8,pc,unsigned(h.regs[5].er),uint16_t(h.regs[4].r),h.getSP());++started;
    }
    if(!selected || (*selected)->base+point((*selected)->task).offset!=pc){
        if(selected)selected->reset();
        if(partialTest && partialTest(h)){++partialEntries;return true;}
        return false;
    }
    auto& c=**selected;auto& task=c.task;
    if(task.stage()==S::AwaitFault)task.resumeFault();
    const auto stage=task.stage();const auto p=point(task);Boundary clocks(h,pc,p.length);
    if(task.complete()){
        clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;
    }
    const int16_t oldRetries=int16_t(task.retries());
    const unsigned cost=task.nextBoundaryClocks();
    const auto advance=task.advanceBefore(cost,[&]{return h.read8(c.dsp+4);},
        [&](unsigned offset,uint8_t byte){h.write8(int8_t(byte),c.dsp+offset);value(h,int8_t(byte));},
        [&](uint16_t address){h.write16(int16_t(address),0x159f2);value(h,int16_t(address));});
    if(advance.clocks!=cost || advance.boundaries!=1)throw std::runtime_error("Native KS DSP-word clock mismatch");
    auto& r=h.regs;
    switch(stage){
    case S::ReadStatus:r[0].rl=int8_t(task.status());value(h,r[0].rl);break;
    case S::MaskStatus:r[0].rl=int8_t(task.status()&2);value(h,r[0].rl);break;
    case S::ResetRetries:r[0].e=h.sub16(r[0].e,r[0].e);break;
    case S::DecrementRetries:{
        const int result=int(oldRetries)-1;r[0].e=int16_t(task.retries());value(h,result);
        flag(h,2,(uint32_t(oldRetries)&~uint32_t(result))&0x8000);break;
    }
    case S::DispatchFault:clocks.call(0x2304);h.pc=h.makepc(clocks.next);return true;
    case S::Prepare:r[0].r=int16_t(task.word()>>8);value(h,r[0].r);break;
    case S::AdvanceAddress:{
        const int old=int16_t(task.originalAddress()),result=old+1;
        r[4].r=int16_t(task.nextAddress());value(h,result);
        flag(h,2,(~uint32_t(old)&uint32_t(result))&0x8000);break;
    }
    default:break;
    }
    h.pc=h.makepc(c.base+point(task).offset);return true;
}
}
