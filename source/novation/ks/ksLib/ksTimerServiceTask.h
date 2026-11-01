#pragma once
#include <array>
#include <cstdint>

namespace ks {

// The timer acknowledges the device, scans optional board inputs, advances
// the half-rate clock and requests panel decay every eighth published tick.
// Interrupt admission/return belongs to the caller, not an emulated CPU here.
class TimerInterruptTask final {
public:
    enum class Stage {
        AdmitContext, ReadStatus, PrepareAcknowledge, WriteAcknowledge,
        ReadBoardMode, SelectBoardScan, ScanBoard, ReadDivider, AdvanceDivider,
        WriteDivider, SelectClock, AdvanceClock, ReadCounter, IncrementCounter,
        WriteCounter, SelectDecayBits, SelectDecay, DecayPanel, ReleaseContext,
        Return, AwaitChild, Complete
    };
    enum class Read { Status, BoardMode, Divider, Counter };
    enum class Write { Acknowledge, Divider, Counter };
    enum class Child { BoardScan, Clock, PanelDecay };
    Stage stage() const { return stage_; }
    Stage resumeStage() const { return resume_; }
    Child child() const { return child_; }
    bool awaitingChild() const { return stage_ == Stage::AwaitChild; }
    bool complete() const { return stage_ == Stage::Complete; }
    uint8_t status() const { return status_; }
    uint8_t boardMode() const { return boardMode_; }
    uint8_t divider() const { return divider_; }
    uint32_t counter() const { return counter_; }
    bool resumeChild() {
        if (!awaitingChild()) return false;
        stage_=resume_; return true;
    }
    unsigned nextClocks(unsigned returnClocks=23) const {
        constexpr std::array<unsigned,20> costs{{
            20,4,3,4,12,3,20,12,3,12,3,20,24,3,24,3,3,20,20,23
        }};
        if (unsigned(stage_) >= costs.size()) return 0;
        return stage_ == Stage::Return ? returnClocks : costs[unsigned(stage_)];
    }
    template<class Reader, class Writer>
    unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned returnClocks=23) {
        const auto cost=nextClocks(returnClocks);
        if (!cost || cost>available) return 0;
        switch(stage_) {
        case Stage::ReadStatus: status_=uint8_t(read(Read::Status)); next(); break;
        case Stage::WriteAcknowledge: write(Write::Acknowledge,0xc0); next(); break;
        case Stage::ReadBoardMode: boardMode_=uint8_t(read(Read::BoardMode)); next(); break;
        case Stage::SelectBoardScan: stage_=boardMode_ ? Stage::ReadDivider : Stage::ScanBoard; break;
        case Stage::ScanBoard: await(Child::BoardScan,Stage::ReadDivider); break;
        case Stage::ReadDivider: divider_=uint8_t(read(Read::Divider)); next(); break;
        case Stage::AdvanceDivider: due_=divider_>=128; divider_=uint8_t(divider_+128); next(); break;
        case Stage::WriteDivider: write(Write::Divider,divider_); next(); break;
        case Stage::SelectClock: stage_=due_ ? Stage::AdvanceClock : Stage::ReleaseContext; break;
        case Stage::AdvanceClock: await(Child::Clock,Stage::ReadCounter); break;
        case Stage::ReadCounter: counter_=read(Read::Counter); next(); break;
        case Stage::IncrementCounter: ++counter_; next(); break;
        case Stage::WriteCounter: write(Write::Counter,counter_); next(); break;
        case Stage::SelectDecay: stage_=(counter_&7) ? Stage::ReleaseContext : Stage::DecayPanel; break;
        case Stage::DecayPanel: await(Child::PanelDecay,Stage::ReleaseContext); break;
        case Stage::Return: stage_=Stage::Complete; break;
        default: next(); break;
        }
        return cost;
    }
private:
    void next() { stage_=Stage(unsigned(stage_)+1); }
    void await(Child child,Stage resume) { child_=child; resume_=resume; stage_=Stage::AwaitChild; }
    Stage stage_=Stage::AdmitContext,resume_=Stage::Complete;
    Child child_=Child::BoardScan;
    uint8_t status_=0,boardMode_=0,divider_=0;
    uint32_t counter_=0;
    bool due_=false;
};

class ControllerClockReadTask final {
public:
    enum class Stage { SelectCounter, ReadCounter, Return, Complete };
    Stage stage() const { return stage_; }
    bool complete() const { return stage_==Stage::Complete; }
    uint32_t counter() const { return counter_; }
    unsigned nextClocks() const {
        constexpr std::array<unsigned,3> costs{{9,18,17}};
        return complete()?0:costs[unsigned(stage_)];
    }
    template<class Reader> unsigned advanceBefore(unsigned available,Reader&& read) {
        const auto cost=nextClocks();
        if (!cost || cost>available) return 0;
        if (stage_==Stage::ReadCounter) counter_=read();
        stage_=Stage(unsigned(stage_)+1);
        return cost;
    }
private:
    Stage stage_=Stage::SelectCounter;
    uint32_t counter_=0;
};

// Two independent modular phase accumulators share this tick. A primary
// carry increments the pulse count and may transmit F8. Flags and timeout
// are observed again after that child completes, including intervening IRQs.
class ClockAccumulatorTask final {
public:
    enum class Stage {
        AdmitPhase, AdmitCount, ReadIncrement, ReadPhase, ReadPulseCount,
        AdvancePhase, SelectCarry, IncrementPulseCount, ReadOutputFlags,
        SelectOutputBit, SelectOutput, AdmitOutput, PrepareClockByte,
        SendClock, ReleaseOutput, WritePhase, WritePulseCount,
        ReadSecondary, ReadSecondaryIncrement, AdvanceSecondary, WriteSecondary,
        ReadRunFlags, ReadTimeout, AdvanceTimeout, SelectTimeout, WriteTimeout,
        SkipTimeoutClear, ClearTimeoutFlag, WriteRunFlags,
        ReleaseCount, ReleasePhase, Return, AwaitOutput, Complete
    };
    enum class Read { Increment, Phase, PulseCount, OutputFlags, Secondary, SecondaryIncrement, RunFlags, Timeout };
    enum class Write { Phase, PulseCount, Secondary, Timeout, RunFlags };
    Stage stage() const { return stage_; }
    bool complete() const { return stage_==Stage::Complete; }
    bool awaitingOutput() const { return stage_==Stage::AwaitOutput; }
    bool resumeOutput() { if (!awaitingOutput()) return false; stage_=Stage::ReleaseOutput; return true; }
    uint32_t phase() const { return phase_; }
    uint32_t increment() const { return increment_; }
    uint16_t pulses() const { return pulses_; }
    uint8_t outputFlags() const { return outputFlags_; }
    uint32_t secondary() const { return secondary_; }
    uint32_t secondaryIncrement() const { return secondaryIncrement_; }
    uint8_t runFlags() const { return runFlags_; }
    uint8_t timeout() const { return timeout_; }
    unsigned nextClocks() const {
        constexpr std::array<unsigned,32> costs{{
            20,20,24,24,15,3,3,3,12,3,3,11,3,20,11,24,
            15,24,24,3,24,12,12,3,3,12,3,3,12,20,20,17
        }};
        return unsigned(stage_)<costs.size()?costs[unsigned(stage_)]:0;
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write) {
        const auto cost=nextClocks();
        if (!cost || cost>available) return 0;
        switch(stage_) {
        case Stage::ReadIncrement: increment_=read(Read::Increment); next(); break;
        case Stage::ReadPhase: phase_=read(Read::Phase); next(); break;
        case Stage::ReadPulseCount: pulses_=uint16_t(read(Read::PulseCount)); next(); break;
        case Stage::AdvancePhase: {
            const auto sum=uint64_t(phase_)+increment_;
            carry_=sum>0xffffffffu; phase_=uint32_t(sum); next(); break;
        }
        case Stage::SelectCarry: stage_=carry_?Stage::IncrementPulseCount:Stage::WritePhase; break;
        case Stage::IncrementPulseCount: ++pulses_; next(); break;
        case Stage::ReadOutputFlags: outputFlags_=uint8_t(read(Read::OutputFlags)); next(); break;
        case Stage::SelectOutput: stage_=(outputFlags_&2)?Stage::AdmitOutput:Stage::WritePhase; break;
        case Stage::SendClock: stage_=Stage::AwaitOutput; break;
        case Stage::WritePhase: write(Write::Phase,phase_); next(); break;
        case Stage::WritePulseCount: write(Write::PulseCount,pulses_); next(); break;
        case Stage::ReadSecondary: secondary_=read(Read::Secondary); next(); break;
        case Stage::ReadSecondaryIncrement: secondaryIncrement_=read(Read::SecondaryIncrement); next(); break;
        case Stage::AdvanceSecondary: secondary_+=secondaryIncrement_; next(); break;
        case Stage::WriteSecondary: write(Write::Secondary,secondary_); next(); break;
        case Stage::ReadRunFlags: runFlags_=uint8_t(read(Read::RunFlags)); next(); break;
        case Stage::ReadTimeout: timeout_=uint8_t(read(Read::Timeout)); next(); break;
        case Stage::AdvanceTimeout: ++timeout_; next(); break;
        case Stage::SelectTimeout: stage_=timeout_?Stage::WriteTimeout:Stage::ClearTimeoutFlag; break;
        case Stage::WriteTimeout: write(Write::Timeout,timeout_); next(); break;
        case Stage::SkipTimeoutClear: stage_=Stage::WriteRunFlags; break;
        case Stage::ClearTimeoutFlag: runFlags_&=0xfe; next(); break;
        case Stage::WriteRunFlags: write(Write::RunFlags,runFlags_); next(); break;
        case Stage::Return: stage_=Stage::Complete; break;
        default: next(); break;
        }
        return cost;
    }
private:
    void next() { stage_=Stage(unsigned(stage_)+1); }
    Stage stage_=Stage::AdmitPhase;
    uint32_t phase_=0,increment_=0,secondary_=0,secondaryIncrement_=0;
    uint16_t pulses_=0;
    uint8_t outputFlags_=0,runFlags_=0,timeout_=0;
    bool carry_=false;
};
}
