#pragma once
#include <cstdint>

namespace ks {
// The signed KS parameter encoding: ten mantissa bits and a five-bit exponent.
// Native values and durations have no CPU register/status/stack/code address.
struct PreparedParameterScale {
    uint32_t mantissa;
    int shift;
    bool negative;
    unsigned clocks,boundaries;
};
struct ParameterScale {
    int32_t coefficient;
    unsigned clocks,boundaries; // Body only; return/transport costs are separate.
};
constexpr PreparedParameterScale prepareParameterScale(int16_t encoded){
    const bool negative=encoded<0;
    // The original saturates the magnitude of -32768 before decoding.
    const unsigned magnitude=negative?(encoded==-32768?32767u:unsigned(-int(encoded))):unsigned(encoded);
    return {0x400u|(magnitude&0x3ffu),int(magnitude>>10)-11,negative,
        30u+(negative?12u:0u)+(encoded==-32768?3u:0u),
        10u+(negative?3u:0u)+(encoded==-32768?1u:0u)};
}
constexpr ParameterScale decodeParameterScale(int16_t encoded){
    const auto p=prepareParameterScale(encoded);
    const uint32_t scaled=p.shift>=0?p.mantissa<<unsigned(p.shift):p.mantissa>>unsigned(-p.shift);
    const unsigned iterations=unsigned(p.shift<0?-p.shift:p.shift);
    const unsigned extra=p.shift?3u+9u*iterations+(p.shift>0?3u:0u):0u;
    const unsigned extraSteps=p.shift?1u+3u*iterations+(p.shift>0?1u:0u):0u;
    return {p.negative?-int32_t(scaled):int32_t(scaled),
        p.clocks+6u+(p.negative?3u:0u)+extra,
        p.boundaries+2u+(p.negative?1u:0u)+extraSteps};
}

// A native calculation plus its externally visible completion time. Arithmetic
// is direct, not driven by code addresses, CPU flags, registers or stack data.
// A scheduler may park this task for an interrupt and resume it unchanged.
// Costs describe the independently admitted 25 MHz control timeline; there is
// one six-clock admission interval for a negative input, all others cost three.
class ParameterScaleTask final {
public:
    struct Advance {unsigned clocks=0,boundaries=0;};
    explicit constexpr ParameterScaleTask(int16_t encoded)
        : result_(decodeParameterScale(encoded)),negative_(encoded<0) {}
    constexpr bool complete()const{return completed_==result_.boundaries;}
    constexpr unsigned elapsedClocks()const{return elapsed_;}
    constexpr unsigned completedBoundaries()const{return completed_;}
    constexpr unsigned remainingClocks()const{return result_.clocks-elapsed_;}
    constexpr unsigned nextBoundaryClocks()const{
        return complete()?0u:(negative_ && completed_==3?6u:3u);
    }
    // No boundary is crossed unless all of its clocks fit. A zero result is a
    // request to yield, never a spin/wait. The scheduler owns interrupt time.
    constexpr Advance advanceBefore(unsigned available){
        const unsigned before=completed_,start=elapsed_;
        if(negative_ && completed_<=3){
            const unsigned prefix=3-completed_;
            const unsigned take=available/3<prefix?available/3:prefix;
            completed_+=take;elapsed_+=3*take;available-=3*take;
            if(completed_==3){
                if(available<6)return {elapsed_-start,completed_-before};
                ++completed_;elapsed_+=6;available-=6;
            }
        }
        const unsigned remaining=result_.boundaries-completed_;
        const unsigned take=available/3<remaining?available/3:remaining;
        completed_+=take;elapsed_+=3*take;
        return {elapsed_-start,completed_-before};
    }
    // Commit only after complete(). The value has no compatibility-frame ABI.
    constexpr ParameterScale result()const{return result_;}
private:
    ParameterScale result_;
    unsigned completed_=0,elapsed_=0;
    bool negative_;
};
}
