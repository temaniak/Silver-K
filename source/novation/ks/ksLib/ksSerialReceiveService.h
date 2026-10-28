#pragma once
#include "ksSerialIrqTask.h"
#include "ksClockPulseTask.h"
#include "ksTimingTablesTask.h"
#include <optional>

namespace ks {
// SCI receive owns the F8 feedback graph. Every child returns a logical
// result; resource callbacks observe the original phase-relative clocks.
class SerialReceiveService final {
public:
    enum class Component { Receive, Latched, Edge, Counter };
    bool complete() const { return receive_.stage() == SerialIrqTask::Stage::Complete; }
    uint32_t components() const { return components_; }
    Component component() const { return active_; }
    bool at(Component component) const { return active_ == component; }
    unsigned nextClocks() const {
        switch (active_) {
        case Component::Receive: return receive_.nextClocks([](SerialIrqTask::Clock what) {
            return what == SerialIrqTask::Clock::Return ? 23u :
                   what == SerialIrqTask::Clock::PortWrite ? 1u : 3u;
        });
        case Component::Latched: return latched_->nextClocks();
        case Component::Edge: return edge_->nextClocks();
        case Component::Counter: return counter_->nextClocks();
        }
        return 0;
    }
    template<class Reader, class Writer>
    unsigned advanceBefore(unsigned available, Reader&& read, Writer&& write) {
        const auto cost = nextClocks();
        if (!cost || cost > available) return 0;
        return advancePlannedPhase(cost,read,write);
    }
private:
    friend class InterruptService;
    // The owner synchronously passes this phase's admitted nonzero cost.
    template<class Reader,class Writer>
    unsigned advancePlannedPhase(unsigned cost,Reader&& read,Writer&& write){
        const auto readWord = [&](uint32_t address, unsigned end) {
            const unsigned gap = address >= 0xfffd10 ? 1 : 3;
            const auto high = uint8_t(read(address, end - gap));
            return uint16_t((unsigned(high) << 8) | uint8_t(read(address + 1, end)));
        };
        const auto writeWord = [&](uint32_t address, uint16_t value, unsigned end) {
            write(address, uint8_t(value >> 8), end - 3);
            write(address + 1, uint8_t(value), end);
        };
        const auto readLong = [&](uint32_t address, unsigned end) {
            uint32_t value = 0;
            for (unsigned n = 0; n < 4; ++n)
                value = (value << 8) | uint8_t(read(address + n, end - 9 + 3 * n));
            return value;
        };
        const auto writeLong = [&](uint32_t address, uint32_t value, unsigned end) {
            for (unsigned n = 0; n < 4; ++n)
                write(address + n, uint8_t(value >> (24 - 8 * n)), end - 9 + 3 * n);
        };
        if (active_ == Component::Receive) {
            using T = SerialIrqTask;
            receive_.advanceBefore(cost, [&](T::Read field) -> uint16_t {
                switch (field) {
                case T::Read::Status: return read(0xffff8c, cost);
                case T::Read::Receive: return read(0xffff8d, cost);
                case T::Read::Producer: return readWord(0xffec64, cost);
                case T::Read::Consumer: return readWord(0xffec66, cost);
                case T::Read::Data: return read(receive_.dataAddress(), cost);
                }
                return 0;
            }, [&](T::Write field, uint16_t value) {
                if (field == T::Write::Producer) writeWord(0xffec64, value, cost);
                else write(field == T::Write::Status ? 0xffff8c :
                           field == T::Write::Error ? 0xffed68 : receive_.dataAddress(),
                           uint8_t(value), cost);
            }, [](T::Clock what) {
                return what == T::Clock::Return ? 23u : what == T::Clock::PortWrite ? 1u : 3u;
            });
            if (receive_.awaiting()) {
                latched_.emplace(ClockPulseTask::Kind::Latched, 0, 0);
                select(Component::Latched);
            }
        } else if (active_ == Component::Counter) {
            counter_->advanceBefore(cost, [&](TimingTablesTask::Read) {
                return readWord(0xffffe6, cost);
            }, [](unsigned, uint32_t) {});
            if (counter_->stage() == TimingTablesTask::Stage::Complete) {
                const auto tick = counter_->counter();
                counter_.reset();
                edge_->resumeChild(tick);
                select(Component::Edge);
            }
        } else {
            using T = ClockPulseTask;
            const bool edge = active_ == Component::Edge;
            auto& task = edge ? *edge_ : *latched_;
            const auto stage = task.stage();
            task.advanceBefore(cost, [&](T::Read field) -> uint32_t {
                switch (field) {
                case T::Read::Latched: return read(0xffee9a, cost);
                case T::Read::Flags: return read(0xffee97, stage == T::Stage::WriteSourceFlag ? cost - 3 : cost);
                case T::Read::Active: return read(0xffee96, cost);
                case T::Read::Pulses: return readWord(0xffee78, cost);
                case T::Read::PulseCount: return readWord(0xffeea8, cost);
                case T::Read::Timer: return readLong(0xffee78, cost);
                case T::Read::Sum: return readLong(0xffeeaa, cost);
                case T::Read::Samples: return readWord(0xffee9c, cost);
                case T::Read::PreviousTick: return readWord(0xffee9e, cost);
                case T::Read::Estimate: return readLong(0xffeea0, cost);
                case T::Read::Gain: return readWord(0xffeea4, cost);
                case T::Read::Integral: return readWord(0xffeea6, cost);
                }
                return 0;
            }, [&](T::Write field, uint32_t value) {
                switch (field) {
                case T::Write::Latched: write(0xffee9a, uint8_t(value), cost); break;
                case T::Write::Flags: write(0xffee97, uint8_t(value), cost); break;
                case T::Write::PreviousTick: writeWord(0xffee9e, uint16_t(value), cost); break;
                case T::Write::Samples: writeWord(0xffee9c, uint16_t(value), cost); break;
                case T::Write::Sum: writeLong(0xffeeaa, value, cost); break;
                case T::Write::PulseCount: writeWord(0xffeea8, uint16_t(value), cost); break;
                case T::Write::SumHigh: writeWord(0xffeeaa, uint16_t(value), cost); break;
                case T::Write::SumLow: writeWord(0xffeeac, uint16_t(value), cost); break;
                case T::Write::Estimate: writeLong(0xffeea0, value, cost); break;
                case T::Write::Period: writeLong(0xffee7e, value, cost); break;
                case T::Write::Integral: writeWord(0xffeea6, uint16_t(value), cost); break;
                case T::Write::Gain: writeWord(0xffeea4, uint16_t(value), cost); break;
                }
            });
            if (task.awaitingChild()) {
                if (edge) {
                    counter_.emplace(TimingTablesTask::Kind::Counter);
                    select(Component::Counter);
                } else {
                    edge_.emplace(T::Kind::Edge, 0, task.source());
                    select(Component::Edge);
                }
            } else if (task.complete()) {
                if (edge) {
                    edge_.reset();
                    latched_->resumeChild(0);
                    select(Component::Latched);
                } else {
                    latched_.reset();
                    receive_.resumeChild();
                    select(Component::Receive);
                }
            }
        }
        return cost;
    }
    void select(Component component) { active_ = component; components_ |= 1u << unsigned(component); }
    SerialIrqTask receive_{SerialIrqTask::Kind::Receive};
    std::optional<ClockPulseTask> latched_, edge_;
    std::optional<TimingTablesTask> counter_;
    Component active_ = Component::Receive;
    uint32_t components_ = 1;
};
}
