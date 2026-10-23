#pragma once
#include "ksSerialIrqTask.h"
#include "ksTpu0Service.h"
#include <cstdint>

namespace ks {
// Closed interrupt lifecycles. State consists only of peripheral data and
// typed task continuations: no register file, condition flags, return address or stack.
// The clock credits preserve the original preemption/observation cadence even
// when the old register saves and instruction fetches no longer take place.
class InterruptService final {
public:
    enum class Kind { SerialError, SerialTransmit, Unused, Tpu0 };
    explicit InterruptService(Kind kind)
        : kind_(kind), serial_(kind == Kind::SerialError
              ? SerialIrqTask::Kind::Error : SerialIrqTask::Kind::Transmit) {if(kind==Kind::Tpu0)timer_.emplace();}
    Kind kind() const { return kind_; }
    bool complete() const { return complete_; }
    bool critical() const { return timer_&&timer_->critical(); }
    uint32_t timerComponents() const { return timer_?timer_->components():0; }
    bool timerAt(Tpu0Service::Component component) const {
        return timer_&&!complete_&&timer_->component()==component;
    }
    unsigned nextClocks() const {
        if (complete_) return 0;
        if (timer_) return timer_->nextClocks();
        if (kind_ == Kind::Unused) return 23;
        return serial_.nextClocks([&](SerialIrqTask::Clock clock) {
            return clockCost(clock);
        });
    }
    // Callbacks observe byte transfers at an offset from this phase's start.
    // Each phase is indivisible; an insufficient budget has no side effects.
    template<class Reader, class Writer>
    unsigned advanceBefore(unsigned available, Reader&& read, Writer&& write) {
        const auto cost = nextClocks();
        if (!cost || cost > available) return 0;
        if (timer_) {const auto used=timer_->advanceBefore(available,read,write);complete_=timer_->complete();return used;}
        if (kind_ == Kind::Unused) { complete_ = true; return cost; }
        const auto readWord = [&](uint32_t address) {
            const auto high = uint8_t(read(address, 12));
            return uint16_t((unsigned(high) << 8) | uint8_t(read(address + 1, 15)));
        };
        const auto used = serial_.advanceBefore(cost,
            [&](SerialIrqTask::Read field) -> uint32_t {
                switch (field) {
                case SerialIrqTask::Read::Status: return read(0xffff8c, 4);
                case SerialIrqTask::Read::Producer: return readWord(0xffed6c);
                case SerialIrqTask::Read::Consumer: return readWord(0xffed6e);
                case SerialIrqTask::Read::Data: return read(serial_.dataAddress(), cost);
                default: return 0; // Receive is a separate, still-migrating lifecycle.
                }
            },
            [&](SerialIrqTask::Write field, uint16_t value) {
                switch (field) {
                case SerialIrqTask::Write::Status: write(0xffff8c, uint8_t(value), cost); break;
                case SerialIrqTask::Write::Error: write(0xffed68, uint8_t(value), cost); break;
                case SerialIrqTask::Write::Control: write(0xffff8a, uint8_t(value), cost); break;
                case SerialIrqTask::Write::Consumer:
                    write(0xffed6e, uint8_t(value >> 8), 12);
                    write(0xffed6f, uint8_t(value), 15); break;
                case SerialIrqTask::Write::Transmit: write(0xffff8b, uint8_t(value), cost); break;
                default: break;
                }
            }, [&](SerialIrqTask::Clock clock) { return clockCost(clock); });
        complete_ = serial_.stage() == SerialIrqTask::Stage::Complete;
        return used;
    }
private:
    unsigned clockCost(SerialIrqTask::Clock clock) const {
        switch (clock) {
        case SerialIrqTask::Clock::Return: return 23;
        case SerialIrqTask::Clock::DataRead:
            return serial_.dataAddress() >= 0xfffd10 ? 1 : 3;
        case SerialIrqTask::Clock::PortWrite: return 1;
        default: return 3;
        }
    }
    Kind kind_;
    SerialIrqTask serial_;
    std::optional<Tpu0Service> timer_;
    bool complete_ = false;
};
}
