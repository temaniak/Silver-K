#pragma once

#include "../../../controller/command/nativeArpeggiator.h"

namespace novation::controller::command {
    enum class KSDrumEventKind : uint8_t { NoteOn, NoteOff, Pressure };

    struct KSDrumEvent {
        uint16_t tick, tag;
        KSDrumEventKind kind;
        uint8_t note, velocity;
    };

    struct KSDrumSequence {
        const KSDrumEvent* events = nullptr;
        uint16_t size = 0, length = 0;
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
        bool enabled = false, keySync = false, internal = true, midiOut = false;
    };

    struct KSDrumOutput {
        ArpNote note;
        bool pressure = false, forcedOff = false;
    };

    class KSDrumArpeggiator final {
        static constexpr uint64_t TicksPerPulse = 16;
        struct Active { uint16_t tag; ArpNote note; };

    public:
        bool enabled() const noexcept { return settings_.enabled; }
        bool keySync() const noexcept { return settings_.keySync; }
        std::optional<NativeFrame> nextDeadline() const noexcept { return next_; }
        bool idle() const noexcept { return !next_ && !queued_ && !held_; }

        std::optional<KSDrumOutput> pop() noexcept {
            if (!queued_) return {};
            auto result = output_[head_++];
            if (!--queued_) head_ = 0;
            return result;
        }

        bool configure(NativeFrame at, KSDrumSettings settings) noexcept {
            if (!canApply(at) || settings.channel >= 16 ||
                (settings.enabled && (!settings.sequence.events || !settings.sequence.size || !settings.sequence.length)))
                return false;
            if (settings.sequence.events != settings_.sequence.events || settings.enabled != settings_.enabled ||
                settings.channel != settings_.channel || settings.internal != settings_.internal ||
                settings.midiOut != settings_.midiOut) {
                if (!restart(at)) return false;
            }
            settings_ = settings;
            return true;
        }

        bool restart(NativeFrame at) noexcept {
            if (!canApply(at)) return false;
            close(at);
            anchor_.reset();
            return true;
        }

        bool clock(const MusicalClockSignal& signal) noexcept {
            if (!enabled()) return true;
            if (!canApply(signal.at)) return false;
            if (signal.reset || !signal.running) {
                close(signal.at);
                if (source_ != signal.source || (anchor_ && signal.pulse < *anchor_)) anchor_.reset();
            }
            source_ = signal.source;
            if (!signal.tick || !signal.running) return true;
            if (!anchor_) anchor_ = signal.pulse;
            if (signal.pulse < *anchor_ || signal.pulse - *anchor_ > (UINT64_MAX - UINT16_MAX) / TicksPerPulse ||
                !std::isfinite(signal.framesPerPulse) || signal.framesPerPulse < 1)
                return false;
            phase_ = (signal.pulse - *anchor_) * TicksPerPulse;
            pulseAt_ = signal.at;
            period_ = signal.framesPerPulse;
            const auto& sequence = settings_.sequence;
            if (!positioned_) {
                loop_ = phase_ / sequence.length * sequence.length;
                const auto offset = phase_ - loop_;
                const auto first = sequence.events;
                cursor_ = size_t(std::lower_bound(first, first + sequence.size, offset,
                    [](const KSDrumEvent& event, uint64_t tick) { return event.tick < tick; }) - first);
                advanceLoop();
                positioned_ = true;
            }
            schedule();
            return true;
        }

        bool takeDue(NativeFrame now) noexcept {
            if (queued_ || !next_ || now < *next_) return false;
            const auto at = *next_;
            const auto event = settings_.sequence.events[cursor_++];
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
        bool canApply(NativeFrame at) const noexcept { return !queued_ && (!next_ || !(*next_ < at)); }
        void release(NativeFrame at, ArpNote note, uint8_t velocity, bool forced) noexcept {
            note.at = at;
            note.on = false;
            note.velocity = velocity;
            output_[queued_++] = {note, false, forced};
        }
        void close(NativeFrame at) noexcept {
            for (size_t i = 0; i < held_; ++i) release(at, active_[i].note, 0, true);
            held_ = 0;
            next_.reset();
            positioned_ = false;
        }
        void advanceLoop() noexcept {
            if (cursor_ == settings_.sequence.size) {
                cursor_ = 0;
                loop_ += settings_.sequence.length;
            }
        }
        void schedule() noexcept {
            next_.reset();
            const auto tick = loop_ + settings_.sequence.events[cursor_].tick;
            const auto offset = tick > phase_ ? tick - phase_ : 0;
            if (offset < TicksPerPulse) {
                const auto frames = std::ceil(period_ * double(offset) / TicksPerPulse);
                if (frames < double(UINT64_MAX) && uint64_t(frames) <= UINT64_MAX - pulseAt_.ticks)
                    next_ = NativeFrame{pulseAt_.ticks + uint64_t(frames)};
            }
        }

        KSDrumSettings settings_{};
        std::array<Active, KSDrumCatalog::MaxActive> active_{};
        std::array<KSDrumOutput, KSDrumCatalog::MaxActive> output_{};
        size_t held_ = 0, head_ = 0, queued_ = 0, cursor_ = 0;
        uint64_t loop_ = 0, phase_ = 0, generation_ = 0;
        ClockAuthority source_ = ClockAuthority::Internal;
        NativeFrame pulseAt_{};
        double period_ = 0;
        bool positioned_ = false;
        std::optional<uint64_t> anchor_;
        std::optional<NativeFrame> next_;
    };
}
