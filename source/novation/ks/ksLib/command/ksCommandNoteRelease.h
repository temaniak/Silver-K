#pragma once

#include "ksCommandFeedback.h"
#include "../../../controller/command/commandSession.h"
#include <algorithm>
#include <exception>

namespace novation::controller::command
{
    // One complete polyphonic Note Off. All matching gate writes reserve as
    // one immutable batch; model/key changes commit only after reservation.
    class KSCommandNoteRelease final
    {
    public:
        static constexpr size_t Capacity = KSMusicalState::VoiceCount;
        struct Release
        {
            VoiceAllocation allocation;
            uint16_t age;
            uint64_t observationRevision;
            uint16_t address;
        };

        static std::optional<KSCommandNoteRelease> prepare(KSMusicalState& state,
            KSStorageSlot part, uint8_t note, uint16_t excludedSlots=0) noexcept
        {
            const auto dependency = state.dependency(part, 0);
            const auto* keys = state.keys(part);
            const auto* sustain = state.sustain(part);
            if (!dependency || !keys || !sustain || note >= 128 || state.voiceOperationBusy())
                return std::nullopt;
            KSCommandNoteRelease result(state, *dependency, note, keys->revision(), *sustain);
            result.defer_ = sustain->down && !sustain->bypass;
            // Released tails still own a live ordinary voice. A second Note
            // Off must not be suppressed by the physical/held key bitmap.
            for (size_t index = 0; index < Capacity; ++index)
            {
                if (excludedSlots & (1u << index)) continue;
                const auto& voice = result.voicesBefore_[index];
                if (voice && voice->feedbackAge && voice->part == part && voice->note == note &&
                    state.noteOffExtra(voice->allocation)==0)
                    result.releases_[result.matchedCount_++] = {voice->allocation, voice->feedbackAge,
                        voice->observationRevision, KSCommandFeedback::voiceStatusAddress(VoiceSlot(index))};
            }
            result.count_ = result.defer_ ? 0 : result.matchedCount_;
            return result;
        }

        static std::optional<KSCommandNoteRelease> prepareSustain(KSMusicalState& state,
            KSStorageSlot part, uint8_t value) noexcept
        {
            const auto dependency=state.dependency(part,0);
            const auto* keys=state.keys(part);const auto* sustain=state.sustain(part);
            if (!dependency || !keys || !sustain || value>=128 || state.voiceOperationBusy()) return std::nullopt;
            KSCommandNoteRelease result(state,*dependency,0,keys->revision(),*sustain);
            result.pedal_=true;result.pedalDown_=value>=64;
            if (sustain->down && !result.pedalDown_ && !sustain->bypass)
                for(size_t i=0;i<Capacity;++i) {
                    const auto& voice=result.voicesBefore_[i];
                    if(voice && voice->feedbackAge && voice->part==part && result.pendingBefore_[i] &&
                       *result.pendingBefore_[i]==voice->allocation)
                        result.releases_[result.matchedCount_++]={voice->allocation,voice->feedbackAge,
                            voice->observationRevision,KSCommandFeedback::voiceStatusAddress(VoiceSlot(i))};
                }
            result.count_=result.matchedCount_;
            return result;
        }

        static std::optional<KSCommandNoteRelease> prepareOwned(KSMusicalState& state,KSStorageSlot part,
            const VoiceAllocation* allocations,size_t count) noexcept
        {
            const auto dependency=state.dependency(part,0);
            const auto* keys=state.keys(part);const auto* sustain=state.sustain(part);
            if(!dependency || !keys || !sustain || !allocations || count>Capacity || state.voiceOperationBusy())
                return std::nullopt;
            KSCommandNoteRelease result(state,*dependency,0,keys->revision(),*sustain);
            result.gateOnly_=true;
            for(size_t i=0;i<Capacity;++i) {
                const auto& voice=result.voicesBefore_[i];
                if(voice && voice->feedbackAge && voice->part==part &&
                   std::find(allocations,allocations+count,voice->allocation)!=allocations+count) {
                    if(KSCommandFeedback::voiceAction(voice->feedbackAge)==KSVoiceFeedbackAction::ReadPendingStatus)
                        return std::nullopt;
                    result.releases_[result.matchedCount_++]={voice->allocation,voice->feedbackAge,
                        voice->observationRevision,KSCommandFeedback::voiceStatusAddress(VoiceSlot(i))};
                }
            }
            result.count_=result.matchedCount_;
            return result;
        }

        // Whole-part release ignores sustain. Physical and held keys clear,
        // while pedal state and allocation-owned pending markers persist.
        static std::optional<KSCommandNoteRelease> prepareAllNotes(KSMusicalState& state,
            KSStorageSlot part) noexcept
        {
            const auto dependency=state.dependency(part,0);
            const auto* keys=state.keys(part);const auto* sustain=state.sustain(part);
            if(!dependency || !keys || !sustain || state.voiceOperationBusy())return std::nullopt;
            KSCommandNoteRelease result(state,*dependency,0,keys->revision(),*sustain);
            result.allKeys_=true;
            for(size_t i=0;i<Capacity;++i) {
                const auto& voice=result.voicesBefore_[i];
                if(voice && voice->feedbackAge && voice->part==part)
                    result.releases_[result.matchedCount_++]={voice->allocation,voice->feedbackAge,
                        voice->observationRevision,KSCommandFeedback::voiceStatusAddress(VoiceSlot(i))};
            }
            result.count_=result.matchedCount_;return result;
        }

        // Mono priority replaces the sounding note through a complete new
        // startup intent; this first operation releases only the input key.
        static std::optional<KSCommandNoteRelease> prepareKeysOnly(KSMusicalState& state,
            KSStorageSlot part,uint8_t note) noexcept
        {
            const auto dependency=state.dependency(part,0);const auto* keys=state.keys(part);const auto* sustain=state.sustain(part);
            if(!dependency || !keys || !sustain || note>=128 || state.voiceOperationBusy())return std::nullopt;
            return KSCommandNoteRelease(state,*dependency,note,keys->revision(),*sustain);
        }

        DspAdmission admit(CommandSession& session, uint64_t operation, NativeFrame at) noexcept
        {
            const auto current = session.snapshot();
            if (current.session != state_->session() || current.lifecycle != Lifecycle::Running)
                return DspAdmission::StaleSession;
            if (committed_)
                return DspAdmission::StaleOperation;
            const auto dependency = state_->dependency(dependency_.part, 0);
            const auto* keys = state_->keys(dependency_.part);
            const auto* sustain = state_->sustain(dependency_.part);
            if (!dependency || !(*dependency == dependency_) || !keys || keys->revision() != keyRevision_ ||
                !sustain || sustain->revision!=sustain_.revision || state_->voiceOperationBusy() ||
                (!gateOnly_ && !pedal_ && !keys->canChange()) ||
                (pedal_ && sustain->revision==std::numeric_limits<uint64_t>::max()) || !state_->canPublish(at))
                return DspAdmission::InvalidDraft;
            for (size_t i = 0; i < matchedCount_; ++i)
            {
                const auto voice = state_->voice(releases_[i].allocation);
                if (!voice || voice->feedbackAge != releases_[i].age ||
                    voice->observationRevision != releases_[i].observationRevision ||
                    (!defer_ && voice->observationRevision == std::numeric_limits<uint64_t>::max()))
                    return DspAdmission::InvalidDraft;
            }
            // A voice that did not match at prepare must not silently become
            // a new matching allocation while a full-queue retry is pending.
            for (size_t i = 0; i < Capacity; ++i)
            {
                const auto currentVoice = state_->voice(VoiceSlot(i));
                const auto& before = voicesBefore_[i];
                if (*state_->voiceGeneration(VoiceSlot(i))!=generations_[i] ||
                    !(state_->deferredVoice(VoiceSlot(i))==pendingBefore_[i]) ||
                    bool(currentVoice) != bool(before) || (before &&
                    (!(currentVoice->allocation == before->allocation) ||
                     currentVoice->observationRevision != before->observationRevision)))
                    return DspAdmission::InvalidDraft;
            }
            const auto commit = [&]() noexcept {
                // The single worker prevalidated every mutation above. No
                // callback can interleave a new key/program/voice observation.
                if(pedal_) {if(!state_->setSustainDown(dependency_.part,pedalDown_))std::terminate();}
                else if (!gateOnly_ && !(allKeys_ ? state_->clearKeys(dependency_.part) : state_->releaseKey(dependency_.part, note_)))
                    std::terminate();
                for (size_t i = 0; i < matchedCount_; ++i) {
                    const auto allocation=releases_[i].allocation;
                    if(defer_) {if(!state_->deferVoice(allocation))std::terminate();}
                    else {
                        if(pedal_ && !state_->clearDeferredVoice(allocation))std::terminate();
                        if(!state_->observeVoiceFeedback(allocation,releasedAge(i)))std::terminate();
                    }
                }
                if (!state_->publish(at)) std::terminate();
                committed_ = true;
            };
            if (session.transactions().poisoned(Endpoint::B)) return DspAdmission::Poisoned;
            if (!count_)
            {
                commit();
                return DspAdmission::Accepted;
            }
            const uint32_t gateOff = 0;
            std::array<DspTransactionDraft, Capacity> drafts;
            for (size_t i = 0; i < count_; ++i)
            {
                drafts[i].model = Model::KS;
                drafts[i].endpoint = Endpoint::B;
                drafts[i].earliest = at;
                drafts[i].address = {DspAddressMode::Write, releases_[i].address};
                drafts[i].words = &gateOff;
                drafts[i].wordCount = 1;
            }
            return session.transactions().submit(state_->session(), operation, drafts.data(), count_, tickets_.data(), commit);
        }

        size_t size() const noexcept { return count_; }
        size_t deferredCount() const noexcept { return defer_ ? matchedCount_ : 0; }
        const Release& release(size_t i) const noexcept { return releases_[i]; }
        uint16_t releasedAge(size_t i) const noexcept { return uint16_t((releases_[i].age & 0x3fff) | 1); }
        std::optional<DspTicket> ticket(size_t i) const noexcept
        {
            return committed_ && i < count_ ? std::optional{tickets_[i]} : std::nullopt;
        }

    private:
        KSCommandNoteRelease(KSMusicalState& state, ParameterDependency<KSStorageSlot> dependency,
            uint8_t note, uint64_t keyRevision, KSSustainState sustain) noexcept
            : state_(&state), dependency_(dependency), note_(note), keyRevision_(keyRevision), sustain_(sustain) {
            for(size_t i=0;i<Capacity;++i) {
                voicesBefore_[i]=state.voice(VoiceSlot(i));pendingBefore_[i]=state.deferredVoice(VoiceSlot(i));
                generations_[i]=*state.voiceGeneration(VoiceSlot(i));
            }
        }
        KSMusicalState* state_;
        ParameterDependency<KSStorageSlot> dependency_;
        uint8_t note_;
        uint64_t keyRevision_;
        std::array<Release, Capacity> releases_{};
        KSMusicalState::Voices::Projection voicesBefore_{};
        KSMusicalState::DeferredVoices pendingBefore_{};
        std::array<uint64_t,Capacity> generations_{};
        std::array<DspTicket, Capacity> tickets_{};
        size_t count_ = 0;
        size_t matchedCount_ = 0;
        KSSustainState sustain_{};
        bool pedal_=false,pedalDown_=false,defer_=false,allKeys_=false,gateOnly_=false;
        bool committed_ = false;
    };
}
