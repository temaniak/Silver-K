#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <limits>
#include <optional>

namespace novation::controller::command
{
    // One part's musical key state. Ordered presses may contain duplicates;
    // physical/held maps are independent of that bounded ordering history.
    // No firmware addresses, execution stages or timing state are retained.
    class KSKeyState final
    {
    public:
        static constexpr size_t NoteCount = 128;
        static constexpr size_t OrderedCapacity = 32;
        struct Key {
            uint8_t note = 0, velocity = 0;
            friend constexpr bool operator==(Key a, Key b) noexcept { return a.note == b.note && a.velocity == b.velocity; }
        };
        struct Projection
        {
            std::array<uint8_t, NoteCount> velocities{};
            std::bitset<NoteCount> physical, held;
            std::array<Key, OrderedCapacity> ordered{};
            uint8_t count = 0;
        };

        static std::optional<KSKeyState> prepare(Projection seed) noexcept
        {
            if (seed.count > OrderedCapacity ||
                std::any_of(seed.velocities.begin(), seed.velocities.end(), [](uint8_t v) { return v >= 128; }))
                return std::nullopt;
            for (size_t i = 0; i < seed.count; ++i)
                if (seed.ordered[i].note >= 128 || !seed.ordered[i].velocity || seed.ordered[i].velocity >= 128)
                    return std::nullopt;
            // Unused serialization slots are not musical state.
            std::fill(seed.ordered.begin() + seed.count, seed.ordered.end(), Key{});
            KSKeyState value;
            value.state_ = seed;
            return value;
        }

        bool canChange() const noexcept { return revision_ != std::numeric_limits<uint64_t>::max(); }
        bool press(uint8_t note, uint8_t velocity) noexcept
        {
            if (note >= NoteCount || !velocity || velocity >= 128 || !canChange())
                return false;
            if (state_.count < OrderedCapacity)
                state_.ordered[state_.count++] = {note, velocity};
            state_.velocities[note] = velocity;
            state_.physical.set(note);
            state_.held.set(note);
            ++revision_;
            return true;
        }

        // Hold policy is a semantic input. Sustain publication/voice-release
        // policy belongs to the note controller; it is not a timer/IRQ phase.
        bool release(uint8_t note, bool retainHeld = false) noexcept
        {
            if (note >= NoteCount || !canChange())
                return false;
            state_.physical.reset(note);
            if (!retainHeld)
            {
                state_.held.reset(note);
                const auto end = state_.ordered.begin() + state_.count;
                const auto found = std::find_if(state_.ordered.begin(), end,
                    [note](Key key) { return key.note == note; });
                if (found != end)
                {
                    std::move(found + 1, end, found);
                    state_.ordered[--state_.count] = {};
                }
            }
            ++revision_;
            return true;
        }

        bool clearKeys() noexcept
        {
            if (!canChange())
                return false;
            state_.physical.reset();
            state_.held.reset();
            state_.ordered.fill({});
            state_.count = 0;
            // Per-note raw velocities remain available for musical policies.
            ++revision_;
            return true;
        }

        const Projection& projection() const noexcept { return state_; }
        uint64_t revision() const noexcept { return revision_; }

    private:
        Projection state_;
        uint64_t revision_ = 0;
    };
}
