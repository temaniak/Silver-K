#include "ksTimerServiceCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>

namespace ks {
namespace {
TimerServicePartialTest partialTest=nullptr;
struct Point {unsigned pc,length;};
constexpr std::array<Point,20> interruptPoints{{
    {0x12b0,4},{0x12b4,2},{0x12b6,2},{0x12b8,2},{0x12ba,6},
    {0x12c0,2},{0x12c2,4},{0x12c6,6},{0x12cc,2},{0x12ce,6},
    {0x12d4,2},{0x12d6,4},{0x12da,8},{0x12e2,2},{0x12e4,8},
    {0x12ec,2},{0x12ee,2},{0x12f0,4},{0x12f4,4},{0x12f8,2}
}};
constexpr std::array<Point,3> readPoints{{{0x12fa,6},{0x1300,4},{0x1304,2}}};
constexpr std::array<Point,32> tickPoints{{
    {0x27be,4},{0x27c2,4},{0x27c6,8},{0x27ce,8},{0x27d6,6},
    {0x27dc,2},{0x27de,2},{0x27e0,2},{0x27e2,6},{0x27e8,2},
    {0x27ea,2},{0x27ec,2},{0x27ee,2},{0x27f0,4},{0x27f4,2},
    {0x27f6,8},{0x27fe,6},{0x2804,8},{0x280c,8},{0x2814,2},
    {0x2816,8},{0x281e,6},{0x2824,6},{0x282a,2},{0x282c,2},
    {0x282e,6},{0x2834,2},{0x2836,2},{0x2838,6},{0x283e,4},
    {0x2842,4},{0x2846,2}
}};
template<class Slots> auto find(Slots& slots,uint32_t stack)->typename Slots::value_type* {
    for(auto& slot:slots)if(slot&&slot->token==stack)return &slot;
    return nullptr;
}
template<class Slots> auto free(Slots& slots)->typename Slots::value_type* {
    for(auto& slot:slots)if(!slot)return &slot;
    throw std::runtime_error("Native timer call capacity exceeded");
}
}
void setTimerServicePartialForTests(TimerServicePartialTest value){partialTest=value;}
bool TimerServiceCalls::step(ControllerCpu& h){
    const auto pc=unsigned(h.getPC());
    return pc>=0x27be?tick(h):pc>=0x12fa?read(h):interrupt(h);
}

bool TimerServiceCalls::interrupt(ControllerCpu& h){
    using namespace controllerDetail;
    using T=TimerInterruptTask;using S=T::Stage;
    const auto pc=unsigned(h.getPC());auto* slot=find(interrupts_,h.getSP());
    if(pc==0x12b0){if(!slot)slot=free(interrupts_);slot->emplace(h.getSP());++started;}
    const auto pointOf=[](const T& t){return interruptPoints[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];};
    if(!slot||pointOf((*slot)->task).pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;
    if(task.awaitingChild())task.resumeChild();
    const auto stage=task.stage();const auto point=pointOf(task);
    const auto cost=task.nextClocks(h.interruptMode2?23:17);const auto before=h.cycles;
    RuntimeBoundary f(h,pc,point.length);
    auto read=[&](T::Read field)->uint32_t{
        switch(field){
        case T::Read::Status:return uint8_t(h.read8(0xffffd5));
        case T::Read::BoardMode:return uint8_t(h.read8(0xffec14));
        case T::Read::Divider:return uint8_t(h.read8(0xffee70));
        case T::Read::Counter:return h.read32(0xffec10);
        }return 0;
    };
    auto write=[&](T::Write field,uint32_t value){
        switch(field){
        case T::Write::Acknowledge:f.store8(0xffffd5,int8_t(value));break;
        case T::Write::Divider:f.store8(0xffee70,int8_t(value));break;
        case T::Write::Counter:f.store32(0xffec10,int32_t(value));break;
        }
    };
    if(task.advanceBefore(cost,read,write,h.interruptMode2?23:17)!=cost)throw std::runtime_error("Native timer IRQ admission differs");
    switch(stage){
    case S::AdmitContext:f.push(r[0].er);break;
    case S::ReadStatus:f.move(r[0].rl,task.status());break;
    case S::PrepareAcknowledge:f.move(r[0].rl,int8_t(0xc0));break;
    case S::ReadBoardMode:f.move(r[0].rl,task.boardMode());break;
    case S::ScanBoard:f.call(0x1b7e);break;
    case S::ReadDivider:f.move(r[0].rl,task.divider());break;
    case S::AdvanceDivider:r[0].rl=h.add8(r[0].rl,int8_t(0x80));break;
    case S::AdvanceClock:f.call(0x27be);break;
    case S::ReadCounter:f.move(r[0].er,int32_t(task.counter()));break;
    case S::IncrementCounter:f.inc(r[0].er,1);break;
    case S::SelectDecayBits:r[0].rl&=7;value(h,r[0].rl);break;
    case S::DecayPanel:f.call(0x70e);break;
    case S::ReleaseContext:f.pop(r[0].er);break;
    case S::Return:f.rte();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native timer IRQ clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    if(!task.awaitingChild())call.token=h.getSP();
    h.pc=h.makepc(task.awaitingChild()?f.next:pointOf(task).pc);return true;
}

bool TimerServiceCalls::read(ControllerCpu& h){
    using namespace controllerDetail;using S=ControllerClockReadTask::Stage;
    const auto pc=unsigned(h.getPC());auto* slot=find(reads_,h.getSP());
    if(pc==0x12fa){if(!slot)slot=free(reads_);slot->emplace(h.getSP());++started;}
    if(!slot||readPoints[unsigned((*slot)->task.stage())].pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;const auto stage=task.stage();const auto point=readPoints[unsigned(stage)];
    const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point.length);
    if(task.advanceBefore(cost,[&]{return h.read32(0xffec10);})!=cost)throw std::runtime_error("Native clock read admission differs");
    switch(stage){
    case S::SelectCounter:f.move(h.regs[6].er,0xffec10);break;
    case S::ReadCounter:f.move(h.regs[6].er,int32_t(task.counter()));break;
    case S::Return:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native clock read clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    call.token=h.getSP();h.pc=h.makepc(readPoints[unsigned(task.stage())].pc);return true;
}

bool TimerServiceCalls::tick(ControllerCpu& h){
    using namespace controllerDetail;using T=ClockAccumulatorTask;using S=T::Stage;
    const auto pc=unsigned(h.getPC());auto* slot=find(ticks_,h.getSP());
    if(pc==0x27be){if(!slot)slot=free(ticks_);slot->emplace(h.getSP());++started;}
    const auto pointOf=[](const T& t){return tickPoints[unsigned(t.awaitingOutput()?S::ReleaseOutput:t.stage())];};
    if(!slot||pointOf((*slot)->task).pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;
    if(task.awaitingOutput())task.resumeOutput();
    const auto stage=task.stage();const auto point=pointOf(task);const auto cost=task.nextClocks();
    const auto before=h.cycles;RuntimeBoundary f(h,pc,point.length);
    auto read=[&](T::Read field)->uint32_t{
        switch(field){
        case T::Read::Increment:return h.read32(0xffee7e);
        case T::Read::Phase:return h.read32(0xffee7a);
        case T::Read::PulseCount:return h.read16(0xffee78);
        case T::Read::OutputFlags:return uint8_t(h.read8(0xffee97));
        case T::Read::Secondary:return h.read32(0xffee82);
        case T::Read::SecondaryIncrement:return h.read32(0xffee86);
        case T::Read::RunFlags:return uint8_t(h.read8(0xffee97));
        case T::Read::Timeout:return uint8_t(h.read8(0xffee9a));
        }return 0;
    };
    auto write=[&](T::Write field,uint32_t value){
        switch(field){
        case T::Write::Phase:f.store32(0xffee7a,int32_t(value));break;
        case T::Write::PulseCount:f.store16(0xffee78,int16_t(value));break;
        case T::Write::Secondary:f.store32(0xffee82,int32_t(value));break;
        case T::Write::Timeout:f.store8(0xffee9a,int8_t(value));break;
        case T::Write::RunFlags:f.store8(0xffee97,int8_t(value));break;
        }
    };
    if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native clock accumulator admission differs");
    switch(stage){
    case S::AdmitPhase:f.push(r[1].er);break;
    case S::AdmitCount:f.push(r[2].er);break;
    case S::ReadIncrement:f.move(r[1].er,int32_t(task.increment()));break;
    case S::ReadPhase:f.move(r[0].er,int32_t(task.phase()));break;
    case S::ReadPulseCount:f.move(r[2].e,task.pulses());break;
    case S::AdvancePhase:r[0].er=h.add32(r[0].er,r[1].er);break;
    case S::IncrementPulseCount:f.inc(r[2].e,1);break;
    case S::ReadOutputFlags:f.move(r[1].rl,task.outputFlags());break;
    case S::SelectOutputBit:flag(h,4,(uint8_t(r[1].rl)&2)==0);break;
    case S::AdmitOutput:f.push(r[6].r);break;
    case S::PrepareClockByte:f.move(r[6].rl,int8_t(0xf8));break;
    case S::SendClock:f.call(0x1104);break;
    case S::ReleaseOutput:f.pop(r[6].r);break;
    case S::ReadSecondary:f.move(r[0].er,int32_t(task.secondary()));break;
    case S::ReadSecondaryIncrement:f.move(r[1].er,int32_t(task.secondaryIncrement()));break;
    case S::AdvanceSecondary:r[0].er=h.add32(r[0].er,r[1].er);break;
    case S::ReadRunFlags:f.move(r[0].rh,task.runFlags());break;
    case S::ReadTimeout:f.move(r[0].rl,task.timeout());break;
    case S::AdvanceTimeout:f.inc(r[0].rl,1);break;
    case S::ClearTimeoutFlag:r[0].rh&=uint8_t(0xfe);break;
    case S::ReleaseCount:f.pop(r[2].er);break;
    case S::ReleasePhase:f.pop(r[1].er);break;
    case S::Return:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native clock accumulator clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    if(!task.awaitingOutput())call.token=h.getSP();
    h.pc=h.makepc(task.awaitingOutput()?f.next:pointOf(task).pc);return true;
}
}
