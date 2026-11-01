#include "ksParameterScaleCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
ParameterScalePartialTest partialTest=nullptr;
// This describes only the retained caller/IRQ projection, not native execution.
// Arithmetic, sign/shift decisions and completion time belong to the native task.
enum class Projection {Sign,SignChoice,Magnitude,MinimumCheck,MinimumChoice,Saturate,Exponent,ExponentHalf,ExponentQuarter,
    MantissaLow,MantissaHigh,ClearUpper,AdjustExponent,ZeroChoice,ShiftChoice,Scale,Count,Again,Scaled,
    OriginalSign,SignResultChoice,NegateResult,Return};
struct Point {unsigned location;Projection projection;unsigned iteration=0,length=2;};
Point point(const ParameterScaleTask& task,int16_t input,const PreparedParameterScale& p){
    using P=Projection;unsigned n=task.completedBoundaries();
    if(task.complete())return {0xa6d2,P::Return};
    if(n<2)return {0xa69eu + 2*n,n?P::SignChoice:P::Sign};n-=2;
    if(input<0){
        if(n<3){static constexpr Point prefix[]{{0xa6a2,P::Magnitude},{0xa6a4,P::MinimumCheck,0,4},{0xa6a8,P::MinimumChoice}};return prefix[n];}n-=3;
        if(input==-32768){if(!n)return {0xa6aa,P::Saturate};--n;}
    }
    if(n<8){static constexpr Projection prepare[]{P::Exponent,P::ExponentHalf,P::ExponentQuarter,P::MantissaLow,P::MantissaHigh,P::ClearUpper,P::AdjustExponent,P::ZeroChoice};return {0xa6ac+2*n,prepare[n]};}n-=8;
    if(p.shift){
        if(!n)return {0xa6bc,P::ShiftChoice};--n;
        const unsigned iterations=unsigned(p.shift<0?-p.shift:p.shift);
        if(n<3*iterations){static constexpr Projection scale[]{P::Scale,P::Count,P::Again};return {unsigned((p.shift>0?0xa6be:0xa6c6)+2*(n%3)),scale[n%3],n/3+1};}n-=3*iterations;
        if(p.shift>0){if(!n)return {0xa6c4,P::Scaled};--n;}
    }
    if(n<2)return {0xa6cc+2*n,n?P::SignResultChoice:P::OriginalSign};
    return {0xa6d0,P::NegateResult};
}
}
void setParameterScalePartialForTests(ParameterScalePartialTest fn){partialTest=fn;}
bool ParameterScaleCalls::step(ControllerCpu& h){
    using namespace controllerDetail;using P=Projection;
    const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
    for(auto& c:calls)if(c && c->callerToken==h.getSP()){selected=&c;break;}
    if(pc==0xa69e){
        if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
        if(!selected)throw std::runtime_error("Native KS coefficient-call capacity exceeded");
        selected->emplace(h.regs[0].r,h.getSP());++started;
    }
    if(!selected || point((*selected)->task,(*selected)->input,(*selected)->prepared).location!=pc){
        if(selected)selected->reset();
        if(partialTest && partialTest(h)){++partialEntries;return true;}
        return false;
    }
    auto& c=**selected;const auto p=point(c.task,c.input,c.prepared);Boundary clocks(h,p.location,p.length);auto& v=h.regs;
    if(p.projection==P::Return){
        v[0].er=c.task.result().coefficient;--h.indent;h.clockI(2);h.pc=h.popPC();selected->reset();++completed;return true;
    }
    const unsigned cost=c.task.nextBoundaryClocks();const auto progress=c.task.advanceBefore(cost);
    if(progress.clocks!=cost || progress.boundaries!=1)throw std::runtime_error("Native KS coefficient-call clock mismatch");
    const auto prepared=c.prepared;const unsigned magnitude=c.input<0?(c.input==-32768?32767u:unsigned(-int(c.input))):unsigned(c.input);
    switch(p.projection){
    case P::Sign:v[1].e=c.input;value(h,c.input);break;
    case P::Magnitude:v[0].r=h.sub16(0,c.input);break;
    case P::MinimumCheck:h.sub16(int16_t(uint16_t(-int(c.input))),-32768);break;
    case P::Saturate:v[0].r=32767;value(h,-32769);flag(h,2,true);break; // Admitted reference INC quirk.
    case P::Exponent:v[1].rl=int8_t(magnitude>>8);value(h,v[1].rl);break;
    case P::ExponentHalf:case P::ExponentQuarter:{
        const unsigned shift=p.projection==P::ExponentHalf?1:2,high=magnitude>>8;
        flag(h,1,high&(1u<<(shift-1)));v[1].rl=int8_t(high>>shift);value(h,v[1].rl);break;
    }
    case P::MantissaLow:v[0].rh=int8_t((magnitude>>8)&3);value(h,v[0].rh);break;
    case P::MantissaHigh:v[0].rh=int8_t(((magnitude>>8)&3)|4);value(h,v[0].rh);break;
    case P::ClearUpper:v[0].er=int32_t(prepared.mantissa);h.ccr=uint8_t((h.ccr&0xd0)|4);break;
    case P::AdjustExponent:v[1].rl=h.add8(int(magnitude>>10),-11);break;
    case P::Scale:{
        const bool left=prepared.shift>0;const unsigned n=p.iteration;
        const uint32_t before=left?prepared.mantissa<<(n-1):prepared.mantissa>>(n-1);
        const uint32_t scaled=left?prepared.mantissa<<n:prepared.mantissa>>n;
        flag(h,1,left?bool(before&0x80000000u):bool(before&1));v[0].er=int32_t(scaled);value(h,v[0].er);break;
    }
    case P::Count:{
        const int count=prepared.shift+(prepared.shift>0?-int(p.iteration):int(p.iteration));
        v[1].rl=int8_t(count);value(h,count);flag(h,2,false);break;
    }
    case P::OriginalSign:value(h,c.input);break;
    case P::NegateResult:{
        const int32_t scaled=-c.task.result().coefficient;
        h.ccr=controlArithmetic::subtract<int32_t>(0,scaled,h.ccr).flags;v[0].er=c.task.result().coefficient;break;
    }
    default:break;
    }
    h.pc=point(c.task,c.input,c.prepared).location;return true;
}
}
