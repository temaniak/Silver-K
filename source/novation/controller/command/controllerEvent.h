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
                                      MusicalDeadline, DspReply>;
    inline constexpr uint8_t RoutedByChannel = 255;

    struct ControllerEvent
    {
        EventPayload payload;
        uint8_t part = RoutedByChannel;
    };
}
