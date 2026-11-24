#pragma once

#include "../../../controller/command/nativeArpeggiator.h"
#include "../../../controller/command/nativeEditorState.h"
#include "ksUserDrumPattern.h"
#include <bitset>

namespace novation::controller::command {
    enum class KSDrumEventKind : uint8_t { NoteOn, NoteOff, Pressure };

    struct KSDrumEvent {
        static constexpr uint8_t Always = KSDrumGridEdit::MaxProbability, RepeatDecision = 255;
        uint16_t tick, tag;
        KSDrumEventKind kind;
        uint8_t note, velocity;
        uint8_t probability = Always;
    };

    struct KSDrumSequence {
        const KSDrumEvent* events = nullptr;
        uint16_t size = 0, length = 0;
        uint8_t pattern = NativeDrumPlayhead::Stopped;
    };

    struct KSDrumCatalog {
        static constexpr size_t MaxActive = 16;
        struct Track { uint16_t first, size, length; };
        std::array<Track, 33> tracks{};
        std::array<KSDrumEvent, 4608> events{};
        size_t size = 0;

        KSDrumSequence sequence(size_t index) const noexcept {
            if (index >= tracks.size() || !tracks[index].size) return {};
            const auto track = tracks[index];
            return {events.data() + track.first, track.size, track.length};
        }
    };

    struct KSUserDrumSequence {
        static constexpr uint16_t StepTicks = 384 / 4;
        static constexpr uint16_t LoopTicks = StepTicks * KSDrumGridEdit::Steps;
        std::array<KSDrumEvent, 2 * KSDrumGridEdit::Cells * KSDrumGridEdit::MaxRatchet> events{};
        uint16_t count = 0, length = LoopTicks;

        void compile(const KSUserDrumPattern& pattern) noexcept {
            count = 0;
            length = uint16_t(pattern.steps * StepTicks);
            for (uint16_t step = 0; step < pattern.steps; ++step) {
                const auto first = count;
                const auto stepTick = uint16_t(step * StepTicks);
                for (uint16_t row = 0; row < KSDrumGridEdit::Rows; ++row) {
                    const auto index = uint16_t(row * KSDrumGridEdit::Steps + step);
                    const auto velocity = pattern.velocities[index];
                    const auto probability = pattern.probabilities[index];
                    if (!velocity || !probability) continue;
                    const auto repetitions = pattern.ratchets[index];
                    const auto continuation = probability == KSDrumEvent::Always ? probability : KSDrumEvent::RepeatDecision;
                    for (uint16_t hit = 0; hit < repetitions; ++hit) {
                        const auto start = uint16_t(hit * StepTicks / repetitions);
                        const auto end = uint16_t((hit + 1) * StepTicks / repetitions);
                        const auto gate = uint16_t((end - start) * 3 / 4);
                        events[count++] = {uint16_t(stepTick + start), index, KSDrumEventKind::NoteOn,
                                           pattern.notes[row], velocity, hit ? continuation : probability};
                        events[count++] = {uint16_t(stepTick + start + gate), index, KSDrumEventKind::NoteOff,
                                           pattern.notes[row], 0, continuation};
                    }
                }
                std::sort(events.begin() + first, events.begin() + count,
                    [](const KSDrumEvent& a, const KSDrumEvent& b) {
                        if (a.tick != b.tick) return a.tick < b.tick;
                        if (a.kind != b.kind) return a.kind == KSDrumEventKind::NoteOff;
                        return a.tag < b.tag;
                    });
            }
        }

        KSDrumSequence sequence() const noexcept { return {events.data(), count, length}; }
    };

    // The 2.1.02 Drum table is a set of MIDI tracks at 384 ticks per
    // quarter. Decode running status and pair note generations at load time.
    inline std::optional<KSDrumCatalog> readKSDrumCatalog(const uint8_t* image, size_t size) noexcept {
        constexpr size_t table = 0x11a7a;
        constexpr uint32_t romBase = 0xa00000;
        KSDrumCatalog result;
        if (!image || size < table + 4 * result.tracks.size()) return {};
        const auto offset = [&](size_t index) noexcept {
            const auto* word = image + table + 4 * index;
            const auto address = (uint32_t(word[0]) << 24) | (uint32_t(word[1]) << 16) | (uint32_t(word[2]) << 8) | word[3];
            return size_t(address >= romBase ? address - romBase : address);
        };
        for (size_t index = 0; index < result.tracks.size(); ++index) {
            size_t cursor = offset(index);
            const auto end = index + 1 < result.tracks.size() ? offset(index + 1) : size;
            if (cursor < table + 4 * result.tracks.size() || cursor >= end || end > size) return {};
            auto& track = result.tracks[index];
            track.first = uint16_t(result.size);
            struct Active { uint8_t note; uint16_t tag; };
            std::array<Active, KSDrumCatalog::MaxActive> active{};
            size_t held = 0;
            uint8_t running = 0;
            uint32_t tick = 0;
            for (;;) {
                uint32_t delta = 0;
                uint8_t byte = 0;
                for (unsigned i = 0; i < 4; ++i) {
                    if (cursor >= end) return {};
                    byte = image[cursor++];
                    delta = (delta << 7) | (byte & 127);
                    if (!(byte & 128)) break;
                }
                if ((byte & 128) || delta > UINT16_MAX - tick || cursor >= end) return {};
                tick += delta;

                if (image[cursor] == 0xff) {
                    if (end - cursor < 3 || image[cursor + 1] != 0x2f || image[cursor + 2] != 0 ||
                        held || !tick || !track.size)
                        return {};
                    track.length = uint16_t(tick);
                    break;
                }
                if (image[cursor] & 128) running = image[cursor++] & 0xf0;
                if ((running != 0x80 && running != 0x90 && running != 0xd0) || cursor >= end ||
                    (image[cursor] & 128) || result.size == result.events.size())
                    return {};
                const uint8_t first = image[cursor++];
                auto kind = KSDrumEventKind::Pressure;
                uint8_t pitch = 0, value = first;
                uint16_t tag = track.size;
                if (running != 0xd0) {
                    if (cursor >= end || (image[cursor] & 128)) return {};
                    pitch = first;
                    value = image[cursor++];
                    if (running == 0x90 && value) {
                        if (held == active.size()) return {};
                        kind = KSDrumEventKind::NoteOn;
                        active[held++] = {pitch, tag};
                    } else {
                        kind = KSDrumEventKind::NoteOff;
                        const auto match = std::find_if(active.begin(), active.begin() + held,
                            [&](const Active& note) { return note.note == pitch; });
                        if (match == active.begin() + held) return {};
                        tag = match->tag;
                        std::move(match + 1, active.begin() + held, match);
                        --held;
                    }
                }
                result.events[result.size++] = {uint16_t(tick), tag, kind, pitch, value};
                ++track.size;
            }
        }
        return result;
    }

    struct KSDrumSettings {
        KSDrumSequence sequence;
        uint8_t channel = 0;
        bool enabled = false, keySync = false, latch = false, internal = true, midiOut = false;
        bool retriggerUserSequence = true;
    };

    struct KSDrumOutput {
        ArpNote note;
        bool pressure = false, forcedOff = false;
    };

    class KSDrumArpeggiator final {
        static constexpr uint64_t TicksPerPulse = 16;
        static constexpr uint64_t PulsesPerStep = KSUserDrumSequence::StepTicks / TicksPerPulse;
        struct Active { uint16_t tag; ArpNote note; };
        struct Pending { KSDrumSequence sequence; uint64_t tick; bool patternSwitch = false; };

    public:
        explicit KSDrumArpeggiator(uint32_t seed = 1) noexcept : random_((seed ? seed : 1) * 0x9e3779b9u) {}
        bool enabled() const noexcept { return settings_.enabled; }
        bool switchingPattern() const noexcept { return pending_ && pending_->patternSwitch; }
        std::optional<NativeFrame> nextDeadline() const noexcept { return next_; }
        bool idle() const noexcept { return !next_ && !queued_ && !held_; }

        std::optional<KSDrumOutput> pop() noexcept {
            if (!queued_) return {};
            auto result = output_[head_++];
            if (!--queued_) head_ = 0;
            return result;
        }

        KSDrumSequence userSequence(const KSUserDrumPattern& pattern, uint8_t index = 0) noexcept {
            if (pattern.steps != compiled_.steps || pattern.notes != compiled_.notes ||
                pattern.velocities != compiled_.velocities || pattern.probabilities != compiled_.probabilities ||
                pattern.ratchets != compiled_.ratchets) {
                // The current sequence is borrowed by the player through the
                // loop seam. Subsequent edits may replace the pending buffer.
                userBuffer_ = settings_.sequence.events == user_[0].events.data() ? 1 : 0;
                user_[userBuffer_].compile(pattern);
                compiled_ = pattern;
            }
            auto sequence = user_[userBuffer_].sequence();
            sequence.pattern = index;
            return sequence;
        }

        NativeDrumPlayhead playhead() const noexcept {
            const auto& sequence = settings_.sequence;
            if (!positioned_ || sequence.pattern == NativeDrumPlayhead::Stopped) return {};
            const auto elapsed = std::max(phase_, sequenceOrigin_) - sequenceOrigin_;
            return {sequence.pattern, uint8_t(elapsed % sequence.length / KSUserDrumSequence::StepTicks)};
        }

        bool usesUserSequence() const noexcept {
            return settings_.sequence.events == user_[0].events.data() ||
                   settings_.sequence.events == user_[1].events.data();
        }

        bool configure(NativeFrame at, KSDrumSettings settings, bool deferSequence = false) noexcept {
            if (!canApply(at) || settings.channel >= 16 ||
                (settings.enabled && (!settings.sequence.events || !settings.sequence.length)))
                return false;
            const bool keepKeys = settings.enabled && settings.channel == settings_.channel;
            const bool keepTrigger = keepKeys && (settings.latch || pedalDown_ || keys_.any());
            const bool changeSequence = settings.sequence.events != settings_.sequence.events ||
                settings.sequence.pattern != settings_.sequence.pattern;
            const bool restart = settings.enabled != settings_.enabled ||
                settings.channel != settings_.channel || settings.internal != settings_.internal ||
                settings.midiOut != settings_.midiOut || (triggered_ && !keepTrigger);
            if ((deferSequence || pending_) && !restart && triggered_ && positioned_ && (changeSequence || pending_)) {
                if (changeSequence) queueSequence(at, settings.sequence);
                else pending_.reset();
                settings.sequence = settings_.sequence;
                schedule();
            } else if (changeSequence || restart || pending_) {
                rewind(at);
            }
            if (!keepKeys) {
                keys_.reset();
                pedalDown_ = false;
            }
            if (!keepTrigger) triggered_ = false;
            settings_ = settings;
            return true;
        }

        bool selectSequence(NativeFrame at, KSDrumSequence sequence, uint8_t switchAfter) noexcept {
            if (!canApply(at) || !sequence.events || !sequence.length) return false;
            if (switchAfter != KSDrumGridEdit::SwitchImmediately && triggered_ && positioned_) {
                if (switchingPattern() && sequence.pattern == settings_.sequence.pattern)
                    pending_.reset();
                else {
                    const auto length = settings_.sequence.length;
                    const auto offset = switchAfter == KSDrumGridEdit::SwitchAtEnd ? length :
                        std::min<uint16_t>(length, switchAfter * KSUserDrumSequence::StepTicks);
                    pending_ = Pending{sequence, nextBoundary(at, offset), true};
                }
                schedule();
            } else {
                rewind(at);
                settings_.sequence = sequence;
            }
            return true;
        }

        void setUserSequenceRetrigger(bool enabled) noexcept { settings_.retriggerUserSequence = enabled; }

        bool press(NativeFrame at, uint8_t key, std::optional<KSDrumSequence> selected = {},
                   uint8_t switchAfter = KSDrumGridEdit::DefaultSwitchStep) noexcept {
            if (!enabled() || key >= keys_.size() || !canApply(at)) return false;
            if (selected) {
                if (!selectSequence(at, *selected, switchAfter)) return false;
            } else if (!triggered_ || (settings_.keySync && !switchingPattern() &&
                       (settings_.retriggerUserSequence || !usesUserSequence()))) {
                rewind(at);
            }
            keys_.set(key);
            triggered_ = true;
            return true;
        }

        bool releaseKey(NativeFrame at, uint8_t key) noexcept {
            if (key >= keys_.size() || !canApply(at)) return false;
            keys_.reset(key);
            stopIfReleased(at);
            return true;
        }

        bool retimeSwitch(NativeFrame at, uint8_t switchAfter) noexcept {
            return !switchingPattern() || selectSequence(at, pending_->sequence, switchAfter);
        }

        bool keepAssignedKeys(NativeFrame at, const std::bitset<128>& assigned) noexcept {
            if (!canApply(at)) return false;
            keys_ &= assigned;
            stopIfReleased(at);
            return true;
        }

        bool pedal(NativeFrame at, bool down) noexcept {
            if (!canApply(at)) return false;
            pedalDown_ = down;
            stopIfReleased(at);
            return true;
        }

        bool clear(NativeFrame at) noexcept {
            if (queued_) return false;
            rewind(at);
            keys_.reset();
            triggered_ = pedalDown_ = false;
            return true;
        }

        bool clock(const MusicalClockSignal& signal) noexcept {
            // Transport ticks drive a triggered pattern. Loading or enabling
            // one must leave it silent until a note is routed to this part.
            if (!triggered_) return true;
            if (!canApply(signal.at)) return false;
            if (signal.reset || !signal.running) {
                close(signal.at);
                if (pending_ || source_ != signal.source || (anchor_ && signal.pulse < *anchor_))
                    resetPosition();
            }
            source_ = signal.source;
            if (!signal.tick || !signal.running) return true;
            if (!anchor_) {
                // With Key Sync off, start step one on the shared sixteenth grid.
                if (!settings_.keySync && signal.pulse % PulsesPerStep) return true;
                anchor_ = signal.pulse;
            }
            if (signal.pulse < *anchor_ || signal.pulse - *anchor_ > (UINT64_MAX - UINT16_MAX) / TicksPerPulse ||
                !std::isfinite(signal.framesPerPulse) || signal.framesPerPulse < 1)
                return false;
            phase_ = (signal.pulse - *anchor_) * TicksPerPulse;
            pulseAt_ = signal.at;
            period_ = signal.framesPerPulse;
            const auto& sequence = settings_.sequence;
            if (!positioned_) {
                if (phase_ < sequenceOrigin_) sequenceOrigin_ = 0;
                loop_ = sequenceOrigin_ + (phase_ - sequenceOrigin_) / sequence.length * sequence.length;
                const auto offset = phase_ - loop_;
                const auto first = sequence.events;
                cursor_ = size_t(std::lower_bound(first, first + sequence.size, offset,
                    [](const KSDrumEvent& event, uint64_t tick) { return event.tick < tick; }) - first);
                if (sequence.size) advanceLoop();
                positioned_ = true;
            }
            schedule();
            return true;
        }

        bool takeDue(NativeFrame now) noexcept {
            if (queued_ || !next_ || now < *next_) return false;
            const auto at = *next_;
            if (switchDue_) {
                close(at);
                settings_.sequence = pending_->sequence;
                sequenceOrigin_ = loop_ = pending_->tick;
                pending_.reset();
                cursor_ = 0;
                positioned_ = true;
                schedule();
                return true;
            }
            const auto event = settings_.sequence.events[cursor_++];
            if (event.probability != KSDrumEvent::Always && !stepPlays(event)) {
                advanceLoop();
                schedule();
                return true;
            }
            if (event.kind == KSDrumEventKind::NoteOn) {
                if (held_ == active_.size() || generation_ == UINT64_MAX) return false;
                ArpNote note{at, ++generation_, event.note, event.velocity, settings_.channel, true,
                    settings_.internal, settings_.midiOut};
                active_[held_++] = {event.tag, note};
                output_[queued_++] = {note};
            } else if (event.kind == KSDrumEventKind::NoteOff) {
                const auto match = std::find_if(active_.begin(), active_.begin() + held_,
                    [&](const Active& note) { return note.tag == event.tag; });
                if (match != active_.begin() + held_) {
                    release(at, match->note, event.velocity, false);
                    *match = active_[--held_];
                }
            } else {
                output_[queued_++] = {{at, 0, 0, event.velocity, settings_.channel, false,
                    settings_.internal, settings_.midiOut}, true};
            }
            advanceLoop();
            schedule();
            return true;
        }

    private:
        bool stepPlays(const KSDrumEvent& event) noexcept {
            if (event.probability != KSDrumEvent::RepeatDecision) {
                // One draw at the first hit. Gates and later ratchets reuse it.
                random_ ^= random_ << 13;
                random_ ^= random_ >> 17;
                random_ ^= random_ << 5;
                const auto draw = (uint64_t(random_) * KSDrumGridEdit::MaxProbability) >> 32;
                stepPlays_[event.tag] = draw < event.probability;
            }
            return stepPlays_[event.tag];
        }

        bool canApply(NativeFrame at) const noexcept { return !queued_ && (!next_ || !(*next_ < at)); }
        void rewind(NativeFrame at) noexcept {
            close(at);
            resetPosition();
        }
        void resetPosition() noexcept {
            if (pending_) {
                settings_.sequence = pending_->sequence;
                pending_.reset();
            }
            anchor_.reset();
            sequenceOrigin_ = 0;
        }
        void stopIfReleased(NativeFrame at) noexcept {
            if (triggered_ && !settings_.latch && !pedalDown_ && keys_.none()) {
                triggered_ = false;
                rewind(at);
            }
        }
        void release(NativeFrame at, ArpNote note, uint8_t velocity, bool forced) noexcept {
            note.at = at;
            note.on = false;
            note.velocity = velocity;
            output_[queued_++] = {note, false, forced};
        }
        void close(NativeFrame at) noexcept {
            for (size_t i = 0; i < held_; ++i) release(at, active_[i].note, 0, true);
            held_ = 0;
            stepPlays_.reset();
            next_.reset();
            positioned_ = false;
        }
        void advanceLoop() noexcept {
            if (cursor_ == settings_.sequence.size) {
                cursor_ = 0;
                loop_ += settings_.sequence.length;
            }
        }
        std::optional<NativeFrame> deadline(uint64_t tick) const noexcept {
            const auto offset = tick > phase_ ? tick - phase_ : 0;
            if (offset < TicksPerPulse) {
                const auto frames = std::ceil(period_ * double(offset) / TicksPerPulse);
                if (frames < double(UINT64_MAX) && uint64_t(frames) <= UINT64_MAX - pulseAt_.ticks)
                    return NativeFrame{pulseAt_.ticks + uint64_t(frames)};
            }
            return {};
        }
        uint64_t nextBoundary(NativeFrame at, uint16_t offset) const noexcept {
            const auto length = settings_.sequence.length;
            const auto elapsed = std::max(phase_, sequenceOrigin_) - sequenceOrigin_;
            auto boundary = sequenceOrigin_ + (elapsed / length) * length + offset;
            if (boundary <= phase_) boundary += length;
            if (const auto frame = deadline(boundary); frame && *frame < at) boundary += length;
            return boundary;
        }
        void queueSequence(NativeFrame at, KSDrumSequence sequence) noexcept {
            if (pending_)
                pending_->sequence = sequence;
            else
                pending_ = Pending{sequence, nextBoundary(at, settings_.sequence.length)};
        }
        void schedule() noexcept {
            const auto eventTick = settings_.sequence.size
                ? loop_ + settings_.sequence.events[cursor_].tick : UINT64_MAX;
            switchDue_ = pending_ && pending_->tick <= eventTick;
            next_ = switchDue_ ? deadline(pending_->tick) :
                settings_.sequence.size ? deadline(eventTick) : std::nullopt;
        }

        KSDrumSettings settings_{};
        KSUserDrumPattern compiled_{};
        std::array<KSUserDrumSequence, 2> user_{};
        size_t userBuffer_ = 0;
        uint32_t random_;
        std::bitset<KSDrumGridEdit::Cells> stepPlays_;
        std::bitset<128> keys_;
        std::array<Active, KSDrumCatalog::MaxActive> active_{};
        std::array<KSDrumOutput, KSDrumCatalog::MaxActive> output_{};
        size_t held_ = 0, head_ = 0, queued_ = 0, cursor_ = 0;
        uint64_t loop_ = 0, phase_ = 0, generation_ = 0;
        uint64_t sequenceOrigin_ = 0;
        ClockAuthority source_ = ClockAuthority::Internal;
        NativeFrame pulseAt_{};
        double period_ = 0;
        bool positioned_ = false, triggered_ = false, pedalDown_ = false, switchDue_ = false;
        std::optional<Pending> pending_;
        std::optional<uint64_t> anchor_;
        std::optional<NativeFrame> next_;
    };
}
