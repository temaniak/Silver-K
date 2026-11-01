#include "ksDspBootSetupCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
#include <type_traits>
namespace ks {
namespace {
DspBootSetupPartialTest partialTest=nullptr;
struct Point {unsigned location,length,depth;};
Point point(const DspBootSetupTask& task){
    using S=DspBootSetupTask::Stage;
    switch(task.stage()){
    case S::HeaderHigh:return {0x2332,4,0};
    case S::HeaderLow:return {0x2336,4,0};
    case S::CaptureSize:return {0x233a,2,0};
    case S::StreamCount:return {0x233c,2,0};
    case S::DispatchStream:return {0x233e,4,0};
    case S::AwaitStream:case S::DelayAdmission:return {0x2342,2,0};
    case S::OuterSetup:return {0x2344,4,2};
    case S::OuterAdmission:return {0x2348,2,2};
    case S::InnerSetup:return {0x234a,4,4};
    case S::InnerTick:return {0x234e,2,4};
    case S::InnerGate:return {0x2350,2,4};
    case S::OuterRelease:return {0x2352,2,4};
    case S::OuterTick:return {0x2354,2,2};
    case S::OuterGate:return {0x2356,2,2};
    case S::DelayRelease:return {0x2358,2,2};
    case S::BootStatus:return {task.retry()?0x2368u:0x235au,8,0};
    case S::BootMask:return {task.retry()?0x2370u:0x2362u,2,0};
    case S::BootAdmit:return {task.retry()?0x2372u:0x2364u,2,0};
    case S::BootReset:return {0x2366,2,0};
    case S::BootDecrement:return {0x2374,2,0};
    case S::BootRetry:return {0x2376,2,0};
    case S::DispatchFault:return {0x2378,4,0};
    case S::AwaitFault:case S::BootResume:return {0x237c,2,0};
    case S::TableAddress:return {0x237e,4,0};
    case S::DispatchAddress:return {0x2382,4,0};
    case S::AwaitAddress:case S::ExponentialValue:return {0x2386,6,0};
    case S::ExponentialFactor:return {0x238c,4,0};
    case S::ExponentialCount:return {0x2390,4,0};
    case S::DispatchExponential:return {0x2394,4,0};
    case S::AwaitExponential:case S::ProductSeed:return {0x2398,2,0};
    case S::Product:return {0x239a,2,0};
    case S::ShiftSetup:return {0x239c,2,0};
    case S::Shift:return {0x239e,2,0};
    case S::ShiftTick:return {0x23a0,2,0};
    case S::ShiftGate:return {0x23a2,2,0};
    case S::AddDelta:return {0x23a4,2,0};
    case S::ExponentialTick:return {0x23a6,2,0};
    case S::ExponentialGate:return {0x23a8,2,0};
    case S::ReciprocalDivisor:return {0x23aa,4,0};
    case S::ReciprocalCount:return {0x23ae,4,0};
    case S::Numerator:return {0x23b2,6,0};
    case S::DivideHigh:return {0x23b8,2,0};
    case S::CopyHigh:return {0x23ba,2,0};
    case S::ClearDividend:return {0x23bc,2,0};
    case S::DivideLow:return {0x23be,2,0};
    case S::ComposeWord:return {0x23c0,2,0};
    case S::DispatchReciprocal:return {0x23c2,4,0};
    case S::AwaitReciprocal:case S::DivisorTick:return {0x23c6,2,0};
    case S::ReciprocalTick:return {0x23c8,2,0};
    case S::ReciprocalGate:return {0x23ca,2,0};
    case S::Complete:return {0x23cc,2,0};
    }
    throw std::runtime_error("Invalid native KS bootstrap setup stage");
}
}
void setDspBootSetupPartialForTests(DspBootSetupPartialTest fn){partialTest=fn;}
bool DspBootSetupCalls::step(ControllerCpu& h){
    using namespace controllerDetail;using S=DspBootSetupTask::Stage;
    const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
    for(auto& c:calls)if(c && c->callerToken-point(c->task).depth==h.getSP()){selected=&c;break;}
    if(pc==0x2332 && (h.regs[5].er==0x400000 || h.regs[5].er==0x600000)
        && DspBootStreamCalls::sourceSpanValid(uint32_t(h.regs[4].er),1)){
        if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
        if(!selected)throw std::runtime_error("Native KS bootstrap setup capacity exceeded");
        selected->emplace(unsigned(h.regs[5].er),uint32_t(h.regs[4].er),h.getSP());++started;
    }
    if(!selected || point((*selected)->task).location!=pc){
        if(selected)selected->reset();
        if(partialTest && partialTest(h)){++partialEntries;return true;}
        return false;
    }
    auto& c=**selected;auto& task=c.task;
    if(task.request()!=DspBootSetupTask::Request::None)task.resumeChild();
    const auto stage=task.stage();const auto p=point(task);Boundary clocks(h,pc,p.length);auto& r=h.regs;
    if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
    const auto oldValue=task.wordValue(),oldProduct=task.product();const auto oldDivision=task.division();const int16_t oldRetries=int16_t(task.retries());
    const unsigned cost=task.nextBoundaryClocks();
    const auto progress=task.advanceBefore(cost,[&](unsigned offset){return h.read8(int32_t(c.source+offset));},[&]{return h.read8(c.dsp+4);});
    if(progress.clocks!=cost || progress.boundaries!=1)throw std::runtime_error("Native KS bootstrap setup clock mismatch");
    auto projectSmallCount=[&](auto& output,unsigned count){output=typename std::remove_reference<decltype(output)>::type(count);value(h,int(count));flag(h,2,false);};
    auto dispatch=[&](unsigned target){clocks.call(target);h.pc=h.makepc(clocks.next);};
    switch(stage){
    case S::HeaderHigh:r[0].rh=int8_t(task.headerHigh());value(h,r[0].rh);break;
    case S::HeaderLow:r[0].rl=int8_t(task.headerSize());value(h,r[0].rl);break;
    case S::CaptureSize:r[3].e=int16_t(task.headerSize());value(h,r[3].e);break;
    case S::StreamCount:{
        const int old=int16_t(task.headerSize()),result=old+2;r[3].e=int16_t(task.streamWords());value(h,result);
        flag(h,2,(~uint32_t(old)&uint32_t(result))&0x8000);break;
    }
    case S::DispatchStream:dispatch(0x23ce);return true;
    case S::DelayAdmission:case S::OuterAdmission:clocks.push(r[0].r);break;
    case S::OuterSetup:r[0].r=int16_t(task.outerRemaining());value(h,r[0].r);break;
    case S::InnerSetup:r[0].r=int16_t(task.innerRemaining());value(h,r[0].r);break;
    case S::InnerTick:projectSmallCount(r[0].r,task.innerRemaining());break;
    case S::OuterRelease:case S::DelayRelease:clocks.pop(r[0].r);break;
    case S::OuterTick:projectSmallCount(r[0].r,task.outerRemaining());break;
    case S::BootStatus:r[0].rl=int8_t(task.status());value(h,r[0].rl);break;
    case S::BootMask:r[0].rl=int8_t(task.status()&0x10);value(h,r[0].rl);break;
    case S::BootReset:r[0].e=h.sub16(r[0].e,r[0].e);break;
    case S::BootDecrement:{
        const int result=int(oldRetries)-1;r[0].e=int16_t(task.retries());value(h,result);
        flag(h,2,(uint32_t(oldRetries)&~uint32_t(result))&0x8000);break;
    }
    case S::DispatchFault:dispatch(0x2304);return true;
    case S::TableAddress:r[4].r=int16_t(task.tableAddress());value(h,r[4].r);break;
    case S::DispatchAddress:dispatch(0x2416);return true;
    case S::ExponentialValue:r[1].er=int32_t(task.wordValue());value(h,r[1].er);break;
    case S::ExponentialFactor:r[2].e=int16_t(task.factor());value(h,r[2].e);break;
    case S::ExponentialCount:r[6].e=int16_t(task.exponentialRemaining());value(h,r[6].e);break;
    case S::DispatchExponential:case S::DispatchReciprocal:dispatch(0x2520);return true;
    case S::ProductSeed:r[3].r=int16_t(task.factor());value(h,r[3].r);break;
    case S::Product:r[3].er=int32_t(task.product());h.clockI(20);break;
    case S::ShiftSetup:r[0].rl=int8_t(task.shiftRemaining());value(h,r[0].rl);break;
    case S::Shift:r[3].er=int32_t(task.product());flag(h,1,oldProduct&1);value(h,r[3].er);break;
    case S::ShiftTick:projectSmallCount(r[0].rl,task.shiftRemaining());break;
    case S::AddDelta:h.add32(int32_t(oldValue),int32_t(oldProduct));r[1].er=int32_t(task.wordValue());break;
    case S::ExponentialTick:projectSmallCount(r[6].e,task.exponentialRemaining());break;
    case S::ReciprocalDivisor:r[2].r=int16_t(task.divisor());value(h,r[2].r);break;
    case S::ReciprocalCount:r[2].e=int16_t(task.reciprocalRemaining());value(h,r[2].e);break;
    case S::Numerator:r[3].er=0x800000;value(h,r[3].er);break;
    case S::DivideHigh:case S::DivideLow:{
        const auto result=task.division();r[3].er=int32_t((uint32_t(result.remainder)<<16)|result.quotient);
        flag(h,8,task.divisor()&0x8000);flag(h,4,!task.divisor());h.clockI(20);break;
    }
    case S::CopyHigh:r[1].e=int16_t(task.quotientHigh());value(h,r[1].e);break;
    case S::ClearDividend:r[3].r=h.sub16(int16_t(oldDivision.quotient),int16_t(oldDivision.quotient));break;
    case S::ComposeWord:r[1].r=int16_t(task.wordValue());value(h,r[1].r);break;
    case S::DivisorTick:projectSmallCount(r[2].r,task.divisor());break;
    case S::ReciprocalTick:projectSmallCount(r[2].e,task.reciprocalRemaining());break;
    default:break;
    }
    h.pc=h.makepc(point(task).location);return true;
}
}
