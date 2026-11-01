#pragma once
#include <array>
#include <cstdint>

namespace ks {

// A page is observed byte by byte, including changes made while suspended.
// Source/target are data resource references, never execution addresses.
class FlashPageCompareTask final {
public:
    enum class Stage {
        AdmitContext, AdmitTarget, AdmitSource, AlignPage, BeginScan,
        ReadSource, ReadTarget, Compare, AdmitDifference, NextByte, AdmitNext,
        DiscardSource, DiscardTarget, SkipDifference, RestoreSource,
        RestoreTarget, MarkDifference, ReleaseContext, Return, Complete
    };
    FlashPageCompareTask(uint32_t source, uint32_t target)
        : source_(source), target_(target) {}
    Stage stage() const { return stage_; }
    bool complete() const { return stage_ == Stage::Complete; }
    unsigned position() const { return position_; }
    uint8_t sourceByte() const { return sourceByte_; }
    uint8_t targetByte() const { return targetByte_; }
    bool differs() const { return sourceByte_ != targetByte_; }
    uint32_t source() const { return source_; }
    uint32_t target() const { return target_; }
    unsigned nextClocks(unsigned readClocks = 3) const {
        constexpr std::array<unsigned, 19> costs{{
            20,20,20,9,6,8,8,3,3,3,3,20,20,3,20,20,6,20,17
        }};
        if (complete()) return 0;
        if (stage_ == Stage::ReadSource || stage_ == Stage::ReadTarget)
            return 5 + readClocks;
        return costs[unsigned(stage_)];
    }
    template<class Reader>
    unsigned advanceBefore(unsigned available, Reader&& read,
                           unsigned readClocks = 3) {
        const auto cost = nextClocks(readClocks);
        if (!cost || cost > available) return 0;
        switch (stage_) {
        case Stage::ReadSource: sourceByte_ = read(false, position_); next(); break;
        case Stage::ReadTarget: targetByte_ = read(true, position_); next(); break;
        case Stage::AdmitDifference:
            stage_ = differs() ? Stage::RestoreSource : Stage::NextByte; break;
        case Stage::NextByte: ++position_; next(); break;
        case Stage::AdmitNext:
            stage_ = position_ < 256 ? Stage::ReadSource : Stage::DiscardSource; break;
        case Stage::SkipDifference:
            source_ += 256;
            target_ = (target_ & 0x00ffff00u) + 256;
            stage_ = Stage::ReleaseContext; break;
        case Stage::MarkDifference: target_ |= 0x80000000u; next(); break;
        default: next(); break;
        }
        return cost;
    }
private:
    void next() { stage_ = Stage(unsigned(stage_) + 1); }
    Stage stage_ = Stage::AdmitContext;
    uint32_t source_, target_;
    unsigned position_ = 0;
    uint8_t sourceByte_ = 0, targetByte_ = 0;
};

// Native compare/program/verify policy. The caller supplies live page data,
// pending-write count and timer ticks. It also owns the bounded write-window
// priority admission; no CPU register, flags, stack or PC is stored here.
class FlashPageProgramTask final {
public:
    enum class Stage {
        AdmitContext, AdmitCommand, AdmitByte, AdmitClock, AdmitEntry,
        SelectTarget, SelectFlash, Offset, AdmitFlash, CompareProtected,
        AdmitProtected, SkipSource, SkipTarget, FinishProtected,
        CheckAlignment, AdmitAlignment, ComparePage, CheckDifference,
        AdmitDifference, ClearDifference, ReadPending, IncrementPending,
        WritePending, SelectBank, NormalizeBank, FirstCommand,
        WriteFirstCommand, SecondCommand, WriteSecondCommand, ProgramCommand,
        WriteProgramCommand, SelectWriteTarget, SelectWriteSource, BeginWrite,
        EnterWriteWindow, RaiseWritePriority, ReadSource, WriteTarget,
        NextTarget, NextByte, AdmitNext, LeaveWriteWindow, SelectPollTarget,
        ReadStartTick, ReadCurrentTick, Elapsed, CompareTimeout, AdmitTimeout,
        ReadReady, CompareReady, AdmitReady, VerifyPage, FinishVerify, Fail,
        ReleaseClock, ReleaseByte, ReleaseCommand, ReleaseContext, Return,
        AwaitCompare, Complete
    };
    enum class Read { Pending, Source, StartTick, CurrentTick, Ready };
    enum class Write { Pending, FirstCommand, SecondCommand, ProgramCommand, Byte };
    FlashPageProgramTask(uint32_t source, uint32_t target)
        : source_(source), target_(target) {}
    Stage stage() const { return stage_; }
    bool complete() const { return stage_ == Stage::Complete; }
    bool awaitingCompare() const { return stage_ == Stage::AwaitCompare; }
    Stage resumeStage() const { return resume_; }
    uint32_t source() const { return source_; }
    uint32_t target() const { return target_; }
    uint32_t bank() const { return target_ & 0x00f80000u; }
    unsigned position() const { return position_; }
    uint16_t pending() const { return pending_; }
    uint8_t byte() const { return byte_; }
    uint8_t ready() const { return ready_; }
    uint8_t elapsed() const { return elapsed_; }
    uint32_t startTick() const { return startTick_; }
    uint32_t currentTick() const { return currentTick_; }
    bool resumeCompare(uint32_t source, uint32_t target) {
        if (!awaitingCompare()) return false;
        source_ = source; target_ = target; stage_ = resume_; return true;
    }
    unsigned nextClocks(unsigned byteClocks = 3) const {
        constexpr std::array<unsigned, 59> costs{{
            20,20,11,20,3,3,9,3,8,9,3,9,9,8,3,8,20,3,8,6,
            15,3,15,3,9,3,9,3,9,3,9,3,3,3,14,6,8,6,3,3,3,
            14,3,24,24,3,3,3,6,3,3,15,3,6,20,11,20,20,17
        }};
        if (unsigned(stage_) >= costs.size()) return 0;
        if (stage_ == Stage::ReadSource) return 5 + byteClocks;
        if (stage_ == Stage::WriteTarget || stage_ == Stage::ReadReady)
            return 3 + byteClocks;
        return costs[unsigned(stage_)];
    }
    template<class Reader, class Writer>
    unsigned advanceBefore(unsigned available, Reader&& read, Writer&& write,
                           unsigned byteClocks = 3) {
        const auto cost = nextClocks(byteClocks);
        if (!cost || cost > available) return 0;
        switch (stage_) {
        case Stage::Offset: offset_ = target_ - 0x800000u; next(); break;
        case Stage::AdmitFlash:
            stage_ = offset_ & 0x80000000u ? Stage::Fail : Stage::CompareProtected; break;
        case Stage::AdmitProtected:
            stage_ = offset_ < 0x4000 ? Stage::SkipSource : Stage::CheckAlignment; break;
        case Stage::SkipSource: source_ += 256; next(); break;
        case Stage::SkipTarget: target_ += 256; next(); break;
        case Stage::FinishProtected: stage_ = Stage::ReleaseClock; break;
        case Stage::AdmitAlignment:
            stage_ = uint8_t(target_) ? Stage::Fail : Stage::ComparePage; break;
        case Stage::ComparePage: await(Stage::CheckDifference); break;
        case Stage::AdmitDifference:
            stage_ = target_ & 0x80000000u ? Stage::ClearDifference : Stage::ReleaseClock; break;
        case Stage::ClearDifference: target_ &= 0x7fffffffu; next(); break;
        case Stage::ReadPending: pending_ = uint16_t(read(Read::Pending, 0)); next(); break;
        case Stage::IncrementPending: ++pending_; next(); break;
        case Stage::WritePending: write(Write::Pending, pending_, 0); next(); break;
        case Stage::WriteFirstCommand: write(Write::FirstCommand, 0xaa, 0); next(); break;
        case Stage::WriteSecondCommand: write(Write::SecondCommand, 0x55, 0); next(); break;
        case Stage::WriteProgramCommand: write(Write::ProgramCommand, 0xa0, 0); next(); break;
        case Stage::BeginWrite: position_ = 0; next(); break;
        case Stage::ReadSource: byte_ = uint8_t(read(Read::Source, position_)); next(); break;
        case Stage::WriteTarget: write(Write::Byte, byte_, position_); next(); break;
        case Stage::NextByte: ++position_; next(); break;
        case Stage::AdmitNext: stage_ = position_ < 256 ? Stage::ReadSource : Stage::LeaveWriteWindow; break;
        case Stage::ReadStartTick: startTick_ = read(Read::StartTick, 0); next(); break;
        case Stage::ReadCurrentTick: currentTick_ = read(Read::CurrentTick, 0); next(); break;
        case Stage::Elapsed: elapsed_ = uint8_t(currentTick_ - startTick_); next(); break;
        case Stage::AdmitTimeout: stage_ = elapsed_ >= 15 ? Stage::Fail : Stage::ReadReady; break;
        case Stage::ReadReady: ready_ = uint8_t(read(Read::Ready, 255)); next(); break;
        case Stage::AdmitReady: stage_ = ready_ == byte_ ? Stage::VerifyPage : Stage::ReadCurrentTick; break;
        case Stage::VerifyPage: await(Stage::FinishVerify); break;
        case Stage::FinishVerify: stage_ = Stage::ReleaseClock; break;
        case Stage::Fail: target_ |= 0x80000000u; next(); break;
        case Stage::Return: stage_ = Stage::Complete; break;
        default: next(); break;
        }
        return cost;
    }
private:
    void next() { stage_ = Stage(unsigned(stage_) + 1); }
    void await(Stage resume) { resume_ = resume; stage_ = Stage::AwaitCompare; }
    Stage stage_ = Stage::AdmitContext, resume_ = Stage::Complete;
    uint32_t source_, target_, offset_ = 0, startTick_ = 0, currentTick_ = 0;
    unsigned position_ = 0;
    uint16_t pending_ = 0;
    uint8_t byte_ = 0, ready_ = 0, elapsed_ = 0;
};
}
