#pragma once

#include "controllerTime.h"

#include <cstddef>
#include <cstdint>
#include <variant>

namespace novation::controller::command
{
    enum class Model : uint8_t { K, KS };
    enum class Endpoint : uint8_t { A, B };

    struct Session
    {
        const void* owner = nullptr;
        uint64_t generation = 0;

        friend constexpr bool operator==(Session left, Session right) noexcept
        {
            return left.owner == right.owner && left.generation == right.generation;
        }
        friend constexpr bool operator!=(Session left, Session right) noexcept { return !(left == right); }
    };

    struct NoteOn { uint8_t channel, note, velocity; };
    struct NoteOff { uint8_t channel, note, velocity; };
    struct ControlChange { uint8_t channel, controller, value; };
    struct ProgramChange { uint8_t channel, program; };
    struct PolyPressure { uint8_t channel, note, pressure; };
    struct ChannelPressure { uint8_t channel, pressure; };
    struct PitchBend { uint8_t channel; uint16_t value; };
    struct MidiRealtime { uint8_t status; };
    struct MidiCommon { uint8_t status, data1, data2; };
    struct KSDesktopSoundLoad;
    struct KSDesktopSoundCommand { KSDesktopSoundLoad* request; };

    // Desktop-only edit. Slot 0 is Single, slots 1–4 are Performance parts.
    struct KSDrumGridEdit
    {
        enum class Kind : uint8_t {
            SelectUser, Note, Velocity, Length, Probability, Ratchet, Remove, AssignedKeys,
            KeyLow, KeyHigh, SwitchAfter, SelectPattern, Retrigger
        };
        static constexpr uint8_t Rows = 10, Steps = 16, Patterns = 12, Slots = 5;
        static constexpr uint8_t FirstNote = 36, LastNote = 84;
        static constexpr uint8_t MaxProbability = 100, MaxRatchet = 16;
        static constexpr uint8_t Unassigned = 128, SwitchImmediately = 0, SwitchAtEnd = Steps + 1;
        static constexpr uint8_t DefaultSwitchStep = 8;
        static constexpr size_t Cells = Rows * Steps;

        Kind kind;
        uint8_t slot, pattern, index, value;

        constexpr bool valid() const noexcept
        {
            if (slot >= Slots || pattern >= Patterns)
                return false;
            switch (kind)
            {
            case Kind::SelectUser: return pattern == 0 && index == 0 && value <= 1;
            case Kind::Note: return index < Rows && value >= FirstNote && value <= LastNote;
            case Kind::Velocity: return index < Cells && value >= 1 && value < 128;
            case Kind::Length: return index == 0 && value >= 1 && value <= Steps;
            case Kind::KeyLow:
            case Kind::KeyHigh: return index == 0 && value <= Unassigned;
            case Kind::SelectPattern: return index == 0 && value < Patterns;
            case Kind::Probability: return index < Cells && value <= MaxProbability;
            case Kind::Ratchet: return index < Cells && value >= 1 && value <= MaxRatchet;
            case Kind::Remove: return index < Cells && value == 0;
            case Kind::AssignedKeys:
            case Kind::Retrigger: return index == 0 && value <= 1;
            case Kind::SwitchAfter: return index == 0 && value <= SwitchAtEnd;
            }
            return false;
        }

        constexpr bool needsDrumMap() const noexcept
        {
            return kind == Kind::Note || kind == Kind::Velocity || kind == Kind::Probability ||
                   kind == Kind::Ratchet || kind == Kind::Remove;
        }
    };

    // The desktop editor follows MIDI for one storage slot at a time.
    struct KSDrumEditMode
    {
        static constexpr uint8_t Off = 255;
        uint8_t slot = Off;
        constexpr bool valid() const noexcept { return slot < KSDrumGridEdit::Slots || slot == Off; }
    };

    struct KSDrumVoiceEdit
    {
        uint64_t programRevision;
        uint8_t slot, bank, program, field, mask, bits;

        static constexpr bool voiceField(int field) noexcept
        {
            // Arpeggiator and shared FX stay with the part, as does routing.
            return (field >= 0 && field < 84) || field == 125 ||
                   (field >= 127 && field < 179 && field != 149 && field != 167);
        }

        constexpr bool valid() const noexcept
        {
            return programRevision && slot < KSDrumGridEdit::Slots && bank >= 5 && bank <= 8 &&
                   program <= KSDrumGridEdit::LastNote - KSDrumGridEdit::FirstNote &&
                   voiceField(field) && mask && mask < 128 && !(bits & ~mask);
        }
    };

    inline constexpr size_t MaxSysExBytes = 4096;
    struct SysExBytes { const uint8_t* data; size_t size; };

    enum class DeadlineKind : uint8_t { ClockTick, ArpTick, GateOff, ParameterRefresh };
    struct MusicalDeadline
    {
        DeadlineKind kind;
        uint64_t subject = 0;
    };

    struct DspReply
    {
        Endpoint endpoint;
        uint64_t transaction;
        uint32_t word;
    };

    using EventPayload = std::variant<NoteOn, NoteOff, ControlChange, ProgramChange, PolyPressure,
                                      ChannelPressure, PitchBend, MidiRealtime, MidiCommon, SysExBytes,
                                      MusicalDeadline, DspReply, KSDrumGridEdit, KSDesktopSoundCommand,
                                      KSDrumEditMode, KSDrumVoiceEdit>;
    inline constexpr uint8_t RoutedByChannel = 255;

    struct ControllerEvent
    {
        EventPayload payload;
        uint8_t part = RoutedByChannel;
    };
}
