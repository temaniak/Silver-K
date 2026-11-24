#pragma once

#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>

namespace novation::controller::command
{
    enum class TimeDomain { Host, Native, DspA, DspB, Bus };

    template<TimeDomain Domain>
    struct TimePoint
    {
        uint64_t ticks = 0;

        friend constexpr bool operator==(TimePoint left, TimePoint right) noexcept { return left.ticks == right.ticks; }
        friend constexpr bool operator!=(TimePoint left, TimePoint right) noexcept { return !(left == right); }
        friend constexpr bool operator<(TimePoint left, TimePoint right) noexcept { return left.ticks < right.ticks; }
    };

    using HostFrame = TimePoint<TimeDomain::Host>;
    using NativeFrame = TimePoint<TimeDomain::Native>;
    using DspACycle = TimePoint<TimeDomain::DspA>;
    using DspBCycle = TimePoint<TimeDomain::DspB>;
    using BusCycle = TimePoint<TimeDomain::Bus>;

    struct ClockRate
    {
        uint32_t numerator;
        uint32_t denominator = 1;
    };

    enum class Rounding { Down, Up };

    template<TimeDomain From, TimeDomain To>
    class TimeMap
    {
    public:
        using Source = TimePoint<From>;
        using Destination = TimePoint<To>;

        // The reduced factors fit 32 bits for the supported K/KS rates. This
        // bounds the remainder product while allowing full-width timestamps.
        static std::optional<TimeMap> make(Source source, Destination destination,
                                           ClockRate sourceRate, ClockRate destinationRate) noexcept
        {
            if (!sourceRate.numerator || !sourceRate.denominator ||
                !destinationRate.numerator || !destinationRate.denominator)
                return std::nullopt;
            auto numerator = uint64_t(destinationRate.numerator) * sourceRate.denominator;
            auto denominator = uint64_t(sourceRate.numerator) * destinationRate.denominator;
            const auto common = std::gcd(numerator, denominator);
            numerator /= common;
            denominator /= common;
            constexpr auto maximum = std::numeric_limits<uint32_t>::max();
            if (numerator > maximum || denominator > maximum)
                return std::nullopt;
            return TimeMap(source, destination, uint32_t(numerator), uint32_t(denominator));
        }

        std::optional<Destination> map(Source source, Rounding rounding = Rounding::Down) const noexcept
        {
            if (source < sourceOrigin_)
                return std::nullopt;
            const auto elapsed = source.ticks - sourceOrigin_.ticks;
            const auto whole = elapsed / denominator_;
            const auto remainder = (elapsed % denominator_) * numerator_;
            const auto fraction = remainder / denominator_ +
                (rounding == Rounding::Up && remainder % denominator_ != 0);
            const auto remaining = std::numeric_limits<uint64_t>::max() - destinationOrigin_.ticks;
            if (fraction > remaining || whole > (remaining - fraction) / numerator_)
                return std::nullopt;
            return Destination{destinationOrigin_.ticks + whole * numerator_ + fraction};
        }

        // A new host rate starts at the exact worker frontier. Deadlines already
        // admitted in the destination domain retain their existing timestamps.
        std::optional<TimeMap> rebase(Source source, ClockRate sourceRate,
                                     ClockRate destinationRate) const noexcept
        {
            const auto destination = map(source);
            if (!destination)
                return std::nullopt;
            return make(source, *destination, sourceRate, destinationRate);
        }

    private:
        TimeMap(Source source, Destination destination, uint32_t numerator, uint32_t denominator) noexcept
            : sourceOrigin_(source), destinationOrigin_(destination), numerator_(numerator), denominator_(denominator) {}

        Source sourceOrigin_;
        Destination destinationOrigin_;
        uint32_t numerator_;
        uint32_t denominator_;
    };
}
