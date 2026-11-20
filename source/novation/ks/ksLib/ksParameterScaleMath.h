#pragma once

#include <cstdint>

namespace ks
{
    struct ParameterScaleBits
    {
        uint32_t mantissa;
        int shift;
        bool negative;
    };

    constexpr ParameterScaleBits parameterScaleBits(int16_t encoded) noexcept
    {
        const bool negative = encoded < 0;
        const unsigned magnitude = negative ?
            (encoded == -32768 ? 32767u : unsigned(-int(encoded))) : unsigned(encoded);
        return {0x400u | (magnitude & 0x3ffu), int(magnitude >> 10) - 11, negative};
    }

    constexpr int32_t decodeParameterCoefficient(ParameterScaleBits bits) noexcept
    {
        const auto scaled = bits.shift >= 0 ? bits.mantissa << unsigned(bits.shift) :
            bits.mantissa >> unsigned(-bits.shift);
        return bits.negative ? -int32_t(scaled) : int32_t(scaled);
    }

    constexpr int32_t decodeParameterCoefficient(int16_t encoded) noexcept
    {
        return decodeParameterCoefficient(parameterScaleBits(encoded));
    }
}
