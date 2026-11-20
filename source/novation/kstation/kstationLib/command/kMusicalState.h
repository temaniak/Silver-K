#pragma once

#include "../../../controller/command/controllerStateView.h"
#include "kFeedbackProtocol.h"

namespace novation::controller::command
{
    enum class KPart { Main };

    struct KParameters
    {
        uint8_t cutoff = 0;
        uint8_t modWheel = 0;
        uint8_t pressure = 0,breath = 0,volume = 127;
        uint16_t pitchBend = 256;
    };

    class KMusicalState
    {
    public:
        static constexpr size_t PartCount = 1;
        static constexpr size_t VoiceCount = KFeedbackProtocol::VoiceCount;
        using Part = PartState<KParameters>;
        using Voices = VoiceAllocations<KPart, VoiceCount>;

        struct Projection
        {
            Part part;
            Voices::Projection voices;
            std::array<uint16_t, VoiceCount> feedbackCountdown;
            bool sustainDown;
            std::array<bool, VoiceCount> deferredRelease;
            std::array<bool, VoiceCount> activationPending;
            std::array<bool, VoiceCount> releasePending;
            size_t activeVoiceLimit;
            std::array<uint8_t, VoiceCount> unisonUnits;
        };

        using View = ControllerStateView<Projection>;

        explicit KMusicalState(Session session) noexcept
            : session_(session), voices_(session), view_(session) {}
        KMusicalState(const KMusicalState&) = delete;
        KMusicalState& operator=(const KMusicalState&) = delete;
        Session session() const noexcept { return session_; }

        // Program lookup and validation belong to the caller's program store.
        bool loadProgram(ProgramSelection program, KParameters parameters) noexcept
        {
            if (parameters.cutoff >= 128 || !advanceRevision(part_.revisions.program))
                return false;
            part_.program = program;
            part_.parameters = parameters;
            return true;
        }

        bool canReplaceProgram(NativeFrame at) const noexcept
        {
            return part_.program && part_.revisions.program != std::numeric_limits<uint64_t>::max() && canPublish(at);
        }

        bool replaceProgram(ProgramSelection program, KParameters parameters, size_t voiceLimit, NativeFrame at) noexcept
        {
            if (!voiceLimit || voiceLimit>VoiceCount || !canReplaceProgram(at) || !loadProgram(program, parameters)) return false;
            for(size_t i=0;i<VoiceCount;++i) {
                if(const auto slot=voice(VoiceSlot(i)))retireVoice(slot->allocation);
                countdown_[i]=0;
            }
            activeVoiceLimit_=voiceLimit;
            return publish(at);
        }

        size_t activeVoiceLimit() const noexcept {return activeVoiceLimit_;}

        bool editCutoff(uint8_t value) noexcept
        {
            if (!part_.program || value >= 128 || !advanceRevision(part_.revisions.parameters))
                return false;
            part_.parameters.cutoff = value;
            return true;
        }

        bool canCommitModWheel(uint8_t value, NativeFrame at) const noexcept
        {
            return part_.program && value < 128 &&
                part_.revisions.parameters != std::numeric_limits<uint64_t>::max() && canPublish(at);
        }
        bool commitModWheel(uint8_t value, NativeFrame at) noexcept
        {
            if (!canCommitModWheel(value,at)) return false;
            part_.parameters.modWheel=value; ++part_.revisions.parameters;
            return publish(at);
        }

        bool canCommitLiveParameters(KParameters parameters,size_t voiceLimit,NativeFrame at) const noexcept
        {
            return part_.program && parameters.cutoff<128 && parameters.modWheel<128 && parameters.volume<128 &&
                parameters.pressure<128 && parameters.breath<128 && parameters.pitchBend<=512 &&
                voiceLimit && voiceLimit<=VoiceCount && part_.revisions.parameters!=std::numeric_limits<uint64_t>::max() &&
                canPublish(at);
        }
        bool commitLiveParameters(KParameters parameters,size_t voiceLimit,uint8_t retire,NativeFrame at) noexcept
        {
            if(!canCommitLiveParameters(parameters,voiceLimit,at))return false;
            for(size_t i=0;i<VoiceCount;++i)
                if(retire&(1u<<i))if(const auto current=voice(VoiceSlot(i)))retireVoice(current->allocation);
            activeVoiceLimit_=voiceLimit;
            part_.parameters=parameters;
            ++part_.revisions.parameters;
            return publish(at);
        }
        bool canCommitRelease(const VoiceAllocation* allocations,size_t count,NativeFrame at) const noexcept
        {
            if (count>VoiceCount || (count && !allocations) || !part_.program || !canPublish(at)) return false;
            for(size_t i=0;i<count;++i) {
                const auto source=voice(allocations[i]);
                if(!source || !source->feedbackAge || source->observationRevision==std::numeric_limits<uint64_t>::max() ||
                    (i && size_t(allocations[i-1].slot)>=size_t(allocations[i].slot))) return false;
            }
            return true;
        }
        bool sustainDown() const noexcept { return sustainDown_; }
        bool deferredRelease(VoiceAllocation allocation) const noexcept
        {
            return voice(allocation) && deferredRelease_[size_t(allocation.slot)];
        }
        bool activationPending(VoiceAllocation allocation) const noexcept
        {
            return voice(allocation) && activationPending_[size_t(allocation.slot)];
        }
        bool releasePending(VoiceAllocation allocation) const noexcept
        {
            return voice(allocation) && releasePending_[size_t(allocation.slot)];
        }
        bool hasReadyRelease() const noexcept
        {
            for(size_t i=0;i<VoiceCount;++i)
                if(releasePending_[i] && !activationPending_[i])return true;
            return false;
        }
        bool confirmActivation(VoiceAllocation allocation, NativeFrame at) noexcept
        {
            if(!voiceOperationAvailable() || !activationPending(allocation) || !canPublish(at)) return false;
            activationPending_[size_t(allocation.slot)]=false;
            return publish(at);
        }
        bool commitRelease(const VoiceAllocation* allocations,size_t count,NativeFrame at,
                           bool defer=false,std::optional<bool> pedal=std::nullopt) noexcept
        {
            if(!canCommitRelease(allocations,count,at))return false;
            for(size_t i=0;i<count;++i) {
                const auto index=size_t(allocations[i].slot);
                if(defer) {
                    if(!releasePending_[index])deferredRelease_[index]=true;
                    continue;
                }
                deferredRelease_[index]=false;
                if(activationPending_[index]) {
                    releasePending_[index]=true;
                } else {
                    const auto before=voice(allocations[i]);
                    if(!voices_.observeFeedback(allocations[i],uint16_t((before->feedbackAge&0x3fffu)|1u)))return false;
                    countdown_[index]=0xffff;
                    releasePending_[index]=false;
                }
            }
            if(pedal) sustainDown_=*pedal;
            return publish(at);
        }

        Part part() const noexcept { return part_; }

        std::optional<ParameterDependency<KPart>> dependency(uint64_t rateGeneration) const noexcept
        {
            if (!part_.program)
                return std::nullopt;
            return ParameterDependency<KPart>{session_, KPart::Main, part_.revisions, 0, rateGeneration};
        }

        std::optional<VoiceAllocation> assignVoice(VoiceSlot slot, uint8_t note, uint8_t velocity,
                                                  uint16_t feedbackAge, uint16_t countdown = 0xffff,
                                                  uint8_t unit = 0) noexcept
        {
            if (!part_.program || size_t(slot)>=activeVoiceLimit_)
                return std::nullopt;
            const auto allocation = voices_.assign(slot, KPart::Main, note, velocity, feedbackAge);
            if (allocation) {
                countdown_[size_t(slot)] = countdown;
                deferredRelease_[size_t(slot)] = false;
                activationPending_[size_t(slot)] = false;
                releasePending_[size_t(slot)] = false;
                units_[size_t(slot)] = unit;
            }
            return allocation;
        }

        bool canStartVoice(VoiceSlot slot,uint8_t note,NativeFrame at) const noexcept
        { return part_.program && size_t(slot)<activeVoiceLimit_ && voices_.canAssign(slot,note) && canPublish(at); }

        bool startVoice(VoiceSlot slot,uint8_t note,uint8_t velocity,NativeFrame at,uint8_t unit=0) noexcept
        {
            if(!canStartVoice(slot,note,at) || !assignVoice(slot,note,velocity,0x8000,0xffff,unit)) return false;
            activationPending_[size_t(slot)]=true;
            return publish(at);
        }

        bool observeVoiceFeedback(VoiceAllocation allocation, uint16_t feedbackAge) noexcept
        {
            if (activationPending(allocation) || !voices_.observeFeedback(allocation, feedbackAge)) return false;
            if (!feedbackAge) {
                deferredRelease_[size_t(allocation.slot)] = false;
                releasePending_[size_t(allocation.slot)] = false;
            }
            return true;
        }

        bool retireVoice(VoiceAllocation allocation) noexcept
        {
            if (!voices_.retire(allocation)) return false;
            deferredRelease_[size_t(allocation.slot)] = false;
            activationPending_[size_t(allocation.slot)] = false;
            releasePending_[size_t(allocation.slot)] = false;
            units_[size_t(allocation.slot)] = 0;
            return true;
        }

        std::optional<Voices::Voice> voice(VoiceSlot slot) const noexcept { return voices_.read(slot); }
        std::optional<uint64_t> voiceGeneration(VoiceSlot slot) const noexcept { return voices_.generation(slot); }
        uint8_t voiceUnit(VoiceSlot slot) const noexcept {return units_[size_t(slot)];}

        std::optional<Voices::Voice> voice(VoiceAllocation allocation) const noexcept
        {
            return voices_.read(allocation);
        }

        // After the event has committed, publish desired patch values and the
        // latest voice observations. This does not assert DSP application.
        bool canPublish(NativeFrame frame) const noexcept { return view_.canPublish(frame); }
        bool publish(NativeFrame frame) noexcept
        {
            return view_.publish(frame, {part_, voices_.projection(), countdown_, sustainDown_, deferredRelease_,
                                        activationPending_, releasePending_, activeVoiceLimit_, units_});
        }
        std::optional<View::Snapshot> snapshot() const noexcept { return view_.snapshot(); }

        std::optional<KVoiceFeedbackState> feedback(VoiceAllocation allocation) const noexcept
        {
            const auto source = voice(allocation);
            return source ? std::optional{KVoiceFeedbackState{source->feedbackAge, countdown_[size_t(allocation.slot)]}} : std::nullopt;
        }
        bool commitVoiceFeedback(VoiceAllocation allocation, uint64_t observation, KVoiceFeedbackState value,
                                 NativeFrame at) noexcept
        {
            const auto source = voice(allocation);
            if (!source || source->observationRevision != observation ||
                observation == std::numeric_limits<uint64_t>::max() || !canPublish(at)) return false;
            if (!observeVoiceFeedback(allocation, value.age)) return false;
            countdown_[size_t(allocation.slot)] = value.countdown;
            return publish(at);
        }
        bool voiceOperationAvailable() const noexcept
        { return !voiceOperationOwner_ && voiceOperationRevision_ != std::numeric_limits<uint64_t>::max(); }
        bool ownsVoiceOperation(const void* owner) const noexcept { return owner && voiceOperationOwner_ == owner; }
        uint64_t voiceOperationRevision() const noexcept { return voiceOperationRevision_; }
        bool claimVoiceOperation(const void* owner) noexcept
        {
            if (!owner || !voiceOperationAvailable()) return false;
            voiceOperationOwner_ = owner; ++voiceOperationRevision_; return true;
        }
        bool releaseVoiceOperation(const void* owner) noexcept
        {
            if (!ownsVoiceOperation(owner)) return false;
            voiceOperationOwner_ = nullptr; return true;
        }

    private:
        Session session_;
        Part part_{};
        Voices voices_;
        View view_;
        std::array<uint16_t, VoiceCount> countdown_{};
        std::array<uint8_t, VoiceCount> units_{};
        std::array<bool, VoiceCount> deferredRelease_{};
        std::array<bool, VoiceCount> activationPending_{}, releasePending_{};
        bool sustainDown_ = false;
        size_t activeVoiceLimit_ = VoiceCount;
        const void* voiceOperationOwner_ = nullptr;
        uint64_t voiceOperationRevision_ = 0;
    };
}
