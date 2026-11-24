#pragma once

#include "ksCommandCutoff.h"

namespace novation::controller::command
{
    // OS 2.1.02 current Single program record, including its unpacked editing
    // mirrors. The cutoff observation supplies the field committed by CC105.
    using KSVoiceImagePatch = std::array<uint8_t, 256>;

    struct KSVoiceImageTables
    {
        uint16_t pitch;
        uint16_t filter;
        uint16_t attack;
        uint16_t decay;
        uint16_t lfoRate;
        uint16_t lfoDelay;
        uint16_t glide;
        std::array<uint16_t, 65> modulationCurve;
        std::array<uint32_t, 34> reciprocalTiming;
    };

    // Read numerical bases and curves from an authenticated decoded KS 2.1.02
    // image. Build the reciprocal table from its divisors and rate word.
    std::optional<KSVoiceImageTables> decodeKsVoiceImageTables(const uint8_t* image, size_t size) noexcept;

    struct KSVoiceImageContext
    {
        uint8_t globalFine;
        uint8_t wheelFine;
        uint16_t wheelPitch;
        std::array<uint8_t, 3> modulation;
        std::array<uint8_t, 3> oscillatorAmplitude;
        uint8_t transposedNote;
        uint8_t previousNote;
        bool panelGate;
    };

    using KSVoiceTrackingWords = std::array<uint32_t, 4>;

    struct KSVoiceImageResult
    {
        static constexpr uint16_t ImageAddress = 0x002c;
        static constexpr uint16_t PrimaryVoiceStride = 101;
        static constexpr std::array<uint16_t, 4> TrackingAddresses{0x007e, 0x007f, 0x0086, 0x0087};
        std::array<uint32_t, 100> image;
        KSVoiceTrackingWords tracking;
    };

    // Admitted voice on a selected part, with neutral detune,
    // disabled unison and zero glide time. The caller resolves live allocation
    // and tracking eligibility before selecting this encoder.
    std::optional<KSVoiceImageResult> encodeKsVoiceImage(const KSVoiceImagePatch& patch,
        const KSVoiceImageTables& tables, const KSVoiceImageContext& voice,
        const KSCutoffWord& filter) noexcept;

    // The current Single program supplies both rate pairs for the tracked
    // slot even if the former voice has aged out.
    std::optional<KSVoiceTrackingWords> encodeKsVoiceTracking(const KSVoiceImagePatch& patch,
        const KSVoiceImageTables& tables) noexcept;
}
