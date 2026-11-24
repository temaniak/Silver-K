#pragma once

#include "ksMusicalState.h"
#include "../../../controller/command/dspTransaction.h"

namespace novation::controller::command
{
    enum class KSPartFeedbackAction : uint8_t { Continue, AcknowledgeAndRebuild };
    enum class KSVoiceFeedbackAction : uint8_t { Idle, ReadStatusAndAge, ReadPendingStatus };

    struct KSPartFeedbackResult
    {
        uint8_t status;
        KSPartFeedbackAction action;
    };

    struct KSVoiceFeedbackState
    {
        uint16_t age;
        uint16_t countdown;
    };

    // One foreground pass visits the next voice slot before its four A reads.
    // The owner supplies the initial cursor and each observation boundary.
    class KSCommandFeedback final
    {
    public:
        static constexpr VoiceSlot nextVoiceSlot(VoiceSlot slot) noexcept
        {
            const auto next = size_t(slot) + 1;
            return VoiceSlot(next < KSMusicalState::VoiceCount ? next : 0);
        }

        static constexpr std::optional<DspTransactionDraft> partRead(KSPart part, NativeFrame at) noexcept
        {
            if (size_t(part) >= KSMusicalState::PartCount)
                return std::nullopt;
            return read(Endpoint::A, partStatusAddress(part), at);
        }

        static constexpr uint16_t partStatusAddress(KSPart part) noexcept
        {
            return uint16_t(0x8041u + size_t(part));
        }

        static constexpr uint16_t voiceStatusAddress(VoiceSlot slot) noexcept
        {
            return uint16_t(0x8085 + size_t(slot) * AlternateVoiceStride);
        }

        static constexpr DspTransactionDraft voiceStatusRead(VoiceSlot slot, NativeFrame at) noexcept
        {
            return read(Endpoint::B, voiceStatusAddress(slot), at);
        }

        static constexpr DspTransactionDraft voiceAgeRead(VoiceSlot slot, NativeFrame at) noexcept
        {
            return read(Endpoint::B, uint16_t(0x808f + size_t(slot) * AlternateVoiceStride), at);
        }

        static constexpr KSPartFeedbackResult partResult(uint32_t word) noexcept
        {
            const auto status = uint8_t(word);
            return {status, status == 3 ? KSPartFeedbackAction::AcknowledgeAndRebuild
                                       : KSPartFeedbackAction::Continue};
        }

        static constexpr KSVoiceFeedbackAction voiceAction(uint16_t age) noexcept
        {
            if (!age)
                return KSVoiceFeedbackAction::Idle;
            return (age & 0x8000u) ? KSVoiceFeedbackAction::ReadPendingStatus
                                   : KSVoiceFeedbackAction::ReadStatusAndAge;
        }

        // A pending trigger owns only a status read. An acknowledged trigger
        // activates the voice without an age read or countdown decrement.
        static constexpr std::optional<KSVoiceFeedbackState> pendingResult(
            KSVoiceFeedbackState before, uint32_t statusWord) noexcept
        {
            if (voiceAction(before.age) != KSVoiceFeedbackAction::ReadPendingStatus)
                return std::nullopt;
            if (!(statusWord & 1u))
                return KSVoiceFeedbackState{0x7fff, 0xffff};
            const auto remaining = uint16_t(before.countdown - 1u);
            return KSVoiceFeedbackState{before.age, remaining ? remaining : before.countdown};
        }

        // Sample the owned voice age at status admission. Resolve only after
        // the correlated status and age reads both drain. A zero status also
        // takes an age read and clears the retained bit for a release tail.
        static constexpr std::optional<KSVoiceFeedbackState> voiceResult(
            KSVoiceFeedbackState before, uint32_t statusWord, uint32_t ageWord) noexcept
        {
            if (voiceAction(before.age) != KSVoiceFeedbackAction::ReadStatusAndAge)
                return std::nullopt;
            const auto status = uint8_t(statusWord);
            const auto retained = (status & 8u) ? (before.age & 0x4000u) : 0u;
            const auto age = uint16_t(ageWord >> 8) >> 1;
            const auto decremented = uint16_t(before.countdown - 1u);
            return KSVoiceFeedbackState{uint16_t(retained | age), decremented ? decremented : before.countdown};
        }

    private:
        static constexpr size_t AlternateVoiceStride = 22;

        static constexpr DspTransactionDraft read(Endpoint endpoint, uint16_t address, NativeFrame at) noexcept
        {
            DspTransactionDraft draft;
            draft.model = Model::KS;
            draft.endpoint = endpoint;
            draft.earliest = at;
            draft.releaseEndpoint = DspMilestone::Observation;
            draft.address = {DspAddressMode::Read, address};
            return draft;
        }
    };
}
