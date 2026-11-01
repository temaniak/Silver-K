#include "ksDspAddressCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
DspAddressPartialTest partialTest=nullptr;
struct Point {unsigned location,length;};
Point point(const DspAddressTask& task,unsigned base){
    using S=DspAddressTask::Stage;
    const unsigned phase=base+(task.requestSent()?0x48:0);
    switch(task.stage()){
    case S::SelectPort:return {task.portSelection()==DspAddressTask::PortSelection::B?0x2560u:0x2568u,6};
    case S::SelectDelay:return {0x2566,2};
    case S::ReadStatus:return {phase+(task.retry()?0x0eu:0u),8};
    case S::MaskStatus:return {phase+(task.retry()?0x16u:8u),2};
    case S::AdmitStatus:return {phase+(task.retry()?0x18u:0x0au),2};
    case S::ResetRetries:return {phase+0x0c,2};
    case S::DecrementRetries:return {phase+0x1a,2};
    case S::AdmitRetry:return {phase+0x1c,2};
    case S::DispatchFault:return {phase+0x1e,4};
    case S::AwaitFault:case S::ResumeFault:return {phase+0x22,2};
    case S::AssertHF3:return {base+0x24,2};
    case S::WriteHF3:return {base+0x26,8};
    case S::Command:return {base+0x2e,2};
    case S::WriteCommand:return {base+0x30,8};
    case S::WriteAddressHigh:return {base+0x38,8};
    case S::WriteAddressLow:return {base+0x40,8};
    case S::ClearHF3:return {base+0x6c,2};
    case S::ReleaseHF3:return {base+0x6e,8};
    case S::ReadHigh:return {base+0x76,8};
    case S::ReadMiddle:return {base+0x7e,8};
    case S::ReadLow:return {base+0x86,8};
    case S::Complete:return {base+(task.receives()?0x8eu:0x76u),2};
    }
    throw std::runtime_error("Invalid native KS address task stage");
}
}
void setDspAddressPartialForTests(DspAddressPartialTest fn){partialTest=fn;}
bool DspAddressCalls::step(ControllerCpu& h){
    using namespace controllerDetail;using S=DspAddressTask::Stage;
    const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
    for(auto& c:calls)if(c && c->callerToken==h.getSP()){selected=&c;break;}
    const bool select=pc==0x2560 || pc==0x2568;
    const bool enter=select || pc==0x2416 || (pc==0x256e && !selected);
    if(enter && (select || h.regs[5].er==0x400000 || h.regs[5].er==0x600000)){
        if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
        if(!selected)throw std::runtime_error("Native KS DSP-address capacity exceeded");
        const unsigned port=select?(pc==0x2560?0x600000:0x400000):unsigned(h.regs[5].er);
        selected->emplace(uint16_t(h.regs[4].r),pc,port,h.getSP());++started;
    }
    if(!selected || point((*selected)->task,(*selected)->base).location!=pc){
        if(selected)selected->reset();
        if(partialTest && partialTest(h)){++partialEntries;return true;}
        return false;
    }
    auto& c=**selected;auto& task=c.task;
    if(task.stage()==S::AwaitFault)task.resumeFault();
    const auto stage=task.stage();const auto p=point(task,c.base);Boundary clocks(h,pc,p.length);
    if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
    const int16_t oldRetries=int16_t(task.retries());const unsigned cost=task.nextBoundaryClocks();
    const auto progress=task.advanceBefore(cost,[&](unsigned offset){return h.read8(c.dsp+offset);},
        [&](unsigned offset,uint8_t byte){h.write8(int8_t(byte),c.dsp+offset);value(h,int8_t(byte));});
    if(progress.clocks!=cost || progress.boundaries!=1)throw std::runtime_error("Native KS DSP-address clock mismatch");
    auto& r=h.regs;
    switch(stage){
    case S::SelectPort:r[5].er=int32_t(c.dsp);value(h,r[5].er);break;
    case S::ReadStatus:r[0].rl=int8_t(task.status());value(h,r[0].rl);break;
    case S::MaskStatus:r[0].rl=int8_t(task.status()&task.readyMask());value(h,r[0].rl);break;
    case S::ResetRetries:r[0].e=h.sub16(r[0].e,r[0].e);break;
    case S::DecrementRetries:{
        const int result=int(oldRetries)-1;r[0].e=int16_t(task.retries());value(h,result);
        flag(h,2,(uint32_t(oldRetries)&~uint32_t(result))&0x8000);break;
    }
    case S::DispatchFault:clocks.call(0x2304);h.pc=h.makepc(clocks.next);return true;
    case S::AssertHF3:r[0].rl=8;value(h,8);break;
    case S::Command:r[0].rl=int8_t(task.command());value(h,r[0].rl);break;
    case S::ClearHF3:r[0].rl=0;value(h,0);break;
    case S::ReadHigh:r[4].rh=int8_t(task.data());value(h,r[4].rh);break;
    case S::ReadMiddle:r[4].rl=int8_t(task.data());value(h,r[4].rl);break;
    case S::ReadLow:r[0].rl=int8_t(task.data());value(h,r[0].rl);break;
    default:break;
    }
    h.pc=h.makepc(point(task,c.base).location);return true;
}
}
