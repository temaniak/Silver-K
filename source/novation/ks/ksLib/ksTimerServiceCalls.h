#pragma once
#include "ksTimerServiceTask.h"
#include <array>
#include <optional>

namespace ks {
class ControllerCpu;
class TimerServiceCalls final {
public:
    static bool candidate(unsigned p) { return (p>=0x12b0&&p<0x1306)||(p>=0x27be&&p<0x2848); }
    bool step(ControllerCpu&);
    unsigned activeCalls() const {
        unsigned n=0;
        for(const auto& c:interrupts_) if(c) ++n;
        for(const auto& c:reads_) if(c) ++n;
        for(const auto& c:ticks_) if(c) ++n;
        return n;
    }
    void resetMetrics() { initialActive=activeCalls();started=completed=partialEntries=0; }
    void discardCalls() { for(auto& c:interrupts_)c.reset();for(auto& c:reads_)c.reset();for(auto& c:ticks_)c.reset(); }
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    template<class Task> struct Call { Task task;uint32_t token;explicit Call(uint32_t value):token(value){} };
    bool interrupt(ControllerCpu&);
    bool read(ControllerCpu&);
    bool tick(ControllerCpu&);
    std::array<std::optional<Call<TimerInterruptTask>>,16> interrupts_{};
    std::array<std::optional<Call<ControllerClockReadTask>>,16> reads_{};
    std::array<std::optional<Call<ClockAccumulatorTask>>,16> ticks_{};
};
using TimerServicePartialTest=bool(*)(ControllerCpu&);
void setTimerServicePartialForTests(TimerServicePartialTest);
}
