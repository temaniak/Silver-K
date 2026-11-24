#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace novation::controller {

inline constexpr std::array<unsigned, 4> AudioBufferFloors{0, 512, 1024, 2048};
inline constexpr size_t AudioBlockMargin = 128;

// K's worker queues DSP-rate frames. KS resamples before its host-rate queue.
inline constexpr size_t nativeOutputLead(size_t nativeBlock) noexcept {
    return std::max(size_t{256}, (nativeBlock + AudioBlockMargin + 127) & ~size_t{127});
}

inline constexpr unsigned hostOutputLead(unsigned hostBlock) noexcept {
    return std::min(4096u, std::max(256u, std::min(hostBlock,4096u) + unsigned(AudioBlockMargin)));
}

inline constexpr size_t outputBufferTarget(size_t base, unsigned floor) noexcept {
    return std::max(base, size_t(std::min(4096u, floor)));
}

// Called off the audio thread, with the previous endpoint already detached.
// The callback sees either silence or a restored, primed engine. After
// publication normal playback can drain the queue without reapplying the gate.
template<class Engine>
bool publishPrimedAudio(std::atomic<Engine*>& output, Engine& engine, bool restored,
                        size_t queued, size_t target) noexcept {
    if (!restored || queued < target)
        return false;
    output.store(&engine);
    return true;
}

}
