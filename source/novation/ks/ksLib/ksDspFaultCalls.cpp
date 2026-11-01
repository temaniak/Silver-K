#include "ksDspFaultCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
DspFaultPartialTest partialTest=nullptr;
struct Point {unsigned location,length,depth;};
Point point(const DspFaultTask& task){
    using S=DspFaultTask::Stage;
    switch(task.stage()){
    case S::Admission:return {0x2304,4,0};
    case S::LatchAdmission:return {0x660,2,4};
    case S::ReadLatch:return {0x662,6,6};
    case S::Toggle:return {0x668,2,6};
    case S::PublishLatch:return {0x66a,6,6};
    case S::WriteData:return {0x670,2,6};
    case S::ControlSetup:return {0x672,2,6};
    case S::WriteControl:return {0x674,2,6};
    case S::BeginPulse:return {0x676,4,6};
    case S::DelayAdmission:return {0x67a,2,6};
    case S::DelaySetup:return {0x67c,4,8};
    case S::DelayTick:return {0x680,2,8};
    case S::DelayGate:return {0x682,2,8};
    case S::DelayRelease:return {0x684,2,8};
    case S::EndPulse:return {0x686,4,6};
    case S::LatchRelease:return {0x68a,2,6};
    case S::CompletionDelay:return {0x68c,2,4};
    case S::Complete:return {0x2308,2,0};
    }
    throw std::runtime_error("Invalid native KS fault task stage");
}
}
void setDspFaultPartialForTests(DspFaultPartialTest fn){partialTest=fn;}
bool DspFaultCalls::step(ControllerCpu& h){
    using namespace controllerDetail;using S=DspFaultTask::Stage;
    const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
    for(auto& c:calls)if(c && c->callerToken-point(c->task).depth==h.getSP()){selected=&c;break;}
    if(pc==0x2304){
        if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
        if(!selected)throw std::runtime_error("Native KS fault-call capacity exceeded");
        selected->emplace(h.getSP());++started;
    }
    if(!selected || point((*selected)->task).location!=pc){
        if(selected)selected->reset();
        if(partialTest && partialTest(h)){++partialEntries;return true;}
        return false;
    }
    auto& task=(*selected)->task;const auto stage=task.stage();const auto p=point(task);Boundary clocks(h,pc,p.length);auto& r=h.regs;
    if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
    const unsigned cost=task.nextBoundaryClocks();
    const auto progress=task.advanceBefore(cost,[&](DspFaultTask::Read input){return h.read8(input==DspFaultTask::Read::Latch?0xffec22:0xffff62);},
        [&](DspFaultTask::Write output,uint8_t value){h.write8(int8_t(value),output==DspFaultTask::Write::Latch?0xffec22:output==DspFaultTask::Write::Data?0xffff61:0xffff62);});
    if(progress.clocks!=cost || progress.boundaries!=1)throw std::runtime_error("Native KS fault-call clock mismatch");
    switch(stage){
    case S::Admission:clocks.call(0x660);break;
    case S::LatchAdmission:clocks.push(r[6].r);break;
    case S::ReadLatch:case S::Toggle:r[6].rl=int8_t(task.latch());value(h,r[6].rl);break;
    case S::PublishLatch:case S::WriteData:value(h,int8_t(task.latch()));break;
    case S::ControlSetup:r[6].rl=0x36;value(h,r[6].rl);break;
    case S::WriteControl:value(h,0x36);break;
    case S::DelayAdmission:clocks.push(r[0].r);break;
    case S::DelaySetup:r[0].r=3;value(h,3);break;
    case S::DelayTick:r[0].r=int16_t(task.remainingTicks());value(h,r[0].r);flag(h,2,false);break;
    case S::DelayRelease:clocks.pop(r[0].r);break;
    case S::LatchRelease:clocks.pop(r[6].r);break;
    case S::CompletionDelay:clocks.ret();break;
    default:break;
    }
    h.pc=h.makepc(point(task).location);return true;
}
}
