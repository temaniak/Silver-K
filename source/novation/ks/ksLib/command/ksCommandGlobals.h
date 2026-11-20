#pragma once

#include "ksMusicalState.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace novation::controller::command
{
    class KSRefreshPacket;

    struct KSEffectTime
    {
        uint8_t time;
        uint8_t sync;
    };

    struct KSGlobalPart
    {
        std::array<uint8_t, 3> centeredControls;
        uint8_t externalFlags;
        uint8_t distortionLevel;
        uint8_t distortionWheel;
        uint8_t distortionCompensation;
        KSEffectTime eq;
        uint8_t eqDepth;
        uint8_t eqFrequency;
        uint8_t eqShape;
        KSEffectTime chorus;
        uint8_t chorusMode;
        uint8_t chorusDepth;
        uint8_t chorusCentre;
        uint8_t chorusFeedback;
        uint8_t chorusSend;
        uint8_t chorusWheel;
        uint8_t reverbSend;
        uint8_t reverbWheel;
        uint8_t reverbDecay;
        uint8_t reverbType;
        uint8_t partLevel;
        uint8_t partOutput;
        KSEffectTime delay;
        uint8_t delayRatio;
        uint8_t delaySend;
        uint8_t delayWheel;
        uint8_t delayFeedback;
        uint8_t delayStereoWidth;
        KSEffectTime pan;
        uint8_t panDepth;
        uint8_t panPosition;
        uint8_t triggerToggle;
        uint8_t triggerTime;
        uint8_t triggerAmount;

        // Project the expanded live patch, after the derived fields have been published.
        static KSGlobalPart fromPatch(const std::array<uint8_t, 256>& patch) noexcept;
    };

    struct KSGlobalMaster
    {
        uint8_t vocoderBalance;
        uint8_t vocoderWidth;
        uint8_t sibilanceLevel;
        uint8_t sibilanceType;
        uint8_t previousEffectStatus;
        uint8_t performanceEffectStatus;

        static KSGlobalMaster fromPatch(const std::array<uint8_t, 256>& patch,
                                        uint8_t previousEffectStatus) noexcept;
    };

    struct KSReverbMatrixRow
    {
        uint8_t bias;
        uint8_t depth;
        uint16_t seed;
        uint16_t factor;
        uint16_t recurrenceBias;
        std::array<uint16_t, 6> tail;
    };

    struct KSDelayRatio
    {
        uint16_t left;
        uint16_t right;
    };

    struct KSGlobalTables
    {
        uint16_t timingRate;
        uint16_t shapeBase;
        std::array<KSReverbMatrixRow, 6> reverb;
        std::array<KSDelayRatio, 13> delayRatios;
        std::array<uint32_t, 34> reciprocalTimes;
        std::array<uint32_t, 34> delayTimes;
    };

    // Decode the resource section of the authenticated, unpacked KS 2.1.02 OS
    // and derive the timing tables initialized from it at cold boot.
    std::optional<KSGlobalTables> decodeKsGlobalTables(const uint8_t* decodedOs, size_t size) noexcept;

    // The panel reload occurs before deferred part selection and can also run
    // when cooldown skips the part update.
    bool appendKsAuxiliary(KSRefreshPacket& packet, uint8_t liveAuxiliary) noexcept;

    // Prime each word explicitly. Only Single emits the A/B trigger tail.
    bool appendKsGlobals(KSRefreshPacket& packet, const KSGlobalPart& part, const KSGlobalMaster& master,
                         const KSGlobalTables& tables, std::optional<KSPart> performancePart = std::nullopt) noexcept;
}
