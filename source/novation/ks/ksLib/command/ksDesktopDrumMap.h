#pragma once

#include "ksProgramFlash.h"
#include "ksUserDrumPattern.h"
#include "../../../controller/command/nativeEditorState.h"
#include <memory>
#include <variant>

namespace novation::controller::command {

enum class KSDesktopLoadStatus : uint8_t { Idle, Loading, Applied, Rejected };

struct KSDesktopDrumMap {
    KSDrumBank records{};
    ProgramSelection selection{};
    uint8_t part = 0;
    bool performance = false, complete = false;
};

struct KSDesktopPreset {
    KSPerformanceRecord records{};
    uint8_t part = 0;
    bool wholePerformance = false;
};

struct KSDesktopPartSwitch {
    uint8_t part = 0;
    bool enabled = false;
};

using KSDesktopSound = std::variant<KSDesktopDrumMap, KSDesktopPreset, KSDesktopPartSwitch>;

struct KSDesktopPatchExtras {
    KSUserDrumPatterns sequences;
    std::array<std::optional<KSDrumBank>,4> drumMaps;
    std::atomic<KSDesktopLoadStatus> status{KSDesktopLoadStatus::Loading};
    uint8_t appliedSlots = 0;
};

struct KSDesktopSoundLoad {
    KSDesktopSound sound;
    // Allocated and retired by the control producer. The audio owner only
    // reads the data and publishes the result while the slot is Loading.
    std::shared_ptr<KSDesktopPatchExtras> extras;
    std::atomic<KSDesktopLoadStatus> status{KSDesktopLoadStatus::Idle};
    uint64_t appliedRevision = 0; // Published by the Applied status for part switches.
};

struct KSDesktopDrums {
    std::array<EditorMailbox<KSDrumBank>,4> banks;
    // Stable storage shared by the control producer and the DSP owner. A
    // request slot is reusable only after the controller finishes with it.
    std::array<KSDesktopSoundLoad,8> loads;
};

}
