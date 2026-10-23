#pragma once
#include "ksFlashPageTask.h"
#include <array>
#include <optional>

namespace ks {
class ControllerCpu;
class FlashPageCalls final {
public:
    static bool candidate(unsigned pc) { return pc >= 0x11ae && pc < 0x12b0; }
    bool step(ControllerCpu&);
    unsigned activeCalls() const {
        unsigned count = 0;
        for (const auto& c : compares_) if (c) ++count;
        for (const auto& c : programs_) if (c) ++count;
        return count;
    }
    void resetMetrics() { initialActive = activeCalls(); started = completed = partialEntries = 0; }
    void discardCalls() {
        for (auto& c : compares_) c.reset();
        for (auto& c : programs_) c.reset();
    }
    uint64_t started = 0, completed = 0, partialEntries = 0, initialActive = 0;
private:
    template<class Task> struct Call {
        Task task;
        uint32_t token;
        Call(uint32_t source, uint32_t target, uint32_t stack)
            : task(source, target), token(stack) {}
    };
    bool compare(ControllerCpu&);
    bool program(ControllerCpu&);
    std::array<std::optional<Call<FlashPageCompareTask>>, 16> compares_{};
    std::array<std::optional<Call<FlashPageProgramTask>>, 16> programs_{};
};
using FlashPagePartialTest = bool(*)(ControllerCpu&);
void setFlashPagePartialForTests(FlashPagePartialTest);
}
