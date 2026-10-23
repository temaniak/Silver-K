#include "ksPanelAdcCalibrationCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
PanelAdcCalibrationPartialTest partialTest=nullptr;using T=PanelAdcCalibrationTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,26> points{{{0x158c,4},{0x1590,4},{0x1594,6},{0x159a,2},{0x159c,2},{0x159e,4},{0x15a2,2},{0x15a4,4},{0x15a8,2},{0x15aa,2},{0x15ac,2},{0x15ae,4},{0x15b2,4},{0x15b6,2},{0x15b8,4},{0x15bc,2},{0x15be,2},{0x15c0,2},{0x15c2,6},{0x15c8,2},{0x15ca,4},{0x15ce,2},{0x15d0,2},{0x15d2,2},{0x15d4,4},{0x15d8,2}}};
}
void setPanelAdcCalibrationPartialForTests(PanelAdcCalibrationPartialTest value){partialTest=value;}
bool PanelAdcCalibrationCalls::step(ControllerCpu& h){
    using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    if(pc==0x158c||pc==0x15b8){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native ADC calibration capacity exceeded");
        slot->emplace(pc==0x158c?T::Kind::Snapshot:T::Kind::Read,h.getSP(),uint8_t(h.regs[6].rl));++started;
    }
    if(!slot||points[unsigned((*slot)->task.stage())].pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;const auto stage=task.stage();
    const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,points[unsigned(stage)].length);
    const unsigned address=0x1560a+task.offset();
    auto read=[&](T::Read field)->uint16_t{return uint16_t(h.read16(address+(field==T::Read::Sample?72:0)));};
    auto write=[&](T::Write field,uint16_t value){if(field==T::Write::History)f.store16(address,int16_t(value));else f.store8(address+144,int8_t(value));};
    if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native ADC calibration admission differs");
    switch(stage){
    case S::SaveHistoryAddress:f.push(r[0].er);break;
    case S::SaveSample:f.push(r[1].er);break;
    case S::SelectHistory:f.move(r[0].er,0x1560a);break;
    case S::SetCount:f.move(r[1].rl,0x24);break;
    case S::ArmThreshold:f.move(r[1].rh,0x1);break;
    case S::ReadSample:f.move(r[1].e,task.sample());break;
    case S::AdvanceHistory:f.inc(r[0].er,2);break;
    case S::DecrementCount:f.inc(r[1].rl,-1);break;
    case S::RestoreSample:f.pop(r[1].er);break;
    case S::RestoreHistoryAddress:f.pop(r[0].er);break;
    case S::ReturnSnapshot:f.ret();break;
    case S::SaveAddress:f.push(r[0].er);break;
    case S::ClearIndex:r[0].er=h.sub32(r[0].er,r[0].er);break;
    case S::CaptureIndex:f.move(r[0].rl,r[6].rl);break;
    case S::DoubleIndex:f.shift(r[0].rl,1,true);break;
    case S::LocateHistory:r[0].er=h.add32(r[0].er,0x1560a);break;
    case S::ReadHistory:f.move(r[0].r,task.sample());break;
    case S::SubtractFloor:r[0].r=h.sub16(r[0].r,0x100);break;
    case S::ClearFloor:r[0].r=h.sub16(r[0].r,r[0].r);break;
    case S::BindValue:f.move(r[6].rl,r[0].rh);break;
    case S::RestoreAddress:f.pop(r[0].er);break;
    case S::ReturnValue:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native ADC calibration clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    call.token=h.getSP();h.pc=h.makepc(points[unsigned(task.stage())].pc);return true;
}
}
