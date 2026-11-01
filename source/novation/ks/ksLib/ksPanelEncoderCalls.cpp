#include "ksPanelEncoderCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
PanelEncoderPartialTest partialTest=nullptr;using T=PanelEncoderTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,38> points{{{0x1886,4},{0x188a,2},{0x188c,6},{0x1892,6},{0x1898,2},{0x189a,2},{0x189c,2},{0x189e,6},{0x18a4,2},{0x18a6,2},{0x18a8,2},{0x18aa,2},{0x18ac,2},{0x18ae,2},{0x18b0,6},{0x18b6,2},{0x18b8,2},{0x18ba,2},{0x18bc,2},{0x18be,2},{0x18c0,2},{0x18c2,2},{0x18c4,6},{0x18ca,6},{0x18d0,2},{0x18d2,2},{0x18d4,6},{0x18da,6},{0x18e0,2},{0x18e2,2},{0x18e4,2},{0x18e6,6},{0x18ec,2},{0x18ee,2},{0x18f0,2},{0x18f2,2},{0x18f4,2},{0x18f6,2}}};
}
void setPanelEncoderPartialForTests(PanelEncoderPartialTest value){partialTest=value;}
bool PanelEncoderCalls::step(ControllerCpu& h){
    using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    if(pc==0x1886){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native encoder capacity exceeded");
        slot->emplace(h.getSP());++started;
    }
    if(!slot||points[unsigned((*slot)->task.stage())].pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;const auto stage=task.stage();
    const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,points[unsigned(stage)].length);
    auto read=[&](T::Read field)->uint16_t{
        if(field==T::Read::Timer)return uint16_t(h.read16(0xfffff6));
        return uint8_t(h.read8(field==T::Read::HalfHistory?0xffee73:field==T::Read::Activity?0xffee74:field==T::Read::QuarterHistory?0xffee72:0xffee75));
    };
    auto write=[&](T::Write field,uint8_t value){f.store8(field==T::Write::HalfHistory?0xffee73:field==T::Write::Activity?0xffee74:field==T::Write::QuarterHistory?0xffee72:0xffee75,int8_t(value));};
    if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native encoder admission differs");
    switch(stage){
    case S::ReadTimer:f.move(r[6].r,task.timer());break;
    case S::HalfTimer:f.shift(r[6].r,1,false);break;
    case S::ReadHalfHistory:f.move(r[5].rl,task.previousHalf());break;
    case S::HalfDifference:r[5].rl=h.sub8(r[5].rl,r[6].rl);break;
    case S::AbsHalfDifference:r[5].rl=h.sub8(0,r[5].rl);break;
    case S::ReadActivity:f.move(r[5].rh,task.activity());break;
    case S::AddActivity:r[5].rl=h.add8(r[5].rl,r[5].rh);break;
    case S::DecayActivity:f.inc(r[5].rl,-1);break;
    case S::CompareActivityLimit:h.sub8(r[5].rl,0xd);break;
    case S::ClipActivity:f.move(r[5].rl,0xd);break;
    case S::SetFloor:f.move(r[5].rh,0x4);break;
    case S::SubtractFloor:r[5].rl=h.sub8(r[5].rl,r[5].rh);break;
    case S::ClearFloor:f.move(r[5].rl,0x0);break;
    case S::IncrementGain:f.inc(r[5].rl,1);break;
    case S::QuarterTimer:f.shift(r[6].r,1,false);break;
    case S::RetainQuarter:f.move(r[6].rh,r[6].rl);break;
    case S::ReadQuarterHistory:f.move(r[6].rl,task.previousQuarter());break;
    case S::QuarterDifference:r[6].rl=h.sub8(r[6].rl,r[6].rh);break;
    case S::ReadDeltaHistory:f.move(r[6].rh,task.previousDelta());break;
    case S::CompareDirection:r[6].rh^=r[6].rl;value(h,r[6].rh);break;
    case S::ClearActivity:f.move(r[5].rl,0x0);break;
    case S::ResetGain:f.inc(r[5].rl,1);break;
    case S::TestDirection:f.move(r[6].rl,r[6].rl);break;
    case S::NegateGain:r[5].rl=h.sub8(0,r[5].rl);break;
    case S::BindResult:f.move(r[6].rl,r[5].rl);break;
    case S::Return:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native encoder clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    call.token=h.getSP();h.pc=h.makepc(points[unsigned(task.stage())].pc);return true;
}
}
