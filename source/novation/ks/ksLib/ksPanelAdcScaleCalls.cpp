#include "ksPanelAdcScaleCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
PanelAdcScalePartialTest partialTest=nullptr;using T=PanelAdcScaleTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,50> points{{{0x176e,4},{0x1772,2},{0x1774,2},{0x1776,2},{0x1778,2},{0x177a,6},{0x1780,2},{0x1782,2},{0x1784,6},{0x178a,2},{0x178c,2},{0x178e,4},{0x17a8,2},{0x17aa,2},{0x17ac,4},{0x17b0,2},{0x17b2,2},{0x17b4,2},{0x17b6,2},{0x17b8,6},{0x17be,2},{0x17c0,2},{0x17c2,6},{0x17c8,2},{0x17ca,2},{0x17cc,2},{0x17ce,2},{0x17d0,2},{0x17d2,2},{0x17d4,2},{0x17d6,2},{0x17d8,2},{0x17da,2},{0x17dc,2},{0x17de,4},{0x17e2,2},{0x17e4,2},{0x17e6,2},{0x17e8,2},{0x17ea,6},{0x17f0,2},{0x17f2,2},{0x17f4,6},{0x17fa,2},{0x17fc,2},{0x17fe,6},{0x1804,2},{0x1806,2},{0x1808,2},{0x180a,2}}};
}
void setPanelAdcScalePartialForTests(PanelAdcScalePartialTest value){partialTest=value;}
bool PanelAdcScaleCalls::step(ControllerCpu& h){
    using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    if(pc==0x176e||pc==0x17a8){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native ADC scale capacity exceeded");
        slot->emplace(pc==0x176e?T::Kind::Special:T::Kind::Ordinary,h.getSP(),uint16_t(h.regs[1].r),uint8_t(h.regs[3].rl),uint32_t(h.regs[4].er));++started;
    }
    if(!slot||points[unsigned((*slot)->task.stage())].pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;const auto stage=task.stage();
    const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,points[unsigned(stage)].length);
    auto read=[&](T::Read)->uint8_t{return uint8_t(h.read8(call.table));};
    auto write=[&](T::Write,uint8_t value){f.store8(0x156e4,int8_t(value));};
    if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native ADC scale admission differs");
    switch(stage){
    case S::SpecialWeight:f.move(r[1].e,0x8207);break;
    case S::SpecialMultiply:r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[1].e));h.clockI(20);break;
    case S::SpecialHalf:f.move(r[1].r,r[1].e);break;
    case S::SpecialClearHigh:r[1].e=h.sub16(r[1].e,r[1].e);break;
    case S::SpecialDouble:f.shift(r[1].er,1,true);break;
    case S::SpecialOffset:r[1].er=h.sub32(r[1].er,0x200);break;
    case S::SpecialFloor:r[1].er=h.sub32(r[1].er,r[1].er);break;
    case S::SpecialCompareLimit:h.sub32(r[1].er,0xff00);break;
    case S::SpecialLimit:f.move(r[1].rh,0xffffffff);break;
    case S::SpecialPublish:h.clockI(2);break;
    case S::ReadFlag:f.move(r[2].rh,task.curveFlag());break;
    case S::BipolarWeight:f.move(r[1].e,0x43cc);break;
    case S::BipolarMultiply:r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[1].e));h.clockI(20);break;
    case S::BipolarHalf:f.move(r[1].r,r[1].e);break;
    case S::BipolarClearHigh:r[1].e=h.sub16(r[1].e,r[1].e);break;
    case S::BipolarDouble:f.shift(r[1].er,1,true);break;
    case S::BipolarOffset:r[1].er=h.sub32(r[1].er,0x100);break;
    case S::BipolarFloor:r[1].er=h.sub32(r[1].er,r[1].er);break;
    case S::BipolarCompareLimit:h.sub32(r[1].er,0x8400);break;
    case S::BipolarLimit:f.move(r[1].rh,0xffffff84);break;
    case S::CopyCenterValue:f.move(r[1].rl,r[1].rh);break;
    case S::SetCenter:f.move(r[2].rh,0x40);break;
    case S::SubtractCenter:r[1].rl=h.sub8(r[1].rl,r[2].rh);break;
    case S::CompareCenterWidth:h.sub8(r[1].rl,0x5);break;
    case S::LimitCenterWidth:f.move(r[1].rl,0x5);break;
    case S::ApplyCenterWidth:r[1].rh=h.sub8(r[1].rh,r[1].rl);break;
    case S::LinearWeight:f.move(r[1].e,0x4144);break;
    case S::LinearMultiply:r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[1].e));h.clockI(20);break;
    case S::LinearHalf:f.move(r[1].r,r[1].e);break;
    case S::LinearClearHigh:r[1].e=h.sub16(r[1].e,r[1].e);break;
    case S::LinearDouble:f.shift(r[1].er,1,true);break;
    case S::LinearOffset:r[1].er=h.sub32(r[1].er,0x100);break;
    case S::LinearFloor:r[1].er=h.sub32(r[1].er,r[1].er);break;
    case S::LinearCompareLimit:h.sub32(r[1].er,0x7f00);break;
    case S::LinearLimit:f.move(r[1].rh,0x7f);break;
    case S::BindChannel:f.move(r[1].rl,r[3].rl);break;
    case S::Handoff:f.next=0x1848;break;
    case S::BindSpecialChannel:f.move(r[1].rl,r[3].rl);break;
    case S::DispatchSpecial:f.call(0x1848);break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native ADC scale clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    call.token=h.getSP();h.pc=h.makepc(points[unsigned(task.stage())].pc);return true;
}
}
