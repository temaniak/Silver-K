#pragma once

#include "ksCommandGlobals.h"
#include "ksRefreshPacket.h"

namespace novation::controller::command {

// Render the native 24-bit mixer coefficients on the controller worker.
// Saved patch data and MIDI continue to hold the original seven-bit targets.
class KSLiveMixer final {
public:
    static constexpr unsigned PartLevel = 179;
    static constexpr unsigned FirstVoiceLevel = 37;
    static constexpr unsigned OscillatorCount = 3;
    static constexpr unsigned VoiceLevelCount = 5;

    static constexpr bool supports(unsigned field) noexcept {
        return field == PartLevel || (field >= FirstVoiceLevel && field < FirstVoiceLevel + VoiceLevelCount);
    }

    void route(KSStorageSlot slot, KSPart part, uint64_t revision, uint8_t output,
               uint8_t previous, uint8_t target, NativeFrame now) noexcept {
        scheduleFirstUpdate(now);
        const auto from = encodeKsPartRoute(previous, output);
        const auto to = encodeKsPartRoute(target, output);
        for (unsigned side = 0; side < 2; ++side) {
            const auto index = size_t(part) * 2 + side;
            retarget(index, slot, revision, std::nullopt, from[side + 1], to[side + 1], now);
        }
    }

    void voiceLevel(KSStorageSlot slot, VoiceAllocation voice, uint64_t revision, unsigned lane,
                    uint8_t previous, uint8_t target, NativeFrame now) noexcept {
        scheduleFirstUpdate(now);
        const auto index = RouteCount + size_t(voice.slot) * VoiceLevelCount + lane;
        retarget(index, slot, revision, voice, uint32_t(previous) << 14, uint32_t(target) << 14, now);
    }

    void cancelRoute(KSPart part) noexcept {
        active_.reset(size_t(part) * 2);
        active_.reset(size_t(part) * 2 + 1);
    }

    void cancelPart(KSStorageSlot part) noexcept {
        for (size_t i = 0; i < lanes_.size(); ++i)
            if (active_.test(i) && lanes_[i].part == part)
                active_.reset(i);
    }

    bool active() const noexcept { return active_.any(); }
    bool due(NativeFrame now) const noexcept { return active() && !(now < next_); }

    void append(KSRefreshPacket& packet, const KSMusicalState& state, NativeFrame now) noexcept {
        next_ = {now.ticks + UpdateFrames};
        for (size_t i = 0; i < lanes_.size(); ++i) {
            if (!active_.test(i) || !current(i, state))
                continue;
            auto& ramp = lanes_[i];
            const auto elapsed = std::min(now.ticks - ramp.start.ticks, RampFrames);
            const auto delta = int64_t(ramp.target) - ramp.from;
            const auto value = uint32_t(int64_t(ramp.from) + delta * int64_t(elapsed) / int64_t(RampFrames));
            if (value != ramp.applied) {
                const bool output = i < RouteCount;
                packet.appendWord(output ? Endpoint::A : Endpoint::B, output ? routeAddress(i) : voiceAddress(i), value);
                ramp.applied = value;
            }
            if (elapsed == RampFrames)
                active_.reset(i);
        }
    }

    uint32_t preserve(Endpoint endpoint, uint16_t address, uint32_t target, const KSMusicalState& state) noexcept {
        const auto index = laneFor(endpoint, address);
        if (!index || !active_.test(*index) || !current(*index, state))
            return target;
        if (target == lanes_[*index].target)
            return lanes_[*index].applied;
        // A route, mixer gate or program can supersede the old coefficient.
        active_.reset(*index);
        return target;
    }

private:
    static constexpr size_t RouteCount = KSMusicalState::PartCount * 2;
    static constexpr size_t LaneCount = RouteCount + KSMusicalState::VoiceCount * VoiceLevelCount;
    static_assert(LaneCount <= KSRefreshPacket::MaxWrites && LaneCount <= KSRefreshPacket::MaxWords);
    static constexpr uint16_t RouteBase = KSPartOutputAddress + 1;
    static constexpr uint16_t VoiceStride = 101;
    // Voice-image base 0x2c + the interleaved mixer gain/modulation offsets.
    static constexpr uint16_t FirstVoiceAddress = 0x51;
    static constexpr std::array<uint16_t, VoiceLevelCount> VoiceAddresses{0x55, 0x53, 0x51, 0x58, 0x5a};
    // KS native audio runs at about 45.34 kHz.
    static constexpr uint64_t UpdateFrames = 23, RampFrames = 368;

    static constexpr uint16_t routeAddress(size_t index) noexcept {
        return uint16_t(RouteBase + KSGlobalPartStride * (index / 2) + index % 2);
    }

    static constexpr uint16_t voiceAddress(size_t index) noexcept {
        const auto offset = index - RouteCount;
        return uint16_t(VoiceAddresses[offset % VoiceLevelCount] + VoiceStride * (offset / VoiceLevelCount));
    }

    static std::optional<size_t> laneFor(Endpoint endpoint, uint16_t address) noexcept {
        if (endpoint == Endpoint::A) {
            if (address < RouteBase) return {};
            const auto offset = unsigned(address - RouteBase);
            const auto part = offset / KSGlobalPartStride;
            const auto side = offset % KSGlobalPartStride;
            if (part < KSMusicalState::PartCount && side < 2) return part * 2 + side;
        } else if (address >= FirstVoiceAddress) {
            const auto offset = unsigned(address - FirstVoiceAddress);
            const auto voice = offset / VoiceStride;
            const auto word = FirstVoiceAddress + offset % VoiceStride;
            if (voice < KSMusicalState::VoiceCount && word <= VoiceAddresses.back())
                for (size_t lane = 0; lane < VoiceAddresses.size(); ++lane)
                    if (VoiceAddresses[lane] == word)
                        return RouteCount + voice * VoiceLevelCount + lane;
        }
        return {};
    }

    struct Ramp {
        KSStorageSlot part = KSStorageSlot::Single;
        uint64_t program = 0;
        std::optional<VoiceAllocation> voice;
        NativeFrame start{};
        uint32_t from = 0, target = 0, applied = 0;
    };

    void scheduleFirstUpdate(NativeFrame now) noexcept {
        if (!active()) next_ = {now.ticks + UpdateFrames};
    }

    void retarget(size_t index, KSStorageSlot part, uint64_t program, std::optional<VoiceAllocation> voice,
                  uint32_t previous, uint32_t target, NativeFrame now) noexcept {
        auto& ramp = lanes_[index];
        const bool continuing = active_.test(index) && ramp.part == part && ramp.program == program &&
                                ramp.voice == voice;
        if (continuing && ramp.target == target)
            return;
        const auto from = continuing ? ramp.applied : previous;
        ramp = {part, program, voice, now, from, target, from};
        active_.set(index, from != target);
    }

    bool current(size_t index, const KSMusicalState& state) noexcept {
        const auto& ramp = lanes_[index];
        const auto part = state.part(ramp.part);
        const bool sameLayout = (state.layout() == KSLayout::Single) == (ramp.part == KSStorageSlot::Single);
        if (sameLayout && part && part->revisions.program == ramp.program &&
            (!ramp.voice || state.voice(*ramp.voice)))
            return true;
        active_.reset(index);
        return false;
    }

    std::array<Ramp, LaneCount> lanes_{};
    std::bitset<LaneCount> active_;
    NativeFrame next_{};
};

}
