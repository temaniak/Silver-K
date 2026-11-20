#pragma once

#include "controllerStateView.h"

#include <atomic>

namespace novation::controller::command {

struct NativeEditorPart {
    ProgramSelection program{};
    PartRevisions revisions{};
    uint8_t channel = 0, cutoff = 0, volume = 0;
    std::array<char,17> name{};
    std::array<uint8_t,256> parameters{};
    uint16_t parameterBytes = 0;
};

struct NativeEditorState {
    Session session{};
    NativeFrame frame{};
    uint64_t revision = 0, rateGeneration = 0;
    Model model = Model::K;
    bool performance = false, loadingProgram = false;
    uint8_t performanceProgram = 0, globalChannel = 0, activeMask = 0, selectedPart = RoutedByChannel;
    std::array<char,17> performanceName{};
    std::array<NativeEditorPart,4> parts{};
};

inline std::array<char,17> editorProgramName(const uint8_t* first) noexcept {
    std::array<char,17> name{};
    for (size_t i=0;i<16;++i)
        name[i] = first[i]>=32 && first[i]<127 ? char(first[i]) : ' ';
    return name;
}

// One controller writer and one editor reader. The writer and reader own
// separate slots; the atomic middle slot can drop intermediate publications.
// A closed editor need not poll. The first read after reopening takes the
// latest complete value. The worker can publish an empty value on retirement.
class NativeEditorMailbox final {
public:
    void publish(const std::optional<NativeEditorState>& value) noexcept {
        slots_[write_] = value;
        write_ = middle_.exchange(write_ | Dirty, std::memory_order_acq_rel) & Index;
    }

    std::optional<NativeEditorState> read() noexcept {
        if (middle_.load(std::memory_order_acquire) & Dirty)
            read_ = middle_.exchange(read_, std::memory_order_acq_rel) & Index;
        return slots_[read_];
    }

private:
    static constexpr unsigned Dirty = 4, Index = 3;
    std::array<std::optional<NativeEditorState>,3> slots_{};
    std::atomic<unsigned> middle_{1};
    unsigned read_ = 0, write_ = 2;
};

}
