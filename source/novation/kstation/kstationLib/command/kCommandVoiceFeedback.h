#pragma once

#include "kMusicalState.h"
#include "../../../controller/command/commandSession.h"

namespace novation::controller::command
{
    enum class KFeedbackProgress : uint8_t { Ready, WaitingForReply, Complete, Fault };

    // One correlated real DSP observation. No foreground cursor, polling
    // cadence, MCU state or arithmetic delay is retained by this owner.
    class KCommandVoiceFeedback final
    {
    public:
        KCommandVoiceFeedback() = default;
        ~KCommandVoiceFeedback();
        KCommandVoiceFeedback(const KCommandVoiceFeedback&) = delete;
        KCommandVoiceFeedback& operator=(const KCommandVoiceFeedback&) = delete;
        bool prepare(CommandSession&, KMusicalState&, VoiceAllocation) noexcept;
        DspAdmission admit(uint64_t operation, NativeFrame at) noexcept;
        KFeedbackProgress accept(DspReply reply, NativeFrame at) noexcept;
        KFeedbackProgress progress() const noexcept;
        std::optional<KVoiceFeedbackState> result() const noexcept
        { return complete_ ? std::optional{result_} : std::nullopt; }
        std::optional<DspTicket> ticket() const noexcept { return ticket_; }
        KFeedbackProgress abort() noexcept { return fail(); }
        bool reset() noexcept;
    private:
        bool live() const noexcept;
        bool current() const noexcept;
        KFeedbackProgress fail() noexcept;
        DspAdmission reject(DspAdmission) noexcept;
        CommandSession* session_ = nullptr;
        KMusicalState* state_ = nullptr;
        VoiceAllocation allocation_{};
        ParameterDependency<KPart> dependency_{};
        KVoiceFeedbackState before_{}, result_{};
        uint64_t observation_ = 0, operationRevision_ = 0;
        std::optional<DspTicket> ticket_;
        bool accepted_ = false, complete_ = false, fault_ = false;
    };
}
