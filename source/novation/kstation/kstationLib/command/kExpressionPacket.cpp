#include "kExpressionPacket.h"

namespace novation::controller::command
{
    namespace
    {
        constexpr int32_t signedByte(uint8_t value) noexcept
        { return value < 128 ? int32_t(value) : int32_t(value) - 256; }
        constexpr int32_t signedWord(uint16_t value) noexcept
        { return value < 0x8000 ? int32_t(value) : int32_t(value) - 0x10000; }

        uint32_t modulationWord(const KExpressionSource& source, size_t lane) noexcept
        {
            constexpr size_t biases[]{0x0a, 0x12, 0x1a, 0x33};
            const size_t depth = lane == 3 ? 0x35 : 0x22;
            uint16_t sum = 0;
            for (size_t i = 0; i < source.controls.size(); ++i)
            {
                const auto parameter = uint8_t(source.patch[depth + i] + 0xc0u);
                const auto product = signedByte(parameter) * signedByte(source.controls[i]);
                sum = uint16_t(sum + uint16_t(product));
            }
            const auto wide = signedWord(sum) * 512;
            const auto gain = signedWord(uint16_t(uint32_t(wide) >> 16));
            const auto base = signedByte(uint8_t(source.patch[biases[lane]] + 0xc0u));
            const auto result = uint16_t(base + gain);
            const bool negative = (result & 0x8000u) != 0;
            const auto magnitude = negative ? uint16_t(0u - result) : result;
            const size_t index = magnitude > 64 ? 64 : magnitude;
            const auto product = int32_t(source.modulationCurve[index]) * (negative ? -0x5555 : 0x5555);
            return ((uint32_t(product) << 1) >> 8) & 0xffffffu;
        }

        constexpr uint32_t expressionWord(uint8_t value) noexcept
        { return ((uint32_t(value) << 8) | uint8_t(uint32_t(value) << 1)) << 8; }
    }

    std::array<uint32_t,4> compileKModulationDepths(const KExpressionSource& source) noexcept
    {
        return {modulationWord(source,0),modulationWord(source,1),modulationWord(source,2),modulationWord(source,3)};
    }

    std::optional<KExpressionPacket> compileKModWheelPacket(
        Session session, const KExpressionSource& source, const KExpressionVoice* voices, size_t count) noexcept
    {
        if (!session.owner || !session.generation || count > KExpressionPacket::VoiceCount || (count && !voices))
            return std::nullopt;
        size_t previous = 0;
        for (size_t i = 0; i < count; ++i)
        {
            const auto& allocation = voices[i].allocation;
            const auto slot = size_t(allocation.slot);
            if (allocation.session != session || !allocation.generation || slot >= KExpressionPacket::VoiceCount ||
                (i && slot <= previous)) return std::nullopt;
            previous = slot;
        }
        KExpressionPacket packet;
        packet.session_ = session;
        packet.source_ = source;
        packet.voiceCount_ = count;
        constexpr uint16_t commands[]{0x21, 0x2c, 0x37, 0x46};
        std::array<uint32_t, 4> depths{};
        bool computed = false;
        for (size_t i = 0; i < count; ++i)
        {
            packet.voices_[i] = voices[i];
            if (!voices[i].age) continue;
            if (!computed)
            {
                depths = compileKModulationDepths(source);
                computed = true;
            }
            for (size_t lane = 0; lane < depths.size(); ++lane)
                packet.writes_[packet.size_++] = {Endpoint::A,
                    uint16_t(commands[lane] + 81 * size_t(voices[i].allocation.slot)), depths[lane]};
        }
        for (size_t i = 0; i < source.controls.size(); ++i)
            packet.writes_[packet.size_++] = {Endpoint::A, uint16_t(0x14 + i), expressionWord(source.controls[i])};
        return packet;
    }
}
