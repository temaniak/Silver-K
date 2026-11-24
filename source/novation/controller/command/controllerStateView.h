#pragma once

#include "controllerEvent.h"

#include <array>
#include <limits>
#include <optional>

namespace novation::controller::command
{
    // All revisions are scoped to a Session. Exhaustion must fault or replace
    // the session before another commit can reuse an identity.
    inline constexpr bool advanceRevision(uint64_t& revision) noexcept
    {
        if (revision == std::numeric_limits<uint64_t>::max())
            return false;
        ++revision;
        return true;
    }

    struct ProgramSelection
    {
        uint8_t bank, program;
    };

    struct PartRevisions
    {
        uint64_t program = 0;
        uint64_t parameters = 0;

        friend constexpr bool operator==(PartRevisions left, PartRevisions right) noexcept
        {
            return left.program == right.program && left.parameters == right.parameters;
        }
    };

    template<typename Parameters>
    struct PartState
    {
        std::optional<ProgramSelection> program;
        Parameters parameters{};
        PartRevisions revisions;
    };

    template<typename Part>
    struct ParameterDependency
    {
        Session session;
        Part part;
        PartRevisions revisions;
        uint64_t routingRevision;
        uint64_t rateGeneration;

        friend constexpr bool operator==(const ParameterDependency& left,
                                         const ParameterDependency& right) noexcept
        {
            return left.session == right.session && left.part == right.part &&
                left.revisions == right.revisions && left.routingRevision == right.routingRevision &&
                left.rateGeneration == right.rateGeneration;
        }
    };

    enum class VoiceSlot : uint8_t {};

    struct VoiceAllocation
    {
        Session session;
        VoiceSlot slot;
        uint64_t generation;

        friend constexpr bool operator==(VoiceAllocation left, VoiceAllocation right) noexcept
        {
            return left.session == right.session && left.slot == right.slot &&
                left.generation == right.generation;
        }
    };

    template<typename Part>
    struct AllocatedVoice
    {
        VoiceAllocation allocation;
        Part part;
        uint8_t note;
        uint8_t velocity;
        uint16_t feedbackAge;
        uint64_t observationRevision;
    };

    // Physical slot storage and ownership checks. Each model decides when to
    // assign or retire a voice and how feedback affects its eligibility.
    template<typename Part, size_t Count>
    class VoiceAllocations
    {
    public:
        using Voice = AllocatedVoice<Part>;
        using Projection = std::array<std::optional<Voice>, Count>;

        explicit VoiceAllocations(Session session) noexcept : session_(session) {}

        std::optional<uint64_t> generation(VoiceSlot slot) const noexcept
        {
            return size_t(slot) < Count ? std::optional{generations_[size_t(slot)]} : std::nullopt;
        }

        bool canAssign(VoiceSlot slot, uint8_t note) const noexcept
        {
            return size_t(slot) < Count && note < 128 &&
                generations_[size_t(slot)] != std::numeric_limits<uint64_t>::max();
        }

        std::optional<VoiceAllocation> assign(VoiceSlot slot, Part part, uint8_t note, uint8_t velocity,
                                             uint16_t feedbackAge) noexcept
        {
            const auto index = size_t(slot);
            if (!canAssign(slot, note) || !advanceRevision(generations_[index]))
                return std::nullopt;
            const VoiceAllocation allocation{session_, slot, generations_[index]};
            voices_[index] = Voice{allocation, part, note, velocity, feedbackAge, 0};
            return allocation;
        }

        bool observeFeedback(VoiceAllocation allocation, uint16_t feedbackAge) noexcept
        {
            if (!owns(allocation))
                return false;
            auto& voice = *voices_[size_t(allocation.slot)];
            if (!advanceRevision(voice.observationRevision))
                return false;
            voice.feedbackAge = feedbackAge;
            return true;
        }

        bool retire(VoiceAllocation allocation) noexcept
        {
            if (!owns(allocation))
                return false;
            voices_[size_t(allocation.slot)].reset();
            return true;
        }

        std::optional<Voice> read(VoiceAllocation allocation) const noexcept
        {
            return owns(allocation) ? voices_[size_t(allocation.slot)] : std::nullopt;
        }

        std::optional<Voice> read(VoiceSlot slot) const noexcept
        {
            const auto index = size_t(slot);
            return index < Count ? voices_[index] : std::nullopt;
        }

        Projection projection() const noexcept { return voices_; }

    private:
        bool owns(VoiceAllocation allocation) const noexcept
        {
            const auto index = size_t(allocation.slot);
            return index < Count && voices_[index] && voices_[index]->allocation == allocation;
        }

        Session session_;
        std::array<uint64_t, Count> generations_{};
        Projection voices_{};
    };

    template<typename Projection>
    class ControllerStateView
    {
    public:
        struct Snapshot
        {
            Session session;
            NativeFrame frame;
            uint64_t revision;
            Projection state;
        };

        explicit ControllerStateView(Session session) noexcept : session_(session) {}

        bool canPublish(NativeFrame frame) const noexcept
        {
            return !published_ || (!(frame < published_->frame) &&
                published_->revision != std::numeric_limits<uint64_t>::max());
        }

        // The worker publishes after a complete semantic commit, at its
        // monotonic native frontier. DSP admission and observation are separate.
        bool publish(NativeFrame frame, const Projection& projection) noexcept
        {
            if (!canPublish(frame))
                return false;
            const auto revision = published_ ? published_->revision + 1 : 1;
            published_ = Snapshot{session_, frame, revision, projection};
            return true;
        }

        // Same-worker query. The returned value owns its projection.
        // Synchronization for a concurrent host reader belongs to the binding.
        std::optional<Snapshot> snapshot() const noexcept { return published_; }

    private:
        Session session_;
        std::optional<Snapshot> published_;
    };
}
