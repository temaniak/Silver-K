#pragma once

#include "nativeEditorState.h"
#include "../musicalClock.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace novation::controller::command {

struct CommandAudioFormat {
    double sampleRate;
    unsigned maximumBlock;
    unsigned audioFloor;
    bool parallelDsp = false;
};

// Called synchronously on the DSP owner. The output bytes are borrowed for
// the duration of the call. The host queue copies complete messages.
struct CommandMidiSink {
    void* context = nullptr;
    void (*send)(void*, const uint8_t*, size_t) noexcept = nullptr;
    void emit(const uint8_t* bytes, size_t size) const noexcept {
        if (send)
            send(context, bytes, size);
    }
};

class CommandAudioEngine {
public:
    virtual ~CommandAudioEngine() = default;
    virtual double nativeRate() const noexcept = 0;
    virtual std::array<int32_t, 4> render(int32_t adc) = 0;
    virtual bool enqueueMidiByte(uint8_t) = 0;
    virtual bool enqueueControl(const ControllerEvent&) = 0;
    virtual bool enqueueHostPosition(MusicalClock::Position) = 0;
    virtual void setMidiSink(CommandMidiSink) noexcept = 0;
    virtual NativeEditorState editorState() const = 0;
    virtual bool checkpointReady() const noexcept = 0;
    virtual std::vector<uint8_t> saveState() const = 0;
    virtual bool setParallelDsp(bool) { return false; }
    virtual void stop() = 0;
};

// A cold host session. The callbacks have one producer/consumer pair and the
// editor has one reader. Construct, save, post controls and destroy off audio.
class CommandAudioDevice {
public:
    using Format = CommandAudioFormat;
    static constexpr unsigned MaximumHostBlock = 4096;
    struct Block {
        std::array<float*, 4> outputs;
        const float* inputLeft;
        const float* inputRight;
        unsigned frames;
        MusicalClock::Position position;
        bool offline;
        const ControllerEvent* automation = nullptr;
        size_t automationCount = 0;
    };
    struct MidiEvent {
        uint32_t offset;
        const uint8_t* data;
        size_t size;
        // Part-addressed host automation shares the sample-ordered MIDI stream.
        const ControllerEvent* control = nullptr;
    };
    struct MidiInput {
        void* context = nullptr;
        // Yield borrowed events in host order, including ties.
        bool (*next)(void*, MidiEvent&) noexcept = nullptr;
    };
    struct MidiOutput {
        void* context = nullptr;
        // A short host buffer leaves the complete event pending.
        bool (*write)(void*, const MidiEvent&) noexcept = nullptr;
    };
    enum class Error { None, InvalidBlock, InvalidMidi, MidiCapacity, AudioCapacity, ClockCapacity, Worker };
    struct Metrics {
        uint64_t processedFrames, renderedFrames, midiBytes, hostPositions, missingFrames, staleFrames;
        uint64_t controls, midiOutputEvents, midiOutputOverflows;
    };

    virtual ~CommandAudioDevice();
    CommandAudioDevice(const CommandAudioDevice&) = delete;
    CommandAudioDevice& operator=(const CommandAudioDevice&) = delete;

    void process(const Block&, MidiInput) noexcept;
    void process(const Block&, MidiInput, MidiOutput) noexcept;
    unsigned latency() const noexcept;
    Error error() const noexcept;
    const char* workerWaitBackend() const noexcept;
    void rethrowWorkerError() const;
    Metrics metrics() const noexcept;
    struct PartControls { uint8_t channel, cutoff, volume; };
    std::optional<PartControls> partControls(unsigned part) const noexcept;
    std::optional<uint8_t> partChannel(unsigned part) const noexcept;
    std::optional<NativeEditorState> editorState() noexcept;
    std::optional<NativeEditorState> audioEditorState() noexcept;
    NativeDrumPlayhead drumPlayhead(unsigned part) const noexcept;
    // Ordered typed controls. They enter at sample zero of the next host
    // block, followed by its automation and raw MIDI.
    bool postControls(const ControllerEvent*, size_t);
    bool postControl(const ControllerEvent& event) { return postControls(&event, 1); }

    // A complete archive at a stable audio boundary. A pending input block
    // can defer the request. Saving does not dispatch MIDI or run the DSP.
    std::optional<std::vector<uint8_t>> saveState();
    // Control thread only. The owner applies the change between renders.
    bool setParallelDsp(bool);

protected:
    using Factory = std::function<std::unique_ptr<CommandAudioEngine>(Format)>;
    CommandAudioDevice(Format, Factory);

private:
    struct Worker;
    std::unique_ptr<Worker> worker_;
};
}
