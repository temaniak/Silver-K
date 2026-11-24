#pragma once

#include "../../../controller/command/dspTransaction.h"
#include "../../../controller/command/controllerStateView.h"
#include <optional>

namespace novation::controller::command
{
    struct KVoiceFeedbackState
    {
        uint16_t age = 0;
        uint16_t countdown = 0xffff;
        friend constexpr bool operator==(KVoiceFeedbackState a, KVoiceFeedbackState b) noexcept
        { return a.age == b.age && a.countdown == b.countdown; }
    };

    enum class KVoiceFeedbackAction : uint8_t { Idle, ReadPendingStatus, ReadAge };
    enum class KMailboxAction : uint8_t { Continue, AcknowledgeAndRebuild };
    struct KMailboxFeedback { uint8_t status; KMailboxAction action; };

    // K OS2.1.01 has one DSP and eight voices at stride30. Active voices read
    // age directly. There is no preceding status read to clear retained bit14.
    class KFeedbackProtocol final
    {
    public:
        static constexpr size_t VoiceCount = 8;
        static constexpr uint32_t TriggerPendingBit = 0x80;
        static constexpr uint16_t triggerAddress(VoiceSlot slot) noexcept
        { return uint16_t(0x1a+81*size_t(slot)); }
        static constexpr uint16_t gateAddress(VoiceSlot slot) noexcept
        { return uint16_t(0x8052+30*size_t(slot)); }
        static constexpr KVoiceFeedbackAction action(uint16_t age) noexcept
        {
            return !age ? KVoiceFeedbackAction::Idle :
                (age & 0x8000) ? KVoiceFeedbackAction::ReadPendingStatus : KVoiceFeedbackAction::ReadAge;
        }
        static constexpr std::optional<DspTransactionDraft> voiceRead(
            VoiceSlot slot, uint16_t age, NativeFrame at) noexcept
        {
            if (size_t(slot) >= VoiceCount || action(age) == KVoiceFeedbackAction::Idle) return std::nullopt;
            return read(uint16_t((age & 0x8000 ? 0x8052 : 0x8061) + 30 * size_t(slot)), at);
        }
        static constexpr std::optional<KVoiceFeedbackState> result(KVoiceFeedbackState before, uint32_t word) noexcept
        {
            if (word > 0xffffff || action(before.age) == KVoiceFeedbackAction::Idle) return std::nullopt;
            if ((before.age & 0x8000) && !(word & 1)) return KVoiceFeedbackState{0x7fff, 0xffff};
            const auto remaining = uint16_t(before.countdown - 1u);
            const auto age = before.age & 0x8000 ? before.age :
                uint16_t((before.age & 0x4000u) | (uint16_t(word >> 8) >> 1));
            return KVoiceFeedbackState{age, remaining ? remaining : before.countdown};
        }
        static constexpr DspTransactionDraft mailboxRead(NativeFrame at) noexcept { return read(0x802b, at); }
        static constexpr std::optional<KMailboxFeedback> mailboxResult(uint32_t word) noexcept
        {
            if (word > 0xffffff) return std::nullopt;
            const auto status = uint8_t(word);
            return KMailboxFeedback{status, status == 3 ? KMailboxAction::AcknowledgeAndRebuild : KMailboxAction::Continue};
        }
    private:
        static constexpr DspTransactionDraft read(uint16_t address, NativeFrame at) noexcept
        {
            DspTransactionDraft draft;
            draft.model = Model::K; draft.endpoint = Endpoint::A; draft.earliest = at;
            draft.address = {DspAddressMode::Read, address};
            draft.releaseEndpoint = DspMilestone::Observation;
            return draft;
        }
    };
}
