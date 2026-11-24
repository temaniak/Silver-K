#pragma once

#include "commandSession.h"
#include "nativeMusicalClock.h"

namespace novation::controller::command {
    struct DspClockWords {
        uint32_t phase;
        uint32_t delay;

        friend constexpr bool operator==(DspClockWords left, DspClockWords right) noexcept {
            return left.phase == right.phase && left.delay == right.delay;
        }
    };

    // The same fixed point format is used by the K and KS DSP firmware.
    // Address 3 advances the modulation phase, address 4 scales delay time
    // in the opposite direction. The note/FX images supply the division.
    inline std::optional<DspClockWords> encodeDspClock(ClockRate rate, double framesPerPulse) noexcept {
        if (!rate.numerator || !rate.denominator || !std::isfinite(framesPerPulse) || framesPerPulse < 1)
            return {};

        constexpr uint32_t positiveWord = 0x7fffff;
        constexpr double phaseIncrementPerBpm = 32 * 0xd1b7;
        const double bpm = double(rate.numerator) * 60 / (24.0 * rate.denominator * framesPerPulse);
        const auto increment = uint32_t(std::llround(std::clamp(bpm * phaseIncrementPerBpm, 4096.0,
                                                                double(positiveWord) * 64)));
        const uint32_t phase = (increment >> 12) << 6;
        uint32_t divisor = increment << 2;
        uint64_t resolution = uint64_t(1) << 26;
        for (unsigned i = 0; i < 5 && divisor < 0x80000000; ++i) {
            divisor <<= 1;
            resolution <<= 1;
        }
        const auto delay = (resolution << 8) / (divisor >> 16);
        return DspClockWords{phase, uint32_t(std::min<uint64_t>(positiveWord, delay))};
    }

    class DspMusicalClock final {
    public:
        DspMusicalClock(Model model, ClockRate rate) noexcept : model_(model), rate_(rate) {}

        bool observe(const NativeMusicalClock& clock, bool freshMidiPeriod = false) noexcept {
            const auto source = clock.source();
            if (source != ClockAuthority::Midi)
                return observe(source, clock.framesPerPulse());
            if (!clock.measuredMidiPeriod())
                resetMidiObservation();
            return freshMidiPeriod && observe(source, clock.framesPerPulse());
        }

        bool observe(ClockAuthority source, double framesPerPulse) noexcept {
            if (!std::isfinite(framesPerPulse) || framesPerPulse < 1)
                return false;
            if (source == ClockAuthority::Midi) {
                const auto measured = framesPerPulse;
                if (midiHistory_) {
                    const auto older = (*midiHistory_)[0];
                    const auto recent = (*midiHistory_)[1];
                    // Serial MIDI can delay one F8 and shorten the next interval.
                    // Use the median for coefficients; arp still uses the real pulse.
                    framesPerPulse = std::clamp(measured, std::min(older, recent), std::max(older, recent));
                    midiHistory_ = std::array{recent, measured};
                } else {
                    midiHistory_ = std::array{measured, measured};
                }
            } else {
                midiHistory_.reset();
            }
            if (lastSource_ == source && desired_) {
                const double tolerance = source == ClockAuthority::Midi ? 1.0 : 0.0;
                if (std::abs(framesPerPulse - lastPeriod_) <= tolerance)
                    return false;
            }
            const auto words = encodeDspClock(rate_, framesPerPulse);
            if (!words)
                return false;
            lastPeriod_ = framesPerPulse;
            lastSource_ = source;
            const bool changed = !desired_ || !(*words == *desired_);
            desired_ = words;
            return changed;
        }

        void resetMidiObservation() noexcept { midiHistory_.reset(); }

        bool pending() const noexcept { return desired_ && (!applied_ || !(*applied_ == *desired_)); }
        std::optional<DspClockWords> applied() const noexcept { return applied_; }
        std::optional<DspTicket> ticket() const noexcept {
            return active_ ? std::optional<DspTicket>{tickets_[next_]} : std::nullopt;
        }

        DspAdmission admit(CommandSession& session, Session owner, uint64_t operation, NativeFrame at) noexcept {
            if (active_ || !pending())
                return DspAdmission::StaleOperation;
            if (session.model() != model_)
                return DspAdmission::InvalidDraft;

            const auto words = *desired_;
            std::array<DspTransactionDraft, 3> drafts{};
            size_t count = 0;
            const auto append = [&](Endpoint endpoint, uint16_t address, const uint32_t& word) noexcept {
                auto& draft = drafts[count];
                draft.model = model_;
                draft.endpoint = endpoint;
                draft.earliest = at;
                draft.address = {DspAddressMode::Write, address};
                draft.words = &word;
                draft.wordCount = 1;
                if (count)
                    draft.dependency.earlierDraft = count - 1;
                ++count;
            };
            if (!applied_ || words.delay != applied_->delay)
                append(Endpoint::A, 4, words.delay);
            if (!applied_ || words.phase != applied_->phase) {
                if (model_ == Model::KS)
                    append(Endpoint::B, 3, words.phase);
                append(Endpoint::A, 3, words.phase);
            }
            return session.transactions().submit(owner, operation, drafts.data(), count, tickets_.data(), [&]() noexcept {
                active_ = words;
                count_ = count;
                next_ = 0;
            });
        }

        bool transferred(CommandSession& session, DspTicket completed) noexcept {
            const auto current = ticket();
            DspTransactionProgress progress;
            if (!current || completed.session != current->session || completed.sequence != current->sequence ||
                completed.endpoint != current->endpoint || completed.slot != current->slot ||
                !session.transactions().progress(completed, progress) || progress.status != DspStatus::Transferred ||
                !session.transactions().retire(completed))
                return false;
            if (++next_ == count_) {
                applied_ = active_;
                active_.reset();
            }
            return true;
        }

    private:
        Model model_;
        ClockRate rate_;
        ClockAuthority lastSource_ = ClockAuthority::Internal;
        double lastPeriod_ = 0;
        std::optional<std::array<double, 2>> midiHistory_;
        std::optional<DspClockWords> desired_, active_, applied_;
        std::array<DspTicket, 3> tickets_{};
        size_t count_ = 0, next_ = 0;
    };
}
