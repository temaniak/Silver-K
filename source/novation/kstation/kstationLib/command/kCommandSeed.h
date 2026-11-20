#pragma once

#include "kPolyNoteImage.h"

namespace kstation::command {

struct KCommandVoiceSeed {
    uint16_t age = 0, countdown = 0;
    uint8_t note = 0, velocity = 0, part = 0, deferred = 0;
};

struct KCommandSeed {
    std::array<uint8_t,256> patch{};
    std::array<int16_t,65> modulationCurve{};
    std::array<uint8_t,3> controls{};
    std::array<KCommandVoiceSeed,8> voices{};
    uint8_t channel = 0, sustain = 0, inhibited = 0, heldNote = 60;
    uint64_t cycle = 0;
    novation::controller::command::KVoiceTunings noteTunings{};
};
}
