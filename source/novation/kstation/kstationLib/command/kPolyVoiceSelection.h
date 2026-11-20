#pragma once

#include "../../../controller/command/controllerStateView.h"
#include <algorithm>

namespace novation::controller::command
{
    struct KPolyVoiceState
    {
        uint16_t age = 0;
        uint16_t countdown = 0;
    };

    enum class KPolyVoiceSelectionReason : uint8_t { Free, Oldest, SameNote };

    struct KPolyVoiceSelection
    {
        VoiceSlot slot{};
        KPolyVoiceSelectionReason reason = KPolyVoiceSelectionReason::Free;
    };

    // Only K's Poly2 selection decision. The full eligible slot range is
    // sampled by the caller; no state is changed here. A zero high age wins
    // immediately regardless of its retained low countdown. With every high
    // age nonzero, compare the complete unsigned32 state and keep the first
    // equal minimum. No owner, note, unit or flag masking belongs to Poly2.
    // Allocation/generation validation, activation, DSP writes and NoteOn
    // lifecycle are separate operations; this helper does not implement them.
    constexpr KPolyVoiceSelection selectKPolyVoice(
        const std::array<KPolyVoiceState, 8>& voices, size_t count=8,uint8_t excluded=0) noexcept
    {
        size_t selected = voices.size();
        uint32_t best = std::numeric_limits<uint32_t>::max();
        for (size_t i = 0; i < std::min(count,voices.size()); ++i)
        {
            if(excluded & (1u<<i))continue;
            if (!voices[i].age) return {VoiceSlot(i), KPolyVoiceSelectionReason::Free};
            const auto state = (uint32_t(voices[i].age) << 16) | voices[i].countdown;
            if (state < best || selected==voices.size()) { best = state; selected = i; }
        }
        return {VoiceSlot(selected), KPolyVoiceSelectionReason::Oldest};
    }

}
