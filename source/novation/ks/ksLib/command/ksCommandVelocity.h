#pragma once

#include "ksCommandProgram.h"

namespace novation::controller::command
{
    // Cold immutable data, never a firmware-memory binding. All byte-valued
    // serialized curves are defined; ordinary MIDI edits are seven-bit.
    struct KSNoteVelocityTables
    {
        static constexpr size_t ThresholdCount = 252;
        std::array<uint8_t, ThresholdCount> thresholds{};
    };

    inline std::optional<KSNoteVelocityTables> decodeKsNoteVelocityTables(
        const uint8_t* authenticatedImage, size_t size) noexcept
    {
        constexpr size_t address = 0x1534e;
        if (!authenticatedImage || size < address + KSNoteVelocityTables::ThresholdCount)
            return std::nullopt;
        KSNoteVelocityTables tables{};
        std::copy_n(authenticatedImage + address, tables.thresholds.size(), tables.thresholds.begin());
        return tables;
    }

    struct KSNoteVelocityResult
    {
        // Shaped amplitude remains a seven-bit value even when the start is
        // suppressed. Zero is a valid quantized result, not a Note Off.
        uint8_t value = 0;
        bool suppressed = false;
    };

    // Resolve one complete note's velocity. Split thresholds compare the raw
    // input, independently of power shaping. A suppressed start still follows
    // key ownership, patch selection and voice allocation/population; only the
    // later tracking/image/gate start is omitted by the caller.
    inline std::optional<KSNoteVelocityResult> ksNoteVelocity(uint8_t input,
        uint8_t curve, uint8_t globalPower, const KSNoteVelocityTables& tables) noexcept
    {
        const auto shaped = ksNoteVelocity(input, curve < 4 ? curve : uint8_t(0), globalPower);
        if (!shaped)
            return std::nullopt;
        if (curve < 4)
            return KSNoteVelocityResult{*shaped, false};
        const auto threshold = tables.thresholds[size_t(curve) - 4];
        const bool suppressed = (threshold & 1) ? input < threshold : input > threshold;
        return KSNoteVelocityResult{*shaped, suppressed};
    }
}
