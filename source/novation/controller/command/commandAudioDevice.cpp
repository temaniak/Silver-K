#include "commandAudioDevice.h"
#ifdef SILVER_P12_ACTION_TRACE
#include "p12ActionTrace.h"
#endif
#ifdef SILVER_P12_NOTE_TRACE
#include "p12NoteTrace.h"
#endif
#ifdef SILVER_P12_CPU_SCOPES
#include "p12CpuScopes.h"
#endif
#ifdef SILVER_P12_STATE_SIZE
#include "p12StateSize.h"
#endif

#include "commandAudioInput.h"
#include "commandOutputResampler.h"
#include "../audioOutputPriming.h"
#include "../../kstation/kstationLib/kstationQueue.h"
#include "baseLib/workerWakeup.h"
#include "dsp56kBase/threadtools.h"

#ifdef SILVER_COMMAND_AUDIO_TRACE
#include "commandAudioTrace.h"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace novation::controller::command {
namespace {
using Position = MusicalClock::Position;
using Archive = std::optional<std::vector<uint8_t>>;

constexpr unsigned MaxBlock = CommandAudioDevice::MaximumHostBlock;
constexpr unsigned AudioQueueCapacity = 2 * MaxBlock;

unsigned bufferTarget(CommandAudioDevice::Format format) {
    if (!std::isfinite(format.sampleRate) || format.sampleRate < 8000 || format.sampleRate > 384000 ||
        !format.maximumBlock || format.maximumBlock > MaxBlock)
        throw std::invalid_argument("Unsupported command host format");
    return unsigned(outputBufferTarget(hostOutputLead(format.maximumBlock), format.audioFloor));
}

struct InputSample { uint64_t at; float value; };
struct OutputSample { uint64_t at; std::array<float, 4> value; };
struct DrumPlayheadSample { uint64_t at; NativeDrumPlayheads playheads; };
static_assert(std::atomic<NativeDrumPlayheads>::is_always_lock_free);
struct InputEvent {
    uint64_t at = 0;
    uint8_t byte = 0;
    bool raw = true;
    ControllerEvent control{};
};
struct MidiMessage {
    uint64_t at;
    size_t size;
    std::array<uint8_t, MaxSysExBytes> bytes;
};
struct HostPosition { uint64_t at; Position value; };

bool controlAllowed(Model model, const ControllerEvent& event) noexcept {
    if (event.part != RoutedByChannel && event.part >= (model == Model::K ? 1u : 4u))
        return false;
    if (const auto* edit = std::get_if<KSDrumGridEdit>(&event.payload))
        return model == Model::KS && event.part == RoutedByChannel && edit->valid();
    if (const auto* load = std::get_if<KSDesktopSoundCommand>(&event.payload))
        return model == Model::KS && event.part == RoutedByChannel && load->request;
    if (const auto* mode = std::get_if<KSDrumEditMode>(&event.payload))
        return model == Model::KS && event.part == RoutedByChannel && mode->valid();
    if (const auto* edit = std::get_if<KSDrumVoiceEdit>(&event.payload))
        return model == Model::KS && event.part == RoutedByChannel && edit->valid();
    return std::visit([](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, ControlChange>)
            return value.channel < 16 && value.controller < 128 && value.value < 128;
        else if constexpr (std::is_same_v<T, ProgramChange>)
            return value.channel < 16 && value.program < 128;
        else if constexpr (std::is_same_v<T, NoteOn> || std::is_same_v<T, NoteOff>)
            return value.channel < 16 && value.note < 128 && value.velocity < 128;
        else
            return false;
    }, event.payload);
}
}

struct CommandAudioDevice::Worker {
    explicit Worker(Format format, Factory make)
        : format(format), lead(bufferTarget(format)) {
        std::promise<void> start;
        auto booted = start.get_future();
        thread = std::thread([this, make = std::move(make), start = std::move(start)]() mutable {
            run(make, start);
        });
        try {
            booted.get();
        } catch (...) {
            stop();
            throw;
        }
    }

    ~Worker() { stop(); }

    void stop() noexcept {
        ready.store(false, std::memory_order_release);
        stopping.store(true, std::memory_order_release);
        wake.notify();
        if (thread.joinable()) {
            thread.join();
#ifdef SILVER_COMMAND_AUDIO_TRACE
            if (trace)
                trace->flush(lead, format.maximumBlock, format.sampleRate, wake.backend());
#endif
        }
    }

    void fail(Error cause) noexcept {
        auto expected = Error::None;
        error.compare_exchange_strong(expected, cause, std::memory_order_release, std::memory_order_relaxed);
        ready.store(false, std::memory_order_release);
        stopping.store(true, std::memory_order_release);
        wake.notify();
    }

    bool submitPosition(uint64_t at, Position value) noexcept {
        if (!value.available || !std::isfinite(value.bpm) || value.bpm < 1 || value.bpm > 1000)
            value = {};
        value.hasPpq = value.hasPpq && std::isfinite(value.ppq) && std::abs(value.ppq) < 1e12;

        // The native clock extrapolates an unchanged transport segment.
        // Publish tempo, transport and PPQ discontinuities at their first sample.
        const auto& old = lastPosition.value;
        if (old.available == value.available && old.playing == value.playing && old.bpm == value.bpm &&
            old.hasPpq == value.hasPpq) {
            const auto distance = old.playing ? double(at - lastPosition.at) : 0;
            const auto expected = old.ppq + distance * old.bpm / (60 * format.sampleRate);
            if (!value.hasPpq || std::abs(value.ppq - expected) <= 1e-8)
                return true;
        }
        if (!positions.push({at, value}))
            return false;
        lastPosition = {at, value};
        return true;
    }

    bool postControls(const ControllerEvent* events, size_t count) {
        const std::lock_guard lock(controlProducers);
        if (!ready.load(std::memory_order_acquire) || !events || !count || count > ControlCapacity ||
            !std::all_of(events, events + count, [&](const auto& event) { return controlAllowed(model, event); }))
            return false;
        return controls.push(events, count);
    }

    void process(const Block& block, MidiInput source, MidiOutput destination) noexcept {
        const auto silence = [&] {
            for (auto* channel : block.outputs)
                if (channel)
                    std::fill_n(channel, block.frames, 0.0f);
        };
        if (!block.frames)
            return;
        if (block.frames > format.maximumBlock)
            fail(Error::InvalidBlock);
        if (!ready.load(std::memory_order_acquire)) {
            silence();
            return;
        }

        const auto first = consumed.load(std::memory_order_relaxed);
        const auto inputStart = first + lead;
#ifdef SILVER_COMMAND_AUDIO_TRACE
        CommandAudioTrace::Callback timing{};
        if (trace) {
            timing.begin = CommandAudioTrace::now();
            timing.first = first;
            timing.limit = first + block.frames;
            timing.ready = output.size();
            timing.rendered = rendered.load(std::memory_order_acquire);
            timing.firstMissing = timing.limit;
        }
#endif
        // Publish all input before making its host deadlines reachable for
        // the worker. This also copies ADC samples before aliased output writes.
        acceptingBlock.store(true, std::memory_order_release);
        if (!submitPosition(inputStart, block.position))
            fail(Error::ClockCapacity);
        const auto pending = controls.size();
        if (pending) {
            if (!midi.pushGenerated(pending, [&](size_t i) {
                InputEvent event{inputStart, 0, false};
                controls.peekAt(i, event.control);
                return event;
            })) {
                fail(Error::MidiCapacity);
            } else {
                ControllerEvent consumedControl;
                for (size_t i = 0; i < pending; ++i)
                    controls.pop(consumedControl);
                controlEvents.fetch_add(pending, std::memory_order_relaxed);
            }
        }
        if (block.automationCount && ready.load(std::memory_order_relaxed)) {
            const auto* events = block.automation;
            const auto count = block.automationCount;
            if (!events || count > ControlCapacity ||
                !std::all_of(events, events + count, [&](const auto& event) { return controlAllowed(model, event); }))
                fail(Error::InvalidMidi);
            else if (!midi.pushGenerated(count, [&](size_t i) { return InputEvent{inputStart, 0, false, events[i]}; }))
                fail(Error::MidiCapacity);
            else
                controlEvents.fetch_add(count, std::memory_order_relaxed);
        }
        uint64_t accepted = 0;
        uint32_t lastOffset = 0;
        MidiEvent event{};
        while (ready.load(std::memory_order_relaxed) && source.next && source.next(source.context, event)) {
            if (event.offset < lastOffset || event.offset >= block.frames ||
                (event.control ? !controlAllowed(model, *event.control) : !event.data || !event.size)) {
                fail(Error::InvalidMidi);
                break;
            }
            const auto at = inputStart + event.offset;
            const bool queued = event.control
                ? midi.push(InputEvent{at, 0, false, *event.control})
                : midi.pushGenerated(event.size, [&](size_t i) { return InputEvent{at, event.data[i]}; });
            if (!queued) {
                fail(Error::MidiCapacity);
                break;
            }
            lastOffset = event.offset;
            if (event.control)
                controlEvents.fetch_add(1, std::memory_order_relaxed);
            else
                accepted += event.size;
        }
        midiBytes.fetch_add(accepted, std::memory_order_relaxed);
        if (ready.load(std::memory_order_relaxed) && !input.pushGenerated(block.frames, [&](size_t i) {
            const auto sample = block.inputLeft
                ? (block.inputRight ? AudioInput::mono(block.inputLeft[i], block.inputRight[i])
                                    : AudioInput::finite(block.inputLeft[i]))
                : 0.0f;
            return InputSample{inputStart + i, sample};
        }))
            fail(Error::AudioCapacity);
        if (!ready.load(std::memory_order_acquire)) {
            acceptingBlock.store(false, std::memory_order_release);
            silence();
            return;
        }

        const auto end = first + block.frames;
        if (block.offline)
            waitForOutput(end);
        uint64_t missing = 0, stale = 0;
        OutputSample sample{};
        for (auto at = first; at < end; ++at) {
            while (output.peek(sample) && sample.at < at) {
                output.pop(sample);
                ++stale;
            }
            const bool available = ready.load(std::memory_order_relaxed) && output.peek(sample) && sample.at == at;
            if (available)
                output.pop(sample);
            else {
#ifdef SILVER_COMMAND_AUDIO_TRACE
                if (trace && !missing)
                    timing.firstMissing = at;
#endif
                ++missing;
            }
            for (size_t channel = 0; channel < block.outputs.size(); ++channel)
                if (auto* out = block.outputs[channel])
                    out[at - first] = available ? sample.value[channel] : 0;
        }
        MidiMessage message;
        while (ready.load(std::memory_order_relaxed) && midiOutput.peek(message) && message.at < end) {
            const auto offset = uint32_t(message.at < first ? 0 : message.at - first);
            if (destination.write && !destination.write(destination.context, {offset, message.bytes.data(), message.size}))
                break;
            midiOutput.pop(message);
        }
        if (model == Model::KS) {
            DrumPlayheadSample next, latest;
            bool moved = false;
            while (drumPlayheadOutput.peek(next) && next.at < end) {
                drumPlayheadOutput.pop(latest);
                moved = true;
            }
            if (moved)
                audibleDrumPlayheads.store(latest.playheads, std::memory_order_release);
        }
        missingFrames.fetch_add(missing, std::memory_order_relaxed);
        staleFrames.fetch_add(stale, std::memory_order_relaxed);
#ifdef SILVER_COMMAND_AUDIO_TRACE
        if (trace)
            timing.release = CommandAudioTrace::now();
#endif
        consumed.store(end, std::memory_order_release);
        acceptingBlock.store(false, std::memory_order_release);
        wake.notify();
#ifdef SILVER_COMMAND_AUDIO_TRACE
        if (trace) {
            timing.end = CommandAudioTrace::now();
            timing.midiBytes = accepted;
            timing.missing = missing;
            timing.stale = stale;
            trace->callback(timing);
        }
#endif
        if (block.offline)
            waitForOutput(end + lead);
    }

    void waitForOutput(uint64_t at) const noexcept {
        while (ready.load(std::memory_order_acquire) && rendered.load(std::memory_order_acquire) < at)
            std::this_thread::yield();
    }

    Archive saveState() {
        std::future<Archive> result;
        {
            std::lock_guard lock(saveMutex);
            if (!ready.load(std::memory_order_acquire) || saveRequest)
                return {};
            saveRequest.emplace();
            result = saveRequest->get_future();
            savePending.store(true, std::memory_order_release);
        }
        wake.notify();
        if (result.wait_for(std::chrono::seconds(2)) != std::future_status::ready)
            return {};
        return result.get();
    }

    void finishSave(Archive archive) {
        std::lock_guard lock(saveMutex);
        if (saveRequest) {
            saveRequest->set_value(std::move(archive));
            saveRequest.reset();
            savePending.store(false, std::memory_order_release);
        }
    }

    bool setParallelDsp(bool enabled) {
        std::future<bool> result;
        {
            std::lock_guard lock(parallelMutex);
            if (!ready.load(std::memory_order_acquire) || parallelRequest)
                return false;
            parallelRequest.emplace();
            parallelRequest->enabled = enabled;
            result = parallelRequest->completion.get_future();
            parallelPending.store(true, std::memory_order_release);
        }
        wake.notify();
        return result.get();
    }

    void finishParallelRequest(CommandAudioEngine* engine) {
        std::lock_guard lock(parallelMutex);
        if (!parallelRequest) return;
        bool applied = false;
        try {
            if (engine) applied = engine->setParallelDsp(parallelRequest->enabled);
        } catch (...) {
            // Leave the running backend available if starting the helper failed.
        }
        parallelRequest->completion.set_value(applied);
        parallelRequest.reset();
        parallelPending.store(false, std::memory_order_release);
    }

    void run(Factory& make, std::promise<void>& started) noexcept {
        bool primed = false;
        try {
            using dsp56k::ThreadPriority;
            using dsp56k::ThreadTools;
            ThreadTools::setCurrentThreadName("Silver-K command DSP");
            ThreadTools::setCurrentThreadPriority(ThreadPriority::High);
#ifdef SILVER_P12_ACTION_TRACE
            P12ActionTrace::worker().select(0);
#endif
            auto engine = make(format);
            model = engine->editorState().model;
            engine->setMidiSink({this, [](void* context, const uint8_t* bytes, size_t size) noexcept {
                static_cast<Worker*>(context)->emitMidi(bytes, size);
            }});
            try {
                render(*engine, started, primed);
            } catch (...) {
                engine->stop();
                throw;
            }
            engine->stop();
        } catch (...) {
            if (!primed)
                started.set_exception(std::current_exception());
            else {
                workerError = std::current_exception();
                fail(Error::Worker);
            }
        }
        editor.publish(std::nullopt);
        audioEditor.publish(std::nullopt);
        finishSave({});
        finishParallelRequest(nullptr);
    }

    void emitMidi(const uint8_t* bytes, size_t size) noexcept {
        if (!size || size > MaxSysExBytes) {
            outputOverflows.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        MidiMessage message{producing, size, {}};
        std::copy_n(bytes, size, message.bytes.data());
        if (midiOutput.push(message))
            outputEvents.fetch_add(1, std::memory_order_relaxed);
        else
            outputOverflows.fetch_add(1, std::memory_order_relaxed);
    }

    void render(CommandAudioEngine& engine, std::promise<void>& started, bool& primed) {
        const auto nativeRate = engine.nativeRate();
        AudioInput adc(format.sampleRate, nativeRate);
        OutputResampler<4> resampler;
#ifdef SILVER_P12_NOTE_TRACE
        auto& noteTrace = P12NoteTrace::worker();
#endif
        const auto renderDsp = [&](double phase) {
            const auto audio = engine.render(adc.sample(phase));
#ifdef SILVER_P12_NOTE_TRACE
            noteTrace.audio(audio);
#endif
            return audio;
        };
        resampler.prime([&] { return renderDsp(0); });
        uint64_t produced = 0;
        NativeDrumPlayheads lastDrumPlayheads{};
        while (!stopping.load(std::memory_order_acquire)) {
            if (parallelPending.load(std::memory_order_acquire))
                finishParallelRequest(&engine);
            const auto limit = consumed.load(std::memory_order_acquire) + lead;
            if (produced < limit) {
#ifdef SILVER_COMMAND_AUDIO_TRACE
                CommandAudioTrace::Render timing{};
                if (trace)
                    timing = {CommandAudioTrace::now(), 0, 0, produced, limit, output.size()};
#endif
                while (produced < limit && !stopping.load(std::memory_order_relaxed)) {
                    producing = produced;
#ifdef SILVER_P12_ACTION_TRACE
                    P12ActionTrace::worker().select(produced);
#endif
#ifdef SILVER_P12_NOTE_TRACE
                    noteTrace.select(produced);
#endif
                    deliverInputs(produced, engine);
                    InputSample sample{};
                    const bool present = input.peek(sample) && sample.at == produced;
                    if (present)
                        input.pop(sample);
                    adc.push(present ? sample.value : 0.0f);
                    const auto frame = resampler.next(nativeRate / format.sampleRate, renderDsp);
                    if (produced >= consumed.load(std::memory_order_relaxed) && !output.push({produced, frame}))
                        throw std::runtime_error("Command host output overflow");
                    ++produced;
                }
#ifdef SILVER_COMMAND_AUDIO_TRACE
                if (trace)
                    timing.lastSample = CommandAudioTrace::now();
#endif
                const auto state = engine.editorState();
                // The worker renders ahead. Publish the change when that
                // region of audio is handed to the host.
                if (state.drumPlayheads != lastDrumPlayheads &&
                    drumPlayheadOutput.push({produced - 1, state.drumPlayheads}))
                    lastDrumPlayheads = state.drumPlayheads;
                for (size_t i = 0; i < parts.size(); ++i) {
                    const auto& part = state.parts[i];
                    const auto channel = state.activeMask & (1u << i) ? part.channel : 16u;
                    parts[i].store(channel | (uint32_t(part.cutoff) << 8) | (uint32_t(part.volume) << 16),
                                   std::memory_order_release);
                }
                editor.publish(state);
                audioEditor.publish(state);
                rendered.store(produced, std::memory_order_release);
#ifdef SILVER_COMMAND_AUDIO_TRACE
                if (trace) {
                    timing.end = CommandAudioTrace::now();
                    timing.limit = produced;
                    trace->render(timing);
                }
#endif
            }
            if (!primed) {
                if (output.size() < lead)
                    throw std::runtime_error("Command output did not reach its startup reserve");
#ifdef SILVER_P12_STATE_SIZE
                constexpr auto queueBytes = sizeof(input) + sizeof(output) + sizeof(midi) + sizeof(controls) +
                    sizeof(midiOutput) + sizeof(positions);
                p12StateSizes("primed", model == Model::K ? "K-worker" : "KS-worker", {
                    {"mutable", "worker_bookkeeping", sizeof(Worker) - queueBytes},
                    {"mutable", "input_filter", sizeof(adc)}, {"mutable", "output_resampler", sizeof(resampler)},
                    {"queue", "audio_in", sizeof(input)}, {"queue", "audio_out", sizeof(output)},
                    {"queue", "midi_in", sizeof(midi)}, {"queue", "midi_out", sizeof(midiOutput)},
                    {"queue", "controls", sizeof(controls)}, {"queue", "host_positions", sizeof(positions)},
                    {"detail", "worker", sizeof(Worker)}});
#endif
#ifdef SILVER_P12_ACTION_TRACE
                P12ActionTrace::worker().primed(produced, output.size(), lead);
#endif
                dsp56k::ThreadTools::setCurrentThreadRealtimeParameters(int(format.sampleRate),
                    int(std::max(64u, format.maximumBlock)));
                ready.store(true, std::memory_order_release);
                started.set_value();
                primed = true;
            }
            if (savePending.load(std::memory_order_acquire)) {
                Archive archive;
                if (!acceptingBlock.load(std::memory_order_acquire) && !midi.size() &&
                    !positions.size() && !controls.size() && engine.checkpointReady()) {
                    auto encoded = engine.saveState();
                    if (encoded.empty())
                        throw std::runtime_error("Command archive could not be encoded");
                    archive = std::move(encoded);
                }
                finishSave(std::move(archive));
            }
#ifdef SILVER_COMMAND_AUDIO_TRACE
            const auto waitStart = trace ? CommandAudioTrace::now() : 0;
            const auto result =
#endif
            wake.waitWhile([&] {
                return !stopping.load(std::memory_order_acquire) && !savePending.load(std::memory_order_acquire) &&
                    !parallelPending.load(std::memory_order_acquire) &&
                    produced >= consumed.load(std::memory_order_acquire) + lead;
            });
#ifdef SILVER_COMMAND_AUDIO_TRACE
            if (trace)
                trace->wait({waitStart, CommandAudioTrace::now(), produced, int(result)});
#endif
        }
    }

    void deliverInputs(uint64_t at, CommandAudioEngine& engine) {
#ifdef SILVER_P12_CPU_SCOPES
        p12::CpuScope ingressCpu(p12::CpuRegion::Ingress);
#endif
        HostPosition position{};
        while (positions.peek(position) && position.at <= at) {
            if (!engine.enqueueHostPosition(position.value))
                throw std::runtime_error("Native clock input capacity");
            positions.pop(position);
            hostPositions.fetch_add(1, std::memory_order_relaxed);
        }
        // Keep an unaccepted byte at the front. The next DSP frame can free
        // bounded decoder storage. Complete messages retain their input order.
        InputEvent event{};
        for (size_t i = 0; i < MaxSysExBytes && midi.peek(event) && event.at <= at; ++i) {
            const auto accepted = event.raw ? engine.enqueueMidiByte(event.byte) : engine.enqueueControl(event.control);
            if (!accepted)
                break;
#ifdef SILVER_P12_NOTE_TRACE
            if (event.raw)
                P12NoteTrace::worker().midi(event.at, event.byte);
#endif
#ifdef SILVER_P12_ACTION_TRACE
            if (event.raw)
                P12ActionTrace::worker().hostByte(event.at, event.byte);
#endif
            midi.pop(event);
        }
    }

    const Format format;
    const unsigned lead;
    kstation::Queue<InputSample, AudioQueueCapacity> input;
    kstation::Queue<OutputSample, AudioQueueCapacity> output;
    kstation::Queue<InputEvent, 32768> midi;
    static constexpr size_t ControlCapacity = 256;
    kstation::Queue<ControllerEvent, ControlCapacity> controls;
    kstation::Queue<MidiMessage, 64> midiOutput;
    kstation::Queue<DrumPlayheadSample, 64> drumPlayheadOutput;
    std::atomic<NativeDrumPlayheads> audibleDrumPlayheads{NativeDrumPlayheads{}};
    std::mutex controlProducers;
    uint64_t producing = 0;
    Model model = Model::K;
    kstation::Queue<HostPosition, 2048> positions;
    HostPosition lastPosition{};
    NativeEditorMailbox editor, audioEditor;
    std::array<std::atomic<uint32_t>, 4> parts{};
    baseLib::WorkerWakeup wake;
    std::atomic<bool> ready{false}, stopping{false}, acceptingBlock{false};
    std::atomic<Error> error{Error::None};
    std::atomic<uint64_t> consumed{0}, rendered{0}, midiBytes{0}, hostPositions{0}, missingFrames{0}, staleFrames{0};
    std::atomic<uint64_t> controlEvents{0}, outputEvents{0}, outputOverflows{0};
    std::atomic<bool> savePending{false};
    std::mutex saveMutex;
    std::optional<std::promise<Archive>> saveRequest;
    struct ParallelRequest {
        bool enabled = false;
        std::promise<bool> completion;
    };
    std::atomic<bool> parallelPending{false};
    std::mutex parallelMutex;
    std::optional<ParallelRequest> parallelRequest;
    std::exception_ptr workerError;
#ifdef SILVER_COMMAND_AUDIO_TRACE
    const std::unique_ptr<CommandAudioTrace> trace = CommandAudioTrace::start();
#endif
    std::thread thread;
};

CommandAudioDevice::CommandAudioDevice(Format format, Factory make)
    : worker_(std::make_unique<Worker>(format, std::move(make))) {}

CommandAudioDevice::~CommandAudioDevice() = default;

void CommandAudioDevice::process(const Block& block, MidiInput input) noexcept { process(block, input, {}); }
void CommandAudioDevice::process(const Block& block, MidiInput input, MidiOutput output) noexcept {
    worker_->process(block, input, output);
}
bool CommandAudioDevice::postControls(const ControllerEvent* events, size_t count) {
    return worker_->postControls(events, count);
}
unsigned CommandAudioDevice::latency() const noexcept { return worker_->lead; }
CommandAudioDevice::Error CommandAudioDevice::error() const noexcept { return worker_->error.load(std::memory_order_acquire); }
const char* CommandAudioDevice::workerWaitBackend() const noexcept { return worker_->wake.backend(); }

void CommandAudioDevice::rethrowWorkerError() const {
    if (error() == Error::Worker)
        std::rethrow_exception(worker_->workerError);
}

CommandAudioDevice::Metrics CommandAudioDevice::metrics() const noexcept {
    return {worker_->consumed.load(), worker_->rendered.load(), worker_->midiBytes.load(), worker_->hostPositions.load(),
            worker_->missingFrames.load(), worker_->staleFrames.load(), worker_->controlEvents.load(),
            worker_->outputEvents.load(), worker_->outputOverflows.load()};
}

std::optional<NativeEditorState> CommandAudioDevice::editorState() noexcept { return worker_->editor.read(); }
std::optional<NativeEditorState> CommandAudioDevice::audioEditorState() noexcept { return worker_->audioEditor.read(); }
NativeDrumPlayhead CommandAudioDevice::drumPlayhead(unsigned part) const noexcept {
    if (part >= NativeDrumPlayheads{}.size() || !worker_->ready.load(std::memory_order_acquire))
        return {};
    return worker_->audibleDrumPlayheads.load(std::memory_order_acquire)[part];
}
std::optional<CommandAudioDevice::PartControls> CommandAudioDevice::partControls(unsigned part) const noexcept {
    if (part >= worker_->parts.size() || !worker_->ready.load(std::memory_order_acquire))
        return {};
    const auto value = worker_->parts[part].load(std::memory_order_acquire);
    return uint8_t(value) < 16 ? std::optional{PartControls{uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16)}}
                              : std::nullopt;
}
std::optional<uint8_t> CommandAudioDevice::partChannel(unsigned part) const noexcept {
    if (const auto controls = partControls(part))
        return controls->channel;
    return {};
}
Archive CommandAudioDevice::saveState() { return worker_->saveState(); }
bool CommandAudioDevice::setParallelDsp(bool enabled) { return worker_->setParallelDsp(enabled); }
}
