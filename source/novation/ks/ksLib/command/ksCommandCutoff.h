#pragma once

#include "ksMusicalState.h"
#include "../../../controller/command/commandSession.h"

namespace novation::controller::command
{
    // The tuple holds the bytes sampled at voice-image construction. The two
    // modulo operations model the verified 16-bit sum and 32-bit accumulator.
    constexpr uint32_t encodeKsCutoff(KSCutoffInputs inputs) noexcept
    {
        const auto centered = [](uint8_t byte) constexpr -> int32_t
        {
            const auto bits = uint8_t(int32_t(byte) - 64);
            return bits < 0x80 ? int32_t(bits) : int32_t(bits) - 0x100;
        };
        const auto sum = uint16_t(int32_t(inputs.filter.cutoff) * 128 +
            2 * centered(inputs.velocity) * centered(inputs.filter.velocityDepth));
        const uint32_t bounded = (sum & 0x8000u) ? 0u : (sum > 0x3f80u ? 0x3f80u : sum);
        const uint32_t accumulator = bounded * 2u * 0xaaabu - 0x48001400u +
            (uint32_t(inputs.staticBase) << 16);
        return accumulator >> 8;
    }

    struct KSCutoffWord
    {
        static constexpr uint16_t SlotZeroAddress = 0x005f;
        KSCutoffObservation observation;
        uint32_t word;
    };

    struct KSCutoffMetrics
    {
        uint64_t edits = 0;
        uint64_t directCalculations = 0;
        uint64_t reusedCalculations = 0;
    };

    // P05 CC105 building block. The edit commits at MIDI dispatch.
    // Image construction samples the tuple later, at its own admitted deadline.
    class KSCommandCutoff final
    {
    public:
        static constexpr uint8_t NoChannel = 16;
        using Channels = std::array<uint8_t, KSMusicalState::StorageSlotCount>;

        KSCommandCutoff(KSMusicalState& state, uint8_t channel) noexcept
            : KSCommandCutoff(state, Channels{channel, NoChannel, NoChannel, NoChannel, NoChannel}) {}
        KSCommandCutoff(KSMusicalState& state, Channels channels) noexcept : state_(state), channels_(channels) {}

        std::optional<KSStorageSlot> target(uint8_t channel, uint8_t part = RoutedByChannel) const noexcept
        {
            if (channel >= NoChannel)
                return std::nullopt;
            const auto count = state_.layout() == KSLayout::Single ? size_t(1) : KSMusicalState::PartCount;
            for (size_t i = 0; i < count; ++i)
            {
                const auto slot = KSMusicalState::storageSlot(state_.layout(), KSPart(i));
                if ((part == RoutedByChannel || part == i) && channels_[size_t(*slot)] == channel)
                    return slot;
            }
            return std::nullopt;
        }

        MusicalDispatch dispatcher() noexcept
        {
            return {this, [](void* context, CommandSession&, const ScheduledEvent& event, NativeFrame now) noexcept
            {
                return static_cast<KSCommandCutoff*>(context)->dispatch(event, now);
            }};
        }

        std::optional<KSCutoffWord> observeVoice(KSStorageSlot slot, VoiceAllocation voice, uint16_t staticBase,
                                                 uint64_t rateGeneration) noexcept
        {
            const auto observed = state_.observeCutoff(slot, voice, staticBase, rateGeneration);
            if (!observed)
                return std::nullopt;
            auto& cache = cache_[size_t(voice.slot)];
            if (cache && cache->input == observed->inputs)
                ++metrics_.reusedCalculations;
            else
            {
                cache = Cache{observed->inputs, encodeKsCutoff(observed->inputs)};
                ++metrics_.directCalculations;
            }
            return KSCutoffWord{*observed, cache->word};
        }

        KSCutoffMetrics metrics() const noexcept { return metrics_; }

        // Routing has already selected a current logical part. This avoids
        // consulting the constructor's cold channel map after a channel edit.
        DispatchDecision applyPart(KSStorageSlot part, uint8_t value, NativeFrame now) noexcept
        {
            if (!state_.commitCutoff(part, value, now))
                return {DispatchDecision::Status::Fault, {}};
            ++metrics_.edits;
            return {};
        }

    private:
        DispatchDecision dispatch(const ScheduledEvent& event, NativeFrame now) noexcept
        {
            const auto* cc = std::get_if<ControlChange>(&event.event.payload);
            const auto count = state_.layout() == KSLayout::Single ? size_t(1) : KSMusicalState::PartCount;
            if (!cc || cc->controller != 105 || event.session != state_.session() ||
                (event.event.part != RoutedByChannel && event.event.part >= count))
                return {DispatchDecision::Status::Fault, {}};
            const auto slot = target(cc->channel, event.event.part);
            if (!slot)
                return {};
            if (!state_.commitCutoff(*slot, cc->value, now))
                return {DispatchDecision::Status::Fault, {}};
            ++metrics_.edits;
            return {};
        }

        struct Cache
        {
            KSCutoffInputs input;
            uint32_t word;
        };

        KSMusicalState& state_;
        Channels channels_;
        std::array<std::optional<Cache>, KSMusicalState::VoiceCount> cache_{};
        KSCutoffMetrics metrics_;
    };
}
