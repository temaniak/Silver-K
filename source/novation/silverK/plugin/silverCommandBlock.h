#pragma once

#include "../../controller/command/commandAudioDevice.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace silverK {

inline void processCommandBlock(novation::controller::command::CommandAudioDevice& device,
                                juce::AudioBuffer<float>& audio, const juce::MidiBuffer& midi,
                                unsigned inputChannels, novation::controller::MusicalClock::Position position,
                                bool offline, unsigned maximumBlock = 4096, double rate = 48000,
                                const novation::controller::command::ControllerEvent* controls = nullptr,
                                size_t controlCount = 0, juce::MidiBuffer* output = nullptr,
                                bool retainOutput = false) noexcept {
    using Device = novation::controller::command::CommandAudioDevice;
    struct MidiCursor {
        juce::MidiBufferIterator next, end;
        int first, last;
        bool final;
    } cursor{midi.cbegin(), midi.cend(), 0, 0, false};
    const Device::MidiInput input{&cursor, [](void* context, Device::MidiEvent& event) noexcept {
        auto& cursor = *static_cast<MidiCursor*>(context);
        if (cursor.next == cursor.end)
            return false;
        const auto message = *cursor.next;
        if (message.samplePosition >= cursor.last && !cursor.final)
            return false;
        ++cursor.next;
        event = {message.samplePosition < cursor.first ? UINT32_MAX : uint32_t(message.samplePosition - cursor.first),
                 message.data, size_t(message.numBytes)};
        return true;
    }};
    struct MidiWriter {
        juce::MidiBuffer* output;
        int first = 0;
        size_t accounted = 0;
    } writer{output};
    Device::MidiOutput destination{};
    if (output || retainOutput)
        destination = {&writer, [](void* context, const Device::MidiEvent& event) noexcept {
            auto& writer = *static_cast<MidiWriter*>(context);
            // The caller provides a 16 KiB reserve. Allow headroom for JUCE
            // framing and leave a message in the worker queue when it won't fit.
            constexpr size_t Budget = 12 * 1024, FramingAllowance = 32;
            if (!writer.output || writer.accounted + event.size + FramingAllowance > Budget)
                return false;
            if (!writer.output->addEvent(event.data, int(event.size), writer.first + int(event.offset)))
                return false;
            writer.accounted += event.size + FramingAllowance;
            return true;
        }};

    const auto total = audio.getNumSamples();
    for (auto first = 0; first < total;) {
        const auto frames = std::min(total - first, int(std::max(1u, maximumBlock)));
        cursor.first = writer.first = first;
        cursor.last = first + frames;
        cursor.final = cursor.last == total;
        std::array<float*, 4> channels{};
        for (size_t i = 0; i < channels.size() && i < size_t(audio.getNumChannels()); ++i)
            channels[i] = audio.getWritePointer(int(i), first);
        auto currentPosition = position;
        if (position.available && position.playing && position.hasPpq)
            currentPosition.ppq += double(first) * position.bpm / (60 * rate);
        device.process({channels, inputChannels ? channels[0] : nullptr, inputChannels > 1 ? channels[1] : nullptr,
                        unsigned(frames), currentPosition, offline, first ? nullptr : controls, first ? 0 : controlCount},
                       input, destination);
        first += frames;
    }
}
}
