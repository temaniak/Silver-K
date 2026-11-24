#pragma once

#include "ksCommandGlobals.h"
#include "ksCommandVoiceImage.h"
#include "ksCommandParameterImage.h"
#include "ksRefreshPacket.h"

namespace novation::controller::command
{
    struct KSVoiceRefreshInput
    {
        KSVoiceImageContext context;
        KSCutoffWord cutoff;
        uint8_t unisonLayer=0,unisonSpread=0,glideDistance=0,allocation=0;
    };

    // This is one admitted part bundle. The periodic A:8056 signal and
    // B:8045 panel/auxiliary operation have separate admission boundaries.
    std::optional<KSRefreshPacket> encodeKsPartUpdate(
        KSStorageSlot part, const KSVoiceImagePatch& patch, const KSGlobalMaster& master,
        const KSGlobalTables& globals, const KSVoiceImageTables& voiceTables,
        const KSVoiceRefreshInput* voices, size_t voiceCount, std::optional<VoiceSlot> tracked) noexcept;

    std::optional<KSRefreshPacket> encodeKsSingleFeedbackRebuild(
        const KSVoiceImagePatch& patch, const KSGlobalMaster& master,
        const KSGlobalTables& tables) noexcept;
}
