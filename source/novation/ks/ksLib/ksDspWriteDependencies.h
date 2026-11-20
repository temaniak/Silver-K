#pragma once
#include <cstdint>
namespace ks {
// Dependencies of one HDI write on the authoritative transmit holding word.
// High/middle lanes only stage bits. Low always commits a word, even when its
// value is unchanged. ICR/CVR/IVR, status reads and every admission time remain
// ordered operations. No patch/voice/rate key or cached readiness is involved.
class DspWriteDependencies final {
public:
    enum class Action:uint8_t {Forward,AlreadyStaged};
    static constexpr uint32_t laneMask(unsigned offset)noexcept {
        return (offset&15)==10?0xff0000u:(offset&15)==12?0xff00u:0;
    }
    static constexpr Action plan(uint32_t holding,unsigned offset,uint8_t value)noexcept {
        const auto mask=laneMask(offset);
        const auto shifted=uint32_t(value)<<((offset&15)==10?16:8);
        return mask&&!(holding&0xff000000u)&&(holding&mask)==shifted?Action::AlreadyStaged:Action::Forward;
    }
};
}
