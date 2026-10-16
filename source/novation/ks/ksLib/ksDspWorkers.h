#pragma once

#include <array>
#include <cstdint>
#include <memory>

namespace ks {
// Two DSP executors, joined at every slice before H8/host/serial/snapshots.
// macOS runs DSP A on the coordinator and DSP B on one persistent helper;
// other platforms retain two persistent helpers.
class DspWorkers final {
public:
    using Execute = void(*)(void*, unsigned, int64_t, bool);
    DspWorkers(void* context, Execute execute, int rate, unsigned block);
    ~DspWorkers();
    DspWorkers(const DspWorkers&) = delete;
    DspWorkers& operator=(const DspWorkers&) = delete;
    void configure(int rate, unsigned block) noexcept;
    void run(const std::array<int64_t,2>& targets, bool sampled);
private:
    struct Worker;
    std::array<std::unique_ptr<Worker>,2> workers;
    int rate;
    unsigned block;
};
}
