#pragma once

#include "controllerEvent.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace novation::controller::command {

// One serialized decoder per MIDI byte stream. Admission is synchronous and
// must own any SysEx bytes it accepts before returning. The view is valid only
// during that call.
class MidiDecoder final {
public:
    enum class Result : uint8_t { Consumed, Backpressure, Malformed, MessageTooLarge };

    struct State {
        uint8_t runningStatus = 0;
        uint8_t pendingStatus = 0;
        uint8_t firstData = 255;
        bool discardingSysEx = false;
        std::vector<uint8_t> sysExPrefix;
    };

    State checkpoint() const {
        return {runningStatus_, pendingStatus_, hasFirstData_ ? firstData_ : uint8_t(255),
                discardingSysEx_, {sysEx_.begin(), sysEx_.begin() + sysExSize_}};
    }

    static bool valid(const State& state) noexcept {
        const auto running = state.runningStatus, pending = state.pendingStatus;
        if (state.discardingSysEx || !state.sysExPrefix.empty()) {
            if (running || pending || state.firstData != 255)
                return false;
            if (state.discardingSysEx)
                return state.sysExPrefix.empty();
            return state.sysExPrefix.size() < MaxSysExBytes && state.sysExPrefix.front() == 0xf0 &&
                std::all_of(state.sysExPrefix.begin() + 1, state.sysExPrefix.end(), [](uint8_t b) { return b < 128; });
        }
        if (running && (running < 0x80 || running >= 0xf0))
            return false;
        if (pending && (pending < 0xf0 ? running != pending : running || pending < 0xf1 || pending > 0xf3))
            return false;
        return state.firstData == 255 || (state.firstData < 128 && pending && takesTwoDataBytes(pending));
    }

    bool restore(const State& state) noexcept {
        if (!valid(state))
            return false;
        runningStatus_ = state.runningStatus;
        pendingStatus_ = state.pendingStatus;
        hasFirstData_ = state.firstData != 255;
        firstData_ = hasFirstData_ ? state.firstData : 0;
        discardingSysEx_ = state.discardingSysEx;
        sysExSize_ = state.sysExPrefix.size();
        std::copy(state.sysExPrefix.begin(), state.sysExPrefix.end(), sysEx_.begin());
        return true;
    }

    // Only Backpressure leaves the byte unconsumed. Retry it before sending
    // further bytes. On a malformed interruption a new status can still start
    // (or, for Tune Request, complete) the next message.
    template<class Admission>
    Result push(uint8_t byte, Admission&& admit)
    {
        // Preserve reserved realtime statuses as opaque bytes, as prepared
        // ingress does. Realtime cannot interrupt the current frame.
        if (byte >= 0xf8)
            return emit(MidiRealtime{byte}, admit);

        if (byte == 0xf7 && (sysExSize_ || discardingSysEx_))
        {
            if (sysExSize_)
            {
                sysEx_[sysExSize_] = byte;
                if (emit(SysExBytes{sysEx_.data(), sysExSize_ + 1}, admit) == Result::Backpressure)
                    return Result::Backpressure;
            }
            sysExSize_ = 0;
            discardingSysEx_ = false;
            return Result::Consumed;
        }

        if (byte >= 0x80)
            return startMessage(byte, admit);

        if (discardingSysEx_)
            return Result::Consumed;
        if (sysExSize_)
        {
            // Reserve EOX space and report an overlong frame as soon as it
            // can no longer fit. Discard its data until a status resyncs.
            if (sysExSize_ == sysEx_.size() - 1)
            {
                sysExSize_ = 0;
                discardingSysEx_ = true;
                return Result::MessageTooLarge;
            }
            sysEx_[sysExSize_++] = byte;
            return Result::Consumed;
        }

        const auto status = pendingStatus_ ? pendingStatus_ : runningStatus_;
        if (!status)
            return Result::Malformed;
        if (!hasFirstData_ && takesTwoDataBytes(status))
        {
            pendingStatus_ = status;
            firstData_ = byte;
            hasFirstData_ = true;
            return Result::Consumed;
        }

        const auto message = hasFirstData_ ? makeMessage(status, firstData_, byte)
                                           : makeMessage(status, byte, 0);
        if (!admit(message))
            return Result::Backpressure;
        pendingStatus_ = 0;
        hasFirstData_ = false;
        return Result::Consumed;
    }

    // Running status alone leaves the stream at a message boundary.
    bool atMessageBoundary() const noexcept
    {
        return !pendingStatus_ && !sysExSize_ && !discardingSysEx_;
    }

    void reset() noexcept
    {
        runningStatus_ = 0;
        pendingStatus_ = 0;
        hasFirstData_ = false;
        sysExSize_ = 0;
        discardingSysEx_ = false;
    }

private:
    template<class Admission>
    static Result emit(const EventPayload& message, Admission& admit)
    {
        return admit(message) ? Result::Consumed : Result::Backpressure;
    }

    template<class Admission>
    Result startMessage(uint8_t status, Admission& admit)
    {
        // A refused single-byte message must leave any interrupted frame
        // intact so retry reports the same interruption on acceptance.
        if (status == 0xf6 && emit(MidiCommon{status, 0, 0}, admit) == Result::Backpressure)
            return Result::Backpressure;

        const bool interrupted = pendingStatus_ || sysExSize_;
        reset();
        if (status < 0xf0)
        {
            runningStatus_ = status;
            pendingStatus_ = status;
        }
        else
        {
            switch (status)
            {
            case 0xf0:
                sysEx_[sysExSize_++] = status;
                break;
            case 0xf1:
            case 0xf2:
            case 0xf3:
                pendingStatus_ = status;
                break;
            case 0xf6:
                break;
            default:
                return Result::Malformed;
            }
        }
        return interrupted ? Result::Malformed : Result::Consumed;
    }

    static bool takesTwoDataBytes(uint8_t status) noexcept
    {
        if (status >= 0xf0)
            return status == 0xf2;
        return (status & 0xe0) != 0xc0;
    }

    static EventPayload makeMessage(uint8_t status, uint8_t data1, uint8_t data2)
    {
        const auto channel = static_cast<uint8_t>(status & 0x0f);
        switch (status & 0xf0)
        {
        case 0x80: return NoteOff{channel, data1, data2};
        case 0x90: return NoteOn{channel, data1, data2}; // Includes the wire form for release at velocity zero.
        case 0xa0: return PolyPressure{channel, data1, data2};
        case 0xb0: return ControlChange{channel, data1, data2};
        case 0xc0: return ProgramChange{channel, data1};
        case 0xd0: return ChannelPressure{channel, data1};
        case 0xe0: return PitchBend{channel, static_cast<uint16_t>(data1 | (data2 << 7))};
        default:   return MidiCommon{status, data1, data2};
        }
    }

    std::array<uint8_t, MaxSysExBytes> sysEx_{};
    std::size_t sysExSize_ = 0;
    uint8_t runningStatus_ = 0;
    uint8_t pendingStatus_ = 0;
    uint8_t firstData_ = 0;
    bool hasFirstData_ = false;
    bool discardingSysEx_ = false;
};

}
