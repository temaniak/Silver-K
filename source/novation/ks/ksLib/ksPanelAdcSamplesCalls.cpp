#include "ksPanelAdcSamplesCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
PanelAdcSamplesPartialTest partialTest=nullptr;using T=PanelAdcSamplesTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,22> points{{{0x16a6,2},{0x16a8,6},{0x16ae,2},{0x16b0,6},{0x16b6,6},{0x16bc,2},{0x16be,2},{0x16c0,2},{0x16c2,2},{0x16c4,4},{0x16c8,2},{0x16ca,2},{0x16cc,4},{0x16d0,2},{0x16d2,2},{0x16d4,2},{0x16d6,4},{0x16da,2},{0x16dc,2},{0x16de,2},{0x16e0,2},{0x16e2,2}}};
}
void setPanelAdcSamplesPartialForTests(PanelAdcSamplesPartialTest value){partialTest=value;}
bool PanelAdcSamplesCalls::step(ControllerCpu& h){
    using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    if(pc==0x16a6){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native ADC samples capacity exceeded");
        slot->emplace(h.getSP());++started;
    }
    if(!slot||points[unsigned((*slot)->task.stage())].pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;const auto stage=task.stage();
    const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,points[unsigned(stage)].length);
    const unsigned sampleAddress=0x15652+task.offset()+2*task.channel();
    auto read=[&](T::Read field)->uint16_t{
        if(field==T::Read::Group)return uint8_t(h.read8(0x156e2));
        if(field==T::Read::Previous)return uint16_t(h.read16(sampleAddress));
        return uint8_t(h.read8(0xffff90+task.channel()*2+(field==T::Read::Low)));
    };
    auto write=[&](uint16_t value){f.store16(sampleAddress,int16_t(value));};
    if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native ADC samples admission differs");
    switch(stage){
    case S::ClearGroup:r[0].er=h.sub32(r[0].er,r[0].er);break;
    case S::ReadGroup:f.move(r[0].rl,task.group());break;
    case S::ScaleGroup:f.shift(r[0].rl,1,true);break;
    case S::SelectSamples:r[0].er=h.add32(r[0].er,0x15652);break;
    case S::SelectAdc:f.move(r[2].er,0xffffff90);break;
    case S::SetCount:f.move(r[3].rh,0x4);break;
    case S::ReadHigh:f.move(r[1].rh,task.high());r[2].er+=1;h.clockI(2);break;
    case S::ReadLow:f.move(r[1].rl,task.low());r[2].er+=1;h.clockI(2);break;
    case S::ReadPrevious:f.move(r[1].e,task.previous());break;
    case S::SetRawWeight:f.move(r[4].r,0x8000);break;
    case S::WeightRaw:r[4].er=uint16_t(r[4].er)*uint32_t(uint16_t(r[1].r));h.clockI(20);break;
    case S::BindRawHalf:f.move(r[1].r,r[4].e);break;
    case S::SetPreviousWeight:f.move(r[4].r,0x8000);break;
    case S::WeightPrevious:r[4].er=uint16_t(r[4].er)*uint32_t(uint16_t(r[1].e));h.clockI(20);break;
    case S::AddHalves:r[1].r=h.add16(r[1].r,r[4].e);break;
    case S::Saturate:f.move(r[1].r,0xffff);break;
    case S::AdvanceSample:f.inc(r[0].er,2);break;
    case S::DecrementCount:f.inc(r[3].rh,-1);break;
    case S::Return:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native ADC samples clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    call.token=h.getSP();h.pc=h.makepc(points[unsigned(task.stage())].pc);return true;
}
}
