#pragma once
#include <cstdint>
namespace ks {
// Semantic timeout notification: toggle the fault latch and pulse the display
// control. IO observations remain live, including both read/modify/writes.
// Admission/release intervals retain the independently admitted 25 MHz timing.
// There are no processor registers, flags, stack, PC or device addresses here.
class DspFaultTask final {
public:
    enum class Read {Latch,Control};
    enum class Write {Latch,Data,Control};
    enum class Stage {Admission,LatchAdmission,ReadLatch,Toggle,PublishLatch,WriteData,
        ControlSetup,WriteControl,BeginPulse,DelayAdmission,DelaySetup,DelayTick,
        DelayGate,DelayRelease,EndPulse,LatchRelease,CompletionDelay,Complete};
    struct Advance {unsigned clocks=0,boundaries=0;};
    constexpr Stage stage()const{return stage_;}
    constexpr bool complete()const{return stage_==Stage::Complete;}
    constexpr uint8_t latch()const{return latch_;}
    constexpr unsigned remainingTicks()const{return ticks_;}
    constexpr unsigned elapsedClocks()const{return elapsed_;}
    constexpr unsigned nextBoundaryClocks()const{
        switch(stage_){
        case Stage::Admission:return 20;
        case Stage::LatchAdmission:case Stage::DelayAdmission:case Stage::DelayRelease:case Stage::LatchRelease:return 11;
        case Stage::ReadLatch:case Stage::PublishLatch:return 12;
        case Stage::WriteData:case Stage::WriteControl:return 4;
        case Stage::BeginPulse:case Stage::EndPulse:return 8;
        case Stage::DelaySetup:return 6;
        case Stage::CompletionDelay:return 17;
        case Stage::Complete:return 0;
        default:return 3;
        }
    }
    template<class Reader,class Writer>
    Advance advanceBefore(unsigned available,Reader&& read,Writer&& write){
        Advance progress;
        while(const unsigned cost=nextBoundaryClocks()){
            if(cost>available)break;
            switch(stage_){
            case Stage::Admission:stage_=Stage::LatchAdmission;break;
            case Stage::LatchAdmission:stage_=Stage::ReadLatch;break;
            case Stage::ReadLatch:latch_=uint8_t(read(Read::Latch));stage_=Stage::Toggle;break;
            case Stage::Toggle:latch_^=7;stage_=Stage::PublishLatch;break;
            case Stage::PublishLatch:write(Write::Latch,latch_);stage_=Stage::WriteData;break;
            case Stage::WriteData:write(Write::Data,latch_);stage_=Stage::ControlSetup;break;
            case Stage::ControlSetup:stage_=Stage::WriteControl;break;
            case Stage::WriteControl:write(Write::Control,0x36);stage_=Stage::BeginPulse;break;
            case Stage::BeginPulse:write(Write::Control,uint8_t(read(Read::Control))&uint8_t(~0x20));stage_=Stage::DelayAdmission;break;
            case Stage::DelayAdmission:stage_=Stage::DelaySetup;break;
            case Stage::DelaySetup:ticks_=3;stage_=Stage::DelayTick;break;
            case Stage::DelayTick:--ticks_;stage_=Stage::DelayGate;break;
            case Stage::DelayGate:stage_=ticks_?Stage::DelayTick:Stage::DelayRelease;break;
            case Stage::DelayRelease:stage_=Stage::EndPulse;break;
            case Stage::EndPulse:write(Write::Control,uint8_t(read(Read::Control))|0x20);stage_=Stage::LatchRelease;break;
            case Stage::LatchRelease:stage_=Stage::CompletionDelay;break;
            case Stage::CompletionDelay:stage_=Stage::Complete;break;
            default:break;
            }
            elapsed_+=cost;available-=cost;progress.clocks+=cost;++progress.boundaries;
        }
        return progress;
    }
private:
    Stage stage_=Stage::Admission;
    uint8_t latch_=0;
    unsigned ticks_=0,elapsed_=0;
};
}
