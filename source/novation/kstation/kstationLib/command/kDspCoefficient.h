#pragma once

#include <algorithm>
#include <cstdint>

namespace novation::controller::command {
// K uses a compact signed exponent/mantissa input for this coefficient.
constexpr uint32_t kDspCoefficient(uint16_t input) noexcept {
    const int32_t value=input<0x8000?input:int32_t(input)-0x10000;
    const auto magnitude=uint16_t(value<0?std::min(-value,32767):value);
    const int exponent=int(magnitude>>10)-11;
    const uint32_t mantissa=(magnitude&0x3ffu)|0x400u;
    const auto output=exponent<0?mantissa>>-exponent:mantissa<<exponent;
    return value<0?0u-output:output;
}
}
