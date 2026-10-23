#include "ksPanelAdcIrqCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
PanelAdcIrqPartialTest partialTest=nullptr;using T=PanelAdcIrqTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,101> points{{{0x165c,2},{0x165e,6},{0x1664,4},{0x1668,2},{0x166a,2},{0x166c,6},{0x1672,2},{0x1674,2},{0x1676,2},{0x1678,2},{0x167a,2},{0x167c,2},{0x167e,2},{0x1680,2},{0x1682,4},{0x1686,4},{0x168a,2},{0x168c,4},{0x1690,2},{0x1692,4},{0x1696,2},{0x1698,4},{0x169c,4},{0x16a0,2},{0x16a2,2},{0x16a4,2},{0x16e4,2},{0x16e6,2},{0x16e8,2},{0x16ea,2},{0x16ec,4},{0x16f0,4},{0x16f4,4},{0x16f8,4},{0x16fc,2},{0x16fe,2},{0x1700,6},{0x1706,2},{0x1708,2},{0x170a,6},{0x1710,2},{0x1712,2},{0x1714,6},{0x171a,2},{0x171c,4},{0x1720,2},{0x1722,4},{0x1726,4},{0x172a,2},{0x172c,4},{0x1730,2},{0x1732,4},{0x1736,2},{0x1738,2},{0x173a,2},{0x173c,4},{0x1740,4},{0x1744,2},{0x1746,2},{0x1748,4},{0x174c,4},{0x1750,2},{0x1752,4},{0x1756,2},{0x1758,2},{0x175a,2},{0x175c,6},{0x1762,2},{0x1764,2},{0x1766,2},{0x1768,4},{0x176c,2},{0x1792,6},{0x1798,2},{0x179a,2},{0x179c,2},{0x179e,2},{0x17a0,2},{0x17a2,2},{0x17a4,2},{0x17a6,2},{0x180c,2},{0x180e,2},{0x1810,2},{0x1812,2},{0x1814,4},{0x1818,6},{0x181e,2},{0x1820,6},{0x1826,2},{0x1828,2},{0x182a,4},{0x182e,2},{0x1830,2},{0x1832,4},{0x1836,4},{0x183a,4},{0x183e,4},{0x1842,2},{0x1844,2},{0x1846,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned address,int previous){address&=0xffffff;const bool fresh=(previous&1)||address!=(unsigned(previous)|1u);return address<0x400000?(fresh?3:0):address>=0xfffd10&&address<0xffffa0?(fresh?1:0):address>=0xffffa0?1:3;}
}
void setPanelAdcIrqPartialForTests(PanelAdcIrqPartialTest value){partialTest=value;}
bool PanelAdcIrqCalls::step(ControllerCpu& h){
    using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    if(pc==0x165c){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native ADC IRQ capacity exceeded");
        slot->emplace(h.getSP());++started;
    }
    if(!slot||point((*slot)->task).pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();
    const unsigned irqReturn=h.interruptMode2?23:17,countWrite=byteClocks(0x156e3,h.lastwrite);
    const auto cost=task.nextClocks(irqReturn,countWrite);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
    const unsigned address=0x1560a+task.offset();
    auto read=[&](T::Read field)->uint16_t{
        switch(field){
        case T::Read::Remaining:return uint8_t(h.read8(0x156e3));case T::Read::Status:return uint8_t(h.read8(0xffff98));
        case T::Read::Group:case T::Read::ScaleGroup:case T::Read::NextGroup:return uint8_t(h.read8(0x156e2));
        case T::Read::Current:return uint16_t(h.read16(address+72));
        case T::Read::History:case T::Read::Preview:return uint16_t(h.read16(address));
        case T::Read::Armed:return uint8_t(h.read8(address+144));case T::Read::Last:return uint8_t(h.read8(0x156e4));
        }return 0;
    };
    auto write=[&](T::Write field,uint16_t value){
        if(field==T::Write::History)f.store16(address,int16_t(value));
        else f.store8(field==T::Write::Remaining?0x156e3:field==T::Write::Status?0xffff98:field==T::Write::Armed?address+144:0x156e2,int8_t(value));
    };
    if(task.advanceBefore(cost,read,write,irqReturn,countWrite)!=cost)throw std::runtime_error("Native ADC IRQ admission differs");
    switch(stage){
    case S::SaveBase:f.push(r[0].r);break;
    case S::ReadRemaining:f.move(r[0].rl,task.remaining());break;
    case S::SelectScan:h.clockI(2);break;
    case S::SaveRemaining:f.move(r[0].rh,r[0].rl);break;
    case S::DecrementRemaining:f.inc(r[0].rl,-1);break;
    case S::ReadBusy:f.move(r[0].rl,task.status());break;
    case S::MaskBusy:r[0].rl&=0x7f;value(h,r[0].rl);break;
    case S::CompareRemaining:h.sub8(r[0].rh,0x1);break;
    case S::RestoreQuick:f.pop(r[0].r);break;
    case S::ReturnQuick:f.rte();break;
    case S::SaveShortHigh:f.push(r[0].e);break;
    case S::SaveShortFirst:f.push(r[1].er);break;
    case S::SaveShortSecond:f.push(r[2].er);break;
    case S::SaveShortCount:f.push(r[3].r);break;
    case S::SaveShortFourth:f.push(r[4].er);break;
    case S::SampleSettle:f.call(0x16a6);break;
    case S::RestoreShortFourth:f.pop(r[4].er);break;
    case S::RestoreShortCount:f.pop(r[3].r);break;
    case S::RestoreShortSecond:f.pop(r[2].er);break;
    case S::RestoreShortFirst:f.pop(r[1].er);break;
    case S::RestoreShortHigh:f.pop(r[0].e);break;
    case S::RestoreShortBase:f.pop(r[0].r);break;
    case S::ReturnSettle:f.rte();break;
    case S::ReadStatus:f.move(r[0].rl,task.status());break;
    case S::MaskStatus:r[0].rl&=0x5f;value(h,r[0].rl);break;
    case S::SaveLongHigh:f.push(r[0].e);break;
    case S::SaveLongFirst:f.push(r[1].er);break;
    case S::SaveLongSecond:f.push(r[2].er);break;
    case S::SaveLongCount:f.push(r[3].er);break;
    case S::SaveLongFourth:f.push(r[4].er);break;
    case S::SampleScan:f.call(0x16a6);break;
    case S::ClearGroup:r[0].er=h.sub32(r[0].er,r[0].er);break;
    case S::ReadGroup:f.move(r[0].rl,task.group());break;
    case S::CaptureChannel:f.move(r[3].rl,r[0].rl);break;
    case S::DoubleGroup:f.shift(r[0].rl,1,true);break;
    case S::LocateHistory:r[0].er=h.add32(r[0].er,0x1560a);break;
    case S::ClearTableIndex:r[4].er=h.sub32(r[4].er,r[4].er);break;
    case S::CaptureTableIndex:f.move(r[4].rl,r[3].rl);break;
    case S::LocateTable:r[4].er=h.add32(r[4].er,0xb93c);break;
    case S::SetChannels:f.move(r[3].rh,0x4);break;
    case S::ReadCurrent:f.move(r[1].r,task.current());break;
    case S::ReadHistory:f.move(r[1].e,task.history());break;
    case S::SetThreshold:f.move(r[2].e,0x200);break;
    case S::ReadArmed:f.move(r[2].rl,task.armed());break;
    case S::SetArmedThreshold:f.move(r[2].e,0x800);break;
    case S::Difference:r[1].e=h.sub16(r[1].e,r[1].r);break;
    case S::SelectStable:h.clockI(2);break;
    case S::NegateDifference:r[1].e=h.sub16(0,r[1].e);break;
    case S::CompareNegative:h.sub16(r[1].e,r[2].e);break;
    case S::AdmitNegative:h.clockI(2);break;
    case S::AdjustNegative:r[1].r=h.sub16(r[1].r,0x100);break;
    case S::ComparePositive:h.sub16(r[1].e,r[2].e);break;
    case S::AdmitPositive:h.clockI(2);break;
    case S::AdjustPositive:r[1].r=h.add16(r[1].r,0x100);break;
    case S::ClearArmed:r[2].rl=h.sub8(r[2].rl,r[2].rl);break;
    case S::ReadPreview:f.move(r[3].e,task.preview());break;
    case S::SaveAccepted:f.push(r[1].r);break;
    case S::ReadScaleGroup:f.move(r[1].rl,task.scaleGroup());break;
    case S::MaskScaleGroup:r[1].r&=0xff;value(h,r[1].r);break;
    case S::RetainScaleGroup:f.move(r[1].e,r[1].r);break;
    case S::RestoreAccepted:f.pop(r[1].r);break;
    case S::CompareSpecial:h.sub16(r[1].e,0x20);break;
    case S::ReadLast:f.move(r[2].rh,task.last());break;
    case S::CompareLast:h.sub8(r[3].rl,r[2].rh);break;
    case S::SavePreview:f.push(r[1].r);break;
    case S::BindPreview:f.move(r[1].r,r[3].e);break;
    case S::ScalePreview:f.call(0x17a8);break;
    case S::RestorePreview:f.pop(r[1].r);break;
    case S::ScaleAccepted:f.call(0x17a8);break;
    case S::AdvanceWord:f.inc(r[0].er,2);break;
    case S::AdvanceTable:f.inc(r[4].er,1);break;
    case S::AdvanceChannel:f.inc(r[3].rl,1);break;
    case S::DecrementChannels:f.inc(r[3].rh,-1);break;
    case S::MoreChannels:h.clockI(2);break;
    case S::ReadNextGroup:f.move(r[0].rl,task.nextGroup());break;
    case S::IncrementGroup:r[0].rl=h.add8(r[0].rl,0x4);break;
    case S::CompareNextGroup:h.sub8(r[0].rl,0x20);break;
    case S::SelectMux:f.call(0x1646);break;
    case S::SetConversion:f.move(r[0].rl,0x73);break;
    case S::RestoreLongFourth:f.pop(r[4].er);break;
    case S::RestoreLongCount:f.pop(r[3].er);break;
    case S::RestoreLongSecond:f.pop(r[2].er);break;
    case S::RestoreLongFirst:f.pop(r[1].er);break;
    case S::RestoreLongHigh:f.pop(r[0].e);break;
    case S::RestoreLongBase:f.pop(r[0].r);break;
    case S::ReturnScan:f.rte();break;
    default:break;
    }
    if(stage==S::ReturnQuick)++quickReturns;if(stage==S::ReturnSettle)++settleReturns;if(stage==S::ReturnScan)++scanReturns;
    if(stage==S::WriteHistory)++acceptedChannels;if(stage==S::ScalePreview)++previewHandoffs;
    if(stage==S::SelectSpecial&&task.awaitingChild())++specialHandoffs;if(stage==S::SelectMux)++conversionRestarts;
    if(h.cycles-before!=cost)throw std::runtime_error("Native ADC IRQ clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
