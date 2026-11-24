#pragma once

#include "../../../controller/command/controllerStateView.h"

namespace novation::controller::command
{
    // A complete numerical snapshot. The selected patch applies to the whole
    // supplied cohort. Curve values are captured typed inputs, not an assumed
    // immutable ROM table. The same three controls feed depths and globals.
    struct KExpressionSource
    {
        std::array<uint8_t, 256> patch{};
        std::array<int16_t, 65> modulationCurve{};
        std::array<uint8_t, 3> controls{};
    };

    std::array<uint32_t,4> compileKModulationDepths(const KExpressionSource& source) noexcept;

    struct KExpressionVoice
    {
        VoiceAllocation allocation{};
        uint16_t age = 0;
    };

    struct KExpressionWrite
    {
        Endpoint endpoint = Endpoint::A;
        uint16_t address = 0;
        uint32_t word = 0;
    };

    class KExpressionPacket final
    {
    public:
        static constexpr size_t VoiceCount = 8;
        static constexpr size_t Capacity = 4 * VoiceCount + 3;
        size_t size() const noexcept { return size_; }
        const KExpressionWrite* at(size_t index) const noexcept
        { return index < size_ ? &writes_[index] : nullptr; }
        Session session() const noexcept { return session_; }
        const KExpressionSource& source() const noexcept { return source_; }
        size_t voiceCount() const noexcept { return voiceCount_; }
        const KExpressionVoice* voice(size_t index) const noexcept
        { return index < voiceCount_ ? &voices_[index] : nullptr; }
    private:
        friend std::optional<KExpressionPacket> compileKModWheelPacket(
            Session, const KExpressionSource&, const KExpressionVoice*, size_t) noexcept;
        Session session_{};
        KExpressionSource source_{};
        std::array<KExpressionVoice, VoiceCount> voices_{};
        std::array<KExpressionWrite, Capacity> writes_{};
        size_t size_ = 0, voiceCount_ = 0;
    };

    // Ascending unique allocation slots, all from this session and selected
    // source patch. Age zero skips that voice's four depth writes, while the
    // three global expression words remain mandatory. This is the CC1 partial
    // packet language, never a replacement full parameter image or timer job.
    // Raw MIDI validation/conversion and allocation/current-source validation
    // belong to the caller. Source bytes preserve the full fixed-width domain.
    std::optional<KExpressionPacket> compileKModWheelPacket(
        Session, const KExpressionSource&, const KExpressionVoice*, size_t) noexcept;
}
