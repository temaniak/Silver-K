#include "ksCommandRefresh.h"
#include "ksCommandFeedback.h"

namespace novation::controller::command
{
    std::optional<KSRefreshPacket> encodeKsPartUpdate(
        KSStorageSlot part, const KSVoiceImagePatch& patch, const KSGlobalMaster& master,
        const KSGlobalTables& globals, const KSVoiceImageTables& voiceTables,
        const KSVoiceRefreshInput* voices, size_t voiceCount, std::optional<VoiceSlot> tracked) noexcept
    {
        const auto logical = KSMusicalState::logicalPart(part);
        if (!logical || (voiceCount && !voices) || voiceCount > KSMusicalState::VoiceCount ||
            (tracked && size_t(*tracked) >= KSMusicalState::VoiceCount))
            return std::nullopt;
        KSRefreshPacket packet;
        std::optional<KSVoiceTrackingWords> tracking;
        for (size_t i = 0; i < voiceCount; ++i)
        {
            const auto& voice = voices[i];
            const auto slot = voice.cutoff.observation.allocation.slot;
            if (voice.cutoff.observation.part.part != part ||
                (i && slot <= voices[i - 1].cutoff.observation.allocation.slot))
                return std::nullopt;
            const auto encoded=encodeKsParameterImage(patch,voiceTables,
                {voice.context,voice.unisonLayer,voice.unisonSpread,voice.glideDistance,voice.allocation},voice.cutoff);
            const auto* image=encoded?&encoded->image:nullptr;
            const auto address = uint16_t(KSVoiceImageResult::ImageAddress + size_t(slot) *
                                          KSVoiceImageResult::PrimaryVoiceStride);
            if (!image || !packet.append(Endpoint::B, address, image->image.data(), image->image.size()))
                return std::nullopt;
            tracking = image->tracking;
        }
        if (tracked)
        {
            if (!tracking)
                tracking = encodeKsVoiceTracking(patch, voiceTables);
            if (!tracking)
                return std::nullopt;
            for (size_t index = 0; index < tracking->size(); ++index)
                if (!packet.appendWord(Endpoint::B, uint16_t(KSVoiceImageResult::TrackingAddresses[index] +
                                           size_t(*tracked) * KSVoiceImageResult::PrimaryVoiceStride), (*tracking)[index]))
                    return std::nullopt;
        }
        const auto performancePart = part == KSStorageSlot::Single ? std::nullopt : logical;
        if (!appendKsGlobals(packet, KSGlobalPart::fromPatch(patch), master, globals, performancePart))
            return std::nullopt;
        return packet;
    }

    std::optional<KSRefreshPacket> encodeKsSingleFeedbackRebuild(
        const KSVoiceImagePatch& patch, const KSGlobalMaster& master,
        const KSGlobalTables& tables) noexcept
    {
        KSRefreshPacket packet;
        if (!packet.appendWord(Endpoint::A, KSCommandFeedback::partStatusAddress(KSPart::One), 0) ||
            !appendKsGlobals(packet, KSGlobalPart::fromPatch(patch), master, tables))
            return std::nullopt;
        return packet;
    }
}
