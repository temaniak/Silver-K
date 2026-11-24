#pragma once

#include "midiDecoder.h"

#include <optional>
#include <vector>

namespace novation::controller::command {

// Prepare on the control side. The worker only reads complete typed events
// and synchronously copies any SysEx into its own bounded storage.
class PreparedMidi final {
public:
    static constexpr std::size_t Capacity = 65536;
    enum class Error : uint8_t { None, Empty, Malformed, Incomplete, MessageTooLarge, Capacity };

    PreparedMidi() = default;
    PreparedMidi(PreparedMidi&&) noexcept = default;
    PreparedMidi& operator=(PreparedMidi&&) noexcept = default;

    static PreparedMidi prepare(const uint8_t* input, std::size_t size)
    {
        if (!input || !size)
            return PreparedMidi(Error::Empty);
        if (size > Capacity)
            return PreparedMidi(Error::Capacity);

        PreparedMidi prepared;
        MidiDecoder decoder;
        const auto append = [&](const EventPayload& payload) {
            if (const auto* sysEx = std::get_if<SysExBytes>(&payload))
            {
                const auto offset = prepared.sysEx_.size();
                prepared.sysEx_.insert(prepared.sysEx_.end(), sysEx->data, sysEx->data + sysEx->size);
                prepared.events_.push_back({SysExBytes{nullptr, sysEx->size}, offset});
            }
            else
            {
                prepared.events_.push_back({payload, 0});
            }
            return true;
        };

        for (std::size_t index = 0; index < size; ++index)
        {
            const auto result = decoder.push(input[index], append);
            if (result == MidiDecoder::Result::MessageTooLarge)
                return PreparedMidi(Error::MessageTooLarge);
            if (result != MidiDecoder::Result::Consumed)
                return PreparedMidi(Error::Malformed);
        }
        if (!decoder.atMessageBoundary())
            return PreparedMidi(Error::Incomplete);

        prepared.error_ = Error::None;
        return prepared;
    }

    bool valid() const noexcept { return !events_.empty(); }
    Error error() const noexcept
    {
        return !valid() && error_ == Error::None ? Error::Empty : error_;
    }
    std::size_t eventCount() const noexcept { return events_.size(); }

    // The SysEx view borrows from this object. Consume it before the owner is
    // moved, assigned or destroyed. Indexing performs no parsing or allocation.
    std::optional<EventPayload> eventAt(std::size_t index) const & noexcept
    {
        if (index >= events_.size())
            return std::nullopt;

        const auto& event = events_[index];
        if (const auto* sysEx = std::get_if<SysExBytes>(&event.payload))
            return SysExBytes{sysEx_.data() + event.sysExOffset, sysEx->size};
        return event.payload;
    }
    std::optional<EventPayload> eventAt(std::size_t) const && = delete;

private:
    struct Event
    {
        EventPayload payload;
        std::size_t sysExOffset;
    };

    explicit PreparedMidi(Error error) noexcept : error_(error) {}

    Error error_ = Error::Empty;
    std::vector<Event> events_;
    std::vector<uint8_t> sysEx_;
};

}
