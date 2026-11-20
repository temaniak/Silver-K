#pragma once

#include "ksMusicalState.h"

namespace novation::controller::command
{
    enum class KSVoiceMode : uint8_t { Mono, MonoGlide, Poly, PolySameNote };
    enum class KSNoteAllocation : uint8_t { PolyStart, MonoStart, ResumeRecent, ResumeEstablished, Reuse };

    struct KSVoiceOwner
    {
        KSStorageSlot part = KSStorageSlot::Single;
        // A musical assignment group, e.g. program revision and unison layer.
        // Equality is significant; no firmware-byte wrap is performed here.
        uint64_t group = 0;

        friend constexpr bool operator==(KSVoiceOwner a, KSVoiceOwner b) noexcept
        {
            return a.part == b.part && a.group == b.group;
        }
    };

    struct KSVoiceObservation
    {
        KSVoiceOwner owner;
        uint8_t note = 0;
        uint16_t age = 0, countdown = 0;
    };

    struct KSVoiceSelectionInput
    {
        KSVoiceMode mode = KSVoiceMode::Poly;
        KSVoiceOwner owner;
        uint8_t note = 0;
        bool newMonoVoice = false;
        uint8_t count = KSMusicalState::VoiceCount;
        std::array<KSVoiceObservation, KSMusicalState::VoiceCount> voices{};
    };

    struct KSVoiceSelection
    {
        VoiceSlot slot;
        KSNoteAllocation allocation;
        bool stopBeforeReuse;
    };

    // One complete bounded selection over explicit musical/feedback inputs.
    // Age/countdown remain separate fields; no RAM, execution cursor, timer
    // decay, instruction costs or per-candidate scheduling are represented.
    inline std::optional<KSVoiceSelection> selectKsVoice(const KSVoiceSelectionInput& input) noexcept
    {
        if (!input.count || input.count > input.voices.size() || input.note >= 128 ||
            size_t(input.mode) > size_t(KSVoiceMode::PolySameNote) ||
            size_t(input.owner.part) >= KSMusicalState::StorageSlotCount)
            return std::nullopt;
        for (size_t i = 0; i < input.count; ++i)
            if (input.voices[i].note >= 128 ||
                size_t(input.voices[i].owner.part) >= KSMusicalState::StorageSlotCount)
                return std::nullopt;

        size_t candidate = 0;
        uint16_t bestAge = 0xffff, bestCountdown = 0xffff;
        const auto minimum = [&](size_t i) noexcept {
            const auto& voice = input.voices[i];
            if (voice.age < bestAge || (voice.age == bestAge && voice.countdown < bestCountdown))
            {
                candidate = i;
                bestAge = voice.age;
                bestCountdown = voice.countdown;
            }
        };
        const bool mono = input.mode == KSVoiceMode::Mono || input.mode == KSVoiceMode::MonoGlide;
        const auto initial = mono ? KSNoteAllocation::MonoStart : KSNoteAllocation::PolyStart;
        if (mono && !input.newMonoVoice)
        {
            size_t first = 0;
            for (; first < input.count; ++first)
            {
                if (input.voices[first].owner == input.owner)
                    break;
                minimum(first);
            }
            if (first == input.count)
                return KSVoiceSelection{VoiceSlot(candidate), initial, bestAge != 0};

            bestAge = bestCountdown = 0;
            for (size_t i = first; i < input.count; ++i)
            {
                const auto& voice = input.voices[i];
                if (!(voice.owner == input.owner))
                    continue;
                if (voice.age & 0xc000)
                    return KSVoiceSelection{VoiceSlot(i), KSNoteAllocation::Reuse, false};
                if (voice.age > bestAge || (voice.age == bestAge && voice.countdown > bestCountdown))
                {
                    candidate = i;
                    bestAge = voice.age;
                    bestCountdown = voice.countdown;
                }
            }
            const auto allocation = !bestAge ? initial : bestAge < 256 ? KSNoteAllocation::ResumeRecent
                                                                     : KSNoteAllocation::ResumeEstablished;
            return KSVoiceSelection{VoiceSlot(candidate), allocation, false};
        }

        for (size_t i = 0; i < input.count; ++i)
        {
            const auto& voice = input.voices[i];
            if (input.mode == KSVoiceMode::PolySameNote)
            {
                if (voice.age && voice.owner == input.owner && voice.note == input.note)
                    return KSVoiceSelection{VoiceSlot(i), KSNoteAllocation::Reuse, false};
            }
            else if (!voice.age)
                return KSVoiceSelection{VoiceSlot(i), initial, false};
            minimum(i);
        }
        return KSVoiceSelection{VoiceSlot(candidate), initial, bestAge != 0};
    }
}
