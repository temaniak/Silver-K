#pragma once

#include "ksCommandVoiceImage.h"
#include <algorithm>

namespace novation::controller::command
{
    // Single bank records have 209 loaded bytes. The remaining editing tail
    // is retained, except for the 19 unpacked mirrors of six packed bytes.
    // Bank-specific overlays must be resolved by the caller before this step.
    inline KSVoiceImagePatch decodeKsSingleProgram(const KSVoiceImagePatch& previous,
                                                   const std::array<uint8_t, 209>& record) noexcept
    {
        auto patch = previous;
        std::copy(record.begin(), record.end(), patch.begin());
        patch[209] = (patch[0] >> 3) & 3;
        patch[210] = patch[0] & 7;
        patch[211] = (patch[0] >> 5) & 3;
        patch[212] = (patch[88] >> 3) & 1;
        patch[213] = (patch[88] >> 4) & 1;
        patch[214] = patch[88] & 3;
        patch[215] = (patch[88] >> 2) & 1;
        patch[216] = (patch[88] >> 5) & 3;
        patch[217] = patch[121] & 7;
        patch[218] = (patch[121] >> 3) & 3;
        patch[219] = patch[122] & 3;
        patch[220] = (patch[122] >> 2) & 3;
        patch[221] = (patch[122] >> 4) & 3;
        patch[222] = patch[127] & 3;
        patch[223] = (patch[127] >> 2) & 3;
        patch[224] = (patch[127] >> 4) & 3;
        patch[225] = (patch[127] >> 6) & 1;
        patch[226] = patch[126] & 7;
        const auto nibble = (patch[126] >> 3) & 15;
        patch[227] = uint8_t(nibble < 8 ? nibble : int(nibble) - 16);
        return patch;
    }

    // P05 supports the four arithmetic curves. Split/threshold curves need
    // their own typed threshold source and are deliberately rejected here.
    inline std::optional<uint8_t> ksNoteVelocity(uint8_t input, uint8_t curve, uint8_t globalPower) noexcept
    {
        if (!input || input >= 128 || curve >= 4)
            return std::nullopt;
        const uint8_t biased = uint8_t(globalPower + 2);
        const unsigned exponent = biased ? biased : 256;
        auto value = input;
        for (unsigned n = 1; n < exponent; ++n)
            value = uint8_t(uint16_t(input) * value / 128);
        const auto inverse = uint8_t(value ^ 127);
        if (curve == 1) value = inverse;
        if (curve == 2) value = uint8_t(uint16_t(value) * value / 128);
        if (curve == 3) value = uint8_t(uint16_t(value) * inverse / 128);
        return value;
    }
}
