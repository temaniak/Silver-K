#pragma once

#include "../../../controller/command/commandSession.h"
#include "kCommandWriteState.h"
#include "kPolyNoteImage.h"
#include "kPolyVoiceSelection.h"
#include "kWriteProgress.h"

namespace novation::controller::command
{
    // Owns the selected generation from the pitch query until the trigger
    // transfers. Reserve the complete DSP image and trigger before publication.
    class KCommandNoteOn final
    {
    public:
        KCommandNoteOn()=default;
        ~KCommandNoteOn();
        KCommandNoteOn(const KCommandNoteOn&)=delete;
        KCommandNoteOn& operator=(const KCommandNoteOn&)=delete;

        bool prepare(CommandSession&,KMusicalState&,const KExpressionSource&,const KVoiceTunings&,
                     uint8_t note,uint8_t velocity,KVoiceStart start={}) noexcept;
        DspAdmission admitPitch(uint64_t operation,NativeFrame) noexcept;
        bool acceptPitch(DspReply,NativeFrame) noexcept;
        DspAdmission admitImage(uint64_t operation,NativeFrame) noexcept;
        KWriteProgress progress() noexcept;
        std::optional<DspTicket> ticket() const noexcept;
        KPolyVoiceSelection selection() const noexcept { return selection_; }
        const KPolyNoteImage& image() const noexcept { return image_; }
        uint32_t trigger() const noexcept { return repeatsVoice()?repeat_.trigger:image_.trigger; }
        uint8_t glideNote() const noexcept { return repeatsVoice()?repeat_.glideNote:image_.glideNote; }
        bool reset() noexcept;

    private:
        static constexpr size_t MaxWrites=6;
        bool repeatsVoice() const noexcept { return selection_.reason==KPolyVoiceSelectionReason::SameNote; }
        size_t writeCount() const noexcept { return repeatsVoice()?6:5; }
        bool current() const noexcept;
        bool fail() noexcept;
        DspAdmission rejected(DspAdmission) noexcept;
        uint16_t pitchAddress() const noexcept { return uint16_t(0x805f+30*size_t(selection_.slot)); }
        std::array<DspTransactionDraft,MaxWrites> writes(NativeFrame) const noexcept;

        CommandSession* session_=nullptr;
        KMusicalState* model_=nullptr;
        kCommandDetail::WriteSource captured_{};
        KExpressionSource expression_{};
        KVoiceTunings tunings_{};
        KVoiceStart start_{};
        KPolyVoiceSelection selection_{};
        KPolyNoteImage image_{};
        KRepeatNoteUpdate repeat_{};
        std::optional<DspTicket> pitch_;
        std::array<DspTicket,MaxWrites> tickets_{};
        size_t next_=0;
        uint64_t revision_=0;
        uint32_t slotWord_=0;
        uint8_t note_=0,velocity_=0;
        bool acquired_=false,pitchReady_=false,writing_=false,fault_=false,complete_=false;
    };
}
