#pragma once

#include "../../../controller/command/controllerStateView.h"
#include "ksKeyState.h"

#include <bitset>

namespace novation::controller::command
{
    enum class KSLayout { Single, Performance };
    enum class KSPart : uint8_t { One, Two, Three, Four };
    enum class KSStorageSlot : uint8_t { Single, Multi1, Multi2, Multi3, Multi4 };
    enum class KSProgramRefresh : uint8_t { Deferred, OwnedReload };

    struct KSSustainState
    {
        bool down = false, bypass = false;
        uint64_t revision = 0;
    };

    struct KSFilterParameters
    {
        uint8_t cutoff = 0;
        uint8_t velocityDepth = 0;
    };

    struct KSCutoffInputs
    {
        KSFilterParameters filter;
        uint8_t velocity;
        uint16_t staticBase;

        friend constexpr bool operator==(const KSCutoffInputs& left, const KSCutoffInputs& right) noexcept
        {
            return left.filter.cutoff == right.filter.cutoff &&
                left.filter.velocityDepth == right.filter.velocityDepth &&
                left.velocity == right.velocity && left.staticBase == right.staticBase;
        }
    };

    struct KSCutoffObservation
    {
        ParameterDependency<KSStorageSlot> part;
        VoiceAllocation allocation;
        uint64_t voiceObservationRevision;
        KSCutoffInputs inputs;

        friend constexpr bool operator==(const KSCutoffObservation& left,
                                         const KSCutoffObservation& right) noexcept
        {
            return left.part == right.part && left.allocation == right.allocation &&
                left.voiceObservationRevision == right.voiceObservationRevision && left.inputs == right.inputs;
        }
    };

    class KSMusicalState
    {
    public:
        static constexpr size_t PartCount = 4;
        static constexpr size_t StorageSlotCount = 5;
        static constexpr size_t VoiceCount = 14;
        using Part = PartState<KSFilterParameters>;
        using DirtySlots = std::bitset<StorageSlotCount>;
        using Voices = VoiceAllocations<KSStorageSlot, VoiceCount>;
        using DeferredVoices = std::array<std::optional<VoiceAllocation>, VoiceCount>;

        struct Projection
        {
            KSLayout layout;
            std::array<Part, StorageSlotCount> slots;
            DirtySlots dirty;
            Voices::Projection voices;
            std::array<KSKeyState::Projection, StorageSlotCount> keys;
            std::array<KSSustainState, StorageSlotCount> sustain;
            DeferredVoices deferred;
            std::array<uint8_t, VoiceCount> noteOffExtra;
        };

        using View = ControllerStateView<Projection>;

        explicit KSMusicalState(Session session, KSLayout layout = KSLayout::Single) noexcept
            : session_(session), layout_(layout), voices_(session), view_(session) {}
        KSMusicalState(const KSMusicalState&) = delete;
        KSMusicalState& operator=(const KSMusicalState&) = delete;
        Session session() const noexcept { return session_; }
        KSLayout layout() const noexcept { return layout_; }

        // One accepted voice operation owns its physical transfers until
        // completion or terminal fault. This latch is execution ownership,
        // not a second desired key/voice projection or a firmware counter.
        uint64_t voiceOperationRevision() const noexcept { return voiceOperationRevision_; }
        bool voiceOperationAvailable() const noexcept
        { return !voiceOperationOwner_ && voiceOperationRevision_ < std::numeric_limits<uint64_t>::max()-1; }
        bool ownsVoiceOperation(const void* owner) const noexcept { return owner && voiceOperationOwner_==owner; }
        bool claimVoiceOperation(const void* owner) noexcept
        {
            if (!owner || !voiceOperationAvailable()) return false;
            voiceOperationOwner_=owner;++voiceOperationRevision_;return true;
        }
        bool releaseVoiceOperation(const void* owner) noexcept
        {
            if (!ownsVoiceOperation(owner)) return false;
            voiceOperationOwner_=nullptr;++voiceOperationRevision_;return true;
        }

        static constexpr std::optional<KSPart> logicalPart(KSStorageSlot slot) noexcept
        {
            const auto index = size_t(slot);
            if (index >= StorageSlotCount)
                return std::nullopt;
            return KSPart(index ? index - 1 : 0);
        }

        static constexpr std::optional<KSStorageSlot> storageSlot(KSLayout layout, KSPart part) noexcept
        {
            const auto index = size_t(part);
            if (index >= PartCount)
                return std::nullopt;
            if (layout == KSLayout::Single)
                return part == KSPart::One ? std::optional{KSStorageSlot::Single} : std::nullopt;
            return KSStorageSlot(index + 1);
        }

        // A layout change is a dependency barrier. The model operation that
        // changes layout also owns any required retirement or refresh policy.
        bool setLayout(KSLayout layout) noexcept
        {
            if (layout == layout_)
                return true;
            if (!advanceRevision(routingRevision_))
                return false;
            layout_ = layout;
            return true;
        }

        // Program lookup and model-specific record validation happen before commit.
        bool loadProgram(KSStorageSlot slot, ProgramSelection program, KSFilterParameters filter,
                         KSProgramRefresh refresh = KSProgramRefresh::Deferred) noexcept
        {
            const auto index = size_t(slot);
            if (index >= StorageSlotCount || filter.cutoff >= 128 || filter.velocityDepth >= 128)
                return false;
            auto& part = slots_[index];
            if (!advanceRevision(part.revisions.program))
                return false;
            part.program = program;
            part.parameters = filter;
            // A direct reload owns its fade/auxiliary/release work. It neither
            // creates a deferred image nor discards an already pending edit.
            if (refresh == KSProgramRefresh::Deferred)
                dirty_.set(index);
            return true;
        }

        bool editCutoff(KSStorageSlot slot, uint8_t value) noexcept
        {
            if (!active(slot) || value >= 128)
                return false;
            const auto index = size_t(slot);
            auto& part = slots_[index];
            if (!part.program || !advanceRevision(part.revisions.parameters))
                return false;
            part.parameters.cutoff = value;
            dirty_.set(index);
            return true;
        }

        bool canTouchParameters(KSStorageSlot slot)const noexcept
        {return active(slot) && slots_[size_t(slot)].program &&
            slots_[size_t(slot)].revisions.parameters!=std::numeric_limits<uint64_t>::max();}
        // Stored Single0 can be edited while four Performance parts are
        // sounding. This grants storage ownership, never keys or voices.
        bool canTouchStoredParameters(KSStorageSlot slot)const noexcept
        {return storedDependency(slot,0) && slots_[size_t(slot)].revisions.parameters!=std::numeric_limits<uint64_t>::max();}
        bool editStoredParameters(KSStorageSlot slot,KSFilterParameters filter,bool markDirty=false)noexcept {
            if(filter.cutoff>=128 || filter.velocityDepth>=128 || !canTouchStoredParameters(slot))return false;
            auto& part=slots_[size_t(slot)];++part.revisions.parameters;part.parameters=filter;
            if(markDirty)dirty_.set(size_t(slot));return true;
        }
        bool requestStoredSingleRefresh()noexcept {
            if(layout_!=KSLayout::Performance || !slots_[0].program)return false;
            dirty_.set(0);return true;
        }
        bool touchParameters(KSStorageSlot slot,bool markDirty=true)noexcept {
            if(!canTouchParameters(slot))return false;
            ++slots_[size_t(slot)].revisions.parameters;if(markDirty)dirty_.set(size_t(slot));return true;
        }
        bool editVelocityDepth(KSStorageSlot slot,uint8_t value)noexcept {
            if(value>=128 || !touchParameters(slot))return false;
            slots_[size_t(slot)].parameters.velocityDepth=value;return true;
        }
        bool editParameters(KSStorageSlot slot,KSFilterParameters filter,bool markDirty=true)noexcept {
            if(filter.cutoff>=128 || filter.velocityDepth>=128 || !touchParameters(slot,markDirty))return false;
            slots_[size_t(slot)].parameters=filter;return true;
        }

        bool commitCutoff(KSStorageSlot slot, uint8_t value, NativeFrame frame) noexcept
        {
            if (!view_.canPublish(frame) || !editCutoff(slot, value))
                return false;
            return publish(frame);
        }

        std::optional<Part> part(KSStorageSlot slot) const noexcept
        {
            const auto index = size_t(slot);
            if (index >= StorageSlotCount || !slots_[index].program)
                return std::nullopt;
            return slots_[index];
        }

        std::optional<ParameterDependency<KSStorageSlot>> dependency(KSStorageSlot slot,
                                                                    uint64_t rateGeneration) const noexcept
        {
            if (!active(slot) || !slots_[size_t(slot)].program)
                return std::nullopt;
            return ParameterDependency<KSStorageSlot>{
                session_, slot, slots_[size_t(slot)].revisions, routingRevision_, rateGeneration};
        }

        DirtySlots dirtySlots() const noexcept { return dirty_; }

        std::optional<ParameterDependency<KSStorageSlot>> storedDependency(KSStorageSlot slot,uint64_t rateGeneration)const noexcept {
            if(!active(slot) && !(layout_==KSLayout::Performance && slot==KSStorageSlot::Single))return std::nullopt;
            if(!slots_[size_t(slot)].program)return std::nullopt;
            return ParameterDependency<KSStorageSlot>{session_,slot,slots_[size_t(slot)].revisions,routingRevision_,rateGeneration};
        }

        const KSKeyState* keys(KSStorageSlot slot) const noexcept
        {
            return active(slot) && slots_[size_t(slot)].program ? &keys_[size_t(slot)] : nullptr;
        }

        const KSSustainState* sustain(KSStorageSlot part) const noexcept
        { return keys(part) ? &sustain_[size_t(part)] : nullptr; }
        bool voiceOperationBusy() const noexcept { return voiceOperationOwner_ != nullptr; }
        bool prepareSustain(KSStorageSlot part, bool down, bool bypass) noexcept
        {
            if (view_.snapshot() || !sustain(part)) return false;
            sustain_[size_t(part)]={down,bypass,0};return true;
        }
        bool setSustainDown(KSStorageSlot part, bool down) noexcept
        {
            if (!sustain(part) || !advanceRevision(sustain_[size_t(part)].revision)) return false;
            sustain_[size_t(part)].down=down;return true;
        }
        bool setSustainBypass(KSStorageSlot part, bool bypass) noexcept
        {
            if (!sustain(part) || !advanceRevision(sustain_[size_t(part)].revision)) return false;
            sustain_[size_t(part)].bypass=bypass;return true;
        }
        std::optional<VoiceAllocation> deferredVoice(VoiceSlot slot) const noexcept
        { return size_t(slot)<VoiceCount ? deferred_[size_t(slot)] : std::nullopt; }
        bool deferVoice(VoiceAllocation allocation) noexcept
        {
            if (!voice(allocation)) return false;
            deferred_[size_t(allocation.slot)]=allocation;return true;
        }
        bool clearDeferredVoice(VoiceAllocation allocation) noexcept
        {
            if (!voice(allocation)) return false;
            auto& pending=deferred_[size_t(allocation.slot)];
            if (pending && !(*pending==allocation)) return false;
            pending.reset();return true;
        }

        // A prepared test frontier is projected once, before any publication.
        // Runtime operations never refresh this seed from a legacy controller.
        bool prepareKeys(KSStorageSlot slot, KSKeyState::Projection seed) noexcept
        {
            if (view_.snapshot() || !keys(slot))
                return false;
            const auto prepared = KSKeyState::prepare(seed);
            if (!prepared)
                return false;
            keys_[size_t(slot)] = *prepared;
            return true;
        }

        bool pressKey(KSStorageSlot slot, uint8_t note, uint8_t velocity) noexcept
        {
            return keys(slot) && keys_[size_t(slot)].press(note, velocity);
        }
        bool releaseKey(KSStorageSlot slot, uint8_t note, bool retainHeld = false) noexcept
        {
            return keys(slot) && keys_[size_t(slot)].release(note, retainHeld);
        }
        bool clearKeys(KSStorageSlot slot) noexcept
        {
            return keys(slot) && keys_[size_t(slot)].clearKeys();
        }

        // Call once when the refresh owner admits a bundle. The returned
        // selection stays with that owner while later edits set new dirty bits.
        DirtySlots takeDirtySlots() noexcept
        {
            const auto selected = dirty_;
            dirty_.reset();
            return selected;
        }

        std::optional<VoiceAllocation> assignVoice(VoiceSlot slot, KSStorageSlot part, uint8_t note,
                                                  uint8_t velocity, uint16_t feedbackAge, uint8_t noteOffExtra=0) noexcept
        {
            if (!active(part) || !slots_[size_t(part)].program)
                return std::nullopt;
            const auto allocation=voices_.assign(slot, part, note, velocity, feedbackAge);
            if (allocation) {deferred_[size_t(slot)].reset();noteOffExtra_[size_t(slot)]=noteOffExtra;}
            return allocation;
        }

        std::optional<uint8_t> noteOffExtra(VoiceAllocation allocation) const noexcept
        {return voices_.read(allocation)?std::optional{noteOffExtra_[size_t(allocation.slot)]}:std::nullopt;}

        bool canAssignVoice(VoiceSlot slot, KSStorageSlot part, uint8_t note) const noexcept
        {
            return active(part) && slots_[size_t(part)].program && voices_.canAssign(slot, note);
        }
        std::optional<uint64_t> voiceGeneration(VoiceSlot slot) const noexcept { return voices_.generation(slot); }

        bool observeVoiceFeedback(VoiceAllocation allocation, uint16_t feedbackAge) noexcept
        {
            return voices_.observeFeedback(allocation, feedbackAge);
        }

        bool commitVoiceFeedback(VoiceAllocation allocation, uint16_t feedbackAge, NativeFrame frame) noexcept
        {
            if (!view_.canPublish(frame) || !observeVoiceFeedback(allocation, feedbackAge))
                return false;
            return publish(frame);
        }

        bool retireVoice(VoiceAllocation allocation) noexcept
        {
            if (!voices_.retire(allocation)) return false;
            deferred_[size_t(allocation.slot)].reset();noteOffExtra_[size_t(allocation.slot)]=0;return true;
        }

        std::optional<Voices::Voice> voice(VoiceSlot slot) const noexcept { return voices_.read(slot); }

        std::optional<Voices::Voice> voice(VoiceAllocation allocation) const noexcept
        {
            return voices_.read(allocation);
        }

        // The P05 full-image cutoff inputs are sampled at their semantic
        // observation point. Only the pure arithmetic can reuse equal inputs.
        std::optional<KSCutoffObservation> observeCutoff(KSStorageSlot slot, VoiceAllocation allocation,
                                                        uint16_t staticBase, uint64_t rateGeneration) const noexcept
        {
            const auto source = dependency(slot, rateGeneration);
            const auto assigned = voices_.read(allocation);
            if (!source || !assigned || assigned->part != slot || !assigned->feedbackAge)
                return std::nullopt;
            return KSCutoffObservation{*source, allocation, assigned->observationRevision,
                                       {slots_[size_t(slot)].parameters, assigned->velocity, staticBase}};
        }

        // After the event has committed, publish desired patch values and the
        // latest voice observations. This does not assert DSP application.
        bool publish(NativeFrame frame) noexcept
        {
            std::array<KSKeyState::Projection, StorageSlotCount> keys;
            for (size_t i = 0; i < keys.size(); ++i)
                keys[i] = keys_[i].projection();
            return view_.publish(frame, {layout_, slots_, dirty_, voices_.projection(), keys, sustain_, deferred_,noteOffExtra_});
        }
        bool canPublish(NativeFrame frame) const noexcept { return view_.canPublish(frame); }
        std::optional<View::Snapshot> snapshot() const noexcept { return view_.snapshot(); }

    private:
        bool active(KSStorageSlot slot) const noexcept
        {
            return layout_ == KSLayout::Single ? slot == KSStorageSlot::Single :
                slot != KSStorageSlot::Single && size_t(slot) < StorageSlotCount;
        }

        Session session_;
        KSLayout layout_;
        uint64_t routingRevision_ = 0;
        const void* voiceOperationOwner_ = nullptr;
        uint64_t voiceOperationRevision_ = 0;
        std::array<Part, StorageSlotCount> slots_{};
        DirtySlots dirty_;
        Voices voices_;
        std::array<KSKeyState, StorageSlotCount> keys_;
        std::array<KSSustainState, StorageSlotCount> sustain_{};
        DeferredVoices deferred_{};
        std::array<uint8_t,VoiceCount> noteOffExtra_{};
        View view_;
    };
}
