#include "silverCommandProcessor.h"

#include "silverCommandBlock.h"
#include "silverKSData.h"
#include "ksLib/command/ksCommandDevice.h"
#include "ksLib/command/ksProgramFlash.h"
#include "ksLib/ksRom.h"
#include "kstationLib/command/kCommandDevice.h"
#include "kstationLib/command/kProgramFlash.h"
#include "kstationLib/kstationRomLoader.h"
#include "../../controller/command/controllerArchive.h"
#ifdef SILVER_P12_STATE_SIZE
#include "../../controller/command/p12StateSize.h"
#endif
#ifdef SILVER_P12_CPU_SCOPES
#include "../../controller/command/p12CpuScopes.h"
#endif
#include "baseLib/sha256.h"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace silverK {
namespace {
using namespace cmd;
constexpr const char* StateTag = "SilverCommandState";
constexpr size_t ArchiveLimit = 524288, PluginStateLimit = 4 * 1024 * 1024;

std::vector<uint8_t> bytes(const juce::File& file, size_t max) {
    juce::MemoryBlock block;
    const auto size = file.getSize();
    if (size <= 0 || uint64_t(size) > max || !file.loadFileAsData(block) || block.getSize() != size_t(size))
        throw std::runtime_error(("Cannot read " + file.getFullPathName()).toStdString());
    const auto* first = static_cast<const uint8_t*>(block.getData());
    return {first, first + block.getSize()};
}

std::vector<uint8_t> decodeBytes(const juce::String& text, size_t max) {
    if (text.isEmpty())
        return {};
    const auto separator = text.indexOfChar('.');
    if (separator <= 0 || separator > 10 || !text.substring(0, separator).containsOnly("0123456789"))
        throw std::runtime_error("Invalid saved controller length");
    const auto length = text.substring(0, separator).getLargeIntValue();
    const auto encoded = text.substring(separator + 1);
    if (length < 0 || uint64_t(length) > max || encoded.length() != (length * 8 + 5) / 6 ||
        !encoded.containsOnly(".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"))
        throw std::runtime_error("Invalid saved controller data");
    juce::MemoryBlock block;
    if (!block.fromBase64Encoding(text) || block.getSize() != size_t(length))
        throw std::runtime_error("Invalid saved controller data");
    if (!block.getSize())
        return {};
    const auto* first = static_cast<const uint8_t*>(block.getData());
    return {first, first + block.getSize()};
}

juce::File specifiedFile(const juce::ValueTree& state, const char* name) {
    const auto path = state.getProperty(name).toString();
    if (path.isEmpty())
        return {};
    if (!juce::File::isAbsolutePath(path))
        throw std::runtime_error("Saved resources require absolute file paths");
    return juce::File(path);
}

novation::controller::MusicalClock::Position hostPosition(juce::AudioPlayHead* playHead) {
    if (playHead)
        if (const auto info = playHead->getPosition())
            if (const auto bpm = info->getBpm()) {
                const auto ppq = info->getPpqPosition();
                return {true, info->getIsPlaying(), bool(ppq), *bpm, ppq ? *ppq : 0.0};
            }
    return {};
}

const char* errorText(CommandAudioDevice::Error error) {
    switch (error) {
    case CommandAudioDevice::Error::InvalidBlock: return "Host block exceeds its buffer";
    case CommandAudioDevice::Error::InvalidMidi: return "Invalid host MIDI event";
    case CommandAudioDevice::Error::MidiCapacity: return "MIDI input queue is full";
    case CommandAudioDevice::Error::AudioCapacity: return "Audio input queue is full";
    case CommandAudioDevice::Error::ClockCapacity: return "Transport queue is full";
    default: return "DSP worker stopped";
    }
}
}

class CommandProcessor::ControllerParameter final : public juce::RangedAudioParameter {
public:
    ControllerParameter(const char* id, const char* name, uint8_t cc, int initial)
        : RangedAudioParameter(juce::ParameterID{id, 1}, name), controller(cc), initial_(initial), value_(initial) {}
    const juce::NormalisableRange<float>& getNormalisableRange() const override { return range_; }
    float getValue() const override { return float(value_.load()) / 127; }
    float getDefaultValue() const override { return float(initial_) / 127; }
    void setValue(float value) override {
        if (std::isfinite(value)) {
            const auto sample = int(std::lround(std::clamp(value, 0.0f, 1.0f) * 127));
            value_.store(sample);
            pending_.store(sample);
        }
    }
    float getValueForText(const juce::String& text) const override {
        return float(std::clamp(text.getIntValue(), 0, 127)) / 127;
    }
    juce::String getText(float value, int maxLength) const override {
        const auto text = juce::String(int(std::lround(value * 127)));
        return maxLength > 0 ? text.substring(0, maxLength) : text;
    }
    std::optional<ControllerEvent> take(uint8_t channel, uint8_t part) noexcept {
        const auto value = pending_.exchange(-1);
        return value >= 0 ? std::optional{ControllerEvent{ControlChange{channel, controller, uint8_t(value)}, part}}
                          : std::nullopt;
    }
    void readback(uint8_t value) noexcept {
        if (pending_.load() < 0)
            value_.store(value);
    }
    void discard() noexcept { pending_.store(-1); }

private:
    juce::NormalisableRange<float> range_{0, 127, 1};
    const uint8_t controller;
    const int initial_;
    std::atomic<int> value_, pending_{-1};
};

CommandProcessor::CommandProcessor()
    : AudioProcessor(BusesProperties().withInput("External input", juce::AudioChannelSet::stereo(), false)
                                      .withOutput("Main", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Aux", juce::AudioChannelSet::stereo(), false)),
      message_("Choose firmware and load a controller"),
      cutoff_(new ControllerParameter("cutoff", "Filter cutoff", 105, 64)),
      volume_(new ControllerParameter("volume", "Volume", 7, 100)) {
    addParameter(cutoff_);
    addParameter(volume_);
    const auto initial = juce::SystemStats::getEnvironmentVariable("SILVER_COMMAND_STATE", {});
    if (juce::File::isAbsolutePath(initial)) {
        try {
            const auto state = bytes(juce::File(initial), PluginStateLimit);
            setStateInformation(state.data(), int(state.size()));
        } catch (const std::exception& error) {
            message_ = error.what();
        }
    }
}

CommandProcessor::~CommandProcessor() {
    std::lock_guard lock(control_);
    detachLocked(false);
}

bool CommandProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto mono = juce::AudioChannelSet::mono();
    const auto disabled = juce::AudioChannelSet::disabled();
    if (layout.outputBuses.size() != 2 || layout.inputBuses.size() != 1)
        return false;
    const auto input = layout.getMainInputChannelSet();
    const auto output = layout.getMainOutputChannelSet();
    const auto aux = layout.getChannelSet(false, 1);
    return (input == disabled || input == mono || input == stereo) &&
        ((output == stereo && (aux == disabled || aux == stereo)) ||
         (output == juce::AudioChannelSet::quadraphonic() && aux == disabled));
}

void CommandProcessor::reserveMidiLocked() {
    for (size_t i = 0; i < returnMidi_.size(); ++i) {
        returnMidi_[i].clear();
        returnMidi_[i].ensureSize(16 * 1024);
        reservedMidi_[i] = returnMidi_[i].data.getRawDataPointer();
    }
}

juce::MidiBuffer* CommandProcessor::returnBuffer() noexcept {
    for (auto& buffer : returnMidi_) {
        const auto* address = buffer.data.getRawDataPointer();
        if (address && std::find(reservedMidi_.begin(), reservedMidi_.end(), address) != reservedMidi_.end()) {
            buffer.clear();
            return &buffer;
        }
    }
    return nullptr;
}

void CommandProcessor::prepareToPlay(double rate, int maximum) {
    int latency = 0;
    {
        const std::lock_guard lock(control_);
        const auto block = unsigned(std::clamp(maximum, 1, int(CommandAudioDevice::MaximumHostBlock)));
        if (sampleRate_ != rate || maximumBlock_ != block || !engine_) {
            detachLocked(true);
            sampleRate_ = rate;
            maximumBlock_ = block;
            bootLocked();
        }
        if (engine_)
            latency = int(engine_->latency());
    }
    setLatencySamples(latency);
}

void CommandProcessor::releaseResources() {
    const std::lock_guard lock(control_);
    detachLocked(true);
    sampleRate_ = 0;
    maximumBlock_ = 0;
}

bool CommandProcessor::captureLocked() {
    if (!engine_)
        return true;
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    do {
        if (auto saved = engine_->saveState()) {
            archive_ = std::move(*saved);
#ifdef SILVER_P12_STATE_SIZE
            cmd::p12StateSizes("saved", resources_.model == Model::K ? "K-adapter" : "KS-adapter",
                               {{"archive", "retained_snapshot", archive_.capacity()}});
#endif
            return true;
        }
        if (engine_->error() != CommandAudioDevice::Error::None)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < until);
    message_ = "Controller state is busy. Save again after the pending MIDI completes.";
    return false;
}

void CommandProcessor::detachLocked(bool save) {
    if (save)
        captureLocked();
    audible_.store(nullptr);
    while (callbacks_.load())
        std::this_thread::yield();
    engine_.reset();
    cutoff_->discard();
    volume_->discard();
    keyboardChannels_.fill(std::nullopt);
}

void CommandProcessor::bootLocked() {
    if (!resources_.firmware.existsAsFile()) {
        message_ = resources_.firmware == juce::File{} ? "Choose firmware and load a controller"
                                                       : "The selected firmware file is missing";
        return;
    }
    if (!maximumBlock_) {
        message_ = "Controller will load when the host prepares audio";
        return;
    }
    try {
        // A standalone device can request several buffers back to back on
        // its first callback. Reserve for that burst before making audio live.
        const unsigned reserve = wrapperType == wrapperType_Standalone ? 2048u : 512u;
        const cmd::CommandAudioFormat format{sampleRate_, maximumBlock_, reserve, resources_.parallel};
        auto firmware = bytes(resources_.firmware, 200000);
        if (resources_.model == Model::K) {
            engine_ = std::make_unique<kstation::KCommandDevice>(format,
                kstation::Rom(std::move(firmware)), archive_);
        } else {
            std::vector<uint8_t> data = archive_;
            if (data.empty() && !readKSDataFiles({resources_.data}, data, message_))
                return;
            engine_ = std::make_unique<ks::KSCommandDevice>(format,
                ks::Rom(std::move(firmware)), bytes(resources_.wave, 129024), std::move(data));
        }
        reserveMidiLocked();
#ifdef SILVER_P12_STATE_SIZE
        cmd::p12StateSizes("prepared", resources_.model == Model::K ? "K-adapter" : "KS-adapter", {
            {"mutable", "plugin", sizeof(CommandProcessor)}, {"mutable", "parameter_objects", 2 * sizeof(ControllerParameter)},
            {"archive", "retained_snapshot", archive_.capacity()}, {"detail", "midi_reserve_request", returnMidi_.size() * 16 * 1024}});
#endif
        if (const auto panel = engine_->editorState(); panel && !panel->performance)
            selectPart(0);
        if (const auto part = engine_->partControls(selectedPart_.load())) {
            cutoff_->readback(part->cutoff);
            volume_->readback(part->volume);
        }
        message_.clear();
        audible_.store(engine_.get());
    } catch (const std::exception& error) {
        engine_.reset();
        message_ = error.what();
    }
}

void CommandProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals denormals;
    const RetiredCallback reading(callbacks_);
    auto* engine = audible_.load();
    if (!engine) {
        audio.clear();
        midi.clear();
        return;
    }
    std::array<ControllerEvent, 2> automation{};
    size_t count = 0;
    const auto part = selectedPart_.load();
    if (const auto channel = engine->partChannel(part))
        for (auto* parameter : {cutoff_, volume_})
            if (const auto change = parameter->take(*channel, uint8_t(part)))
                automation[count++] = *change;
    auto* output = returnBuffer();
    processCommandBlock(*engine, audio, midi, unsigned(getTotalNumInputChannels()), hostPosition(getPlayHead()),
                       isNonRealtime(), maximumBlock_, sampleRate_, automation.data(), count, output, true);
    if (output && !output->isEmpty())
        midi.swapWith(*output);
    else
        midi.clear();
    if (const auto controls = engine->partControls(part)) {
        cutoff_->readback(controls->cutoff);
        volume_->readback(controls->volume);
    }
}

CommandProcessor::Resources CommandProcessor::resources() const {
    const std::lock_guard lock(control_);
    return resources_;
}

bool CommandProcessor::activate(Resources resources) {
    int latency = 0;
    {
        const std::lock_guard lock(control_);
        detachLocked(false);
        resources_ = std::move(resources);
        archive_.clear();
        selectPart(0);
        bootLocked();
        if (engine_)
            latency = int(engine_->latency());
    }
    setLatencySamples(latency);
    return latency > 0;
}

juce::String CommandProcessor::status() const {
    const std::lock_guard lock(control_);
    if (engine_ && engine_->error() != CommandAudioDevice::Error::None) {
        try {
            engine_->rethrowWorkerError();
        } catch (const std::exception& error) {
            return error.what();
        }
        return juce::String(errorText(engine_->error())) + ". Reload the controller.";
    }
    if (message_.isNotEmpty())
        return message_;
    if (!engine_)
        return "Audio is stopped";
    const auto metrics = engine_->metrics();
    const auto name = resources_.model == Model::KS ? "KS" : "K";
    auto status = juce::String(name) + " ready   |   " + juce::String(engine_->latency()) + " frames of latency   |   " +
        juce::String(juce::int64(metrics.missingFrames)) + " missing frames";
    if (metrics.midiOutputOverflows)
        status += "   |   MIDI output queue overflow";
    return status;
}

std::optional<NativeEditorState> CommandProcessor::projection() {
    const std::lock_guard lock(control_);
    return engine_ ? engine_->editorState() : std::nullopt;
}

std::vector<CommandProcessor::ProgramEntry> CommandProcessor::catalog() {
    const std::lock_guard lock(control_);
    if (!engine_)
        return {};
    if (auto current = engine_->saveState())
        archive_ = std::move(*current);
    auto saved = decodeControllerArchive(archive_);
    if (!saved)
        return {};
    std::vector<ProgramEntry> result;
    const auto number = [](auto value) { return juce::String(value).paddedLeft('0', 2); };
    if (saved->model == Model::KS) {
        KSProgramFlash storage(saved->userFlash);
        for (uint8_t bank = 1; bank <= 8; ++bank)
            for (uint8_t slot = 0; slot < (bank > 4 ? 50 : 100); ++slot)
                if (const auto patch = storage.single({bank, slot})) {
                    const auto name = editorProgramName(patch->data() + 151);
                    result.push_back({bank, slot, false, number(slot) + "  " + juce::String(name.data()).trim()});
                }
        for (uint8_t slot = 0; slot < 100; ++slot)
            if (const auto scene = storage.performance(slot)) {
                const auto name = editorProgramName(scene->front().data() + 192);
                result.push_back({1, slot, true, number(slot) + "  " + juce::String(name.data()).trim()});
            }
    } else {
        for (uint8_t bank = 1; bank <= 4; ++bank)
            for (uint8_t slot = 0; slot < 100; ++slot) {
                const auto offset = *KStationCommandBackend::programOffset({bank, slot}) - KProgramFlash::UserOffset;
                const auto first = saved->userFlash.begin() + offset;
                if (std::all_of(first, first + KPackedProgramBytes, [](uint8_t byte) { return byte < 128; }))
                    result.push_back({bank, slot, false, number(slot)});
            }
    }
    return result;
}

bool CommandProcessor::selectProgram(unsigned bank, unsigned program, ProgramTarget kind) {
    const std::lock_guard lock(control_);
    const auto state = engine_ ? engine_->editorState() : std::optional<NativeEditorState>{};
    const auto part = selectedPart_.load();
    const bool performance = kind == ProgramTarget::Performance;
    if (!state || bank < 1 || bank > (state->model == Model::KS ? 8u : 4u) ||
        program >= (bank > 4 && !performance ? 50u : 100u) ||
        (performance && state->model != Model::KS))
        return false;
    const auto target = state->performance && kind == ProgramTarget::Part ? uint8_t(part) : RoutedByChannel;
    const auto channel = target == RoutedByChannel ? state->globalChannel : state->parts[part].channel;
    const auto midiBank = performance ? 65u : state->performance && kind == ProgramTarget::Single ? bank + 32 : bank;
    const std::array<ControllerEvent, 2> selection{
        ControllerEvent{ControlChange{channel, 32, uint8_t(midiBank)}, target},
        ControllerEvent{ProgramChange{channel, uint8_t(program)}, target}};
    return engine_->postControls(selection.data(), selection.size());
}

bool CommandProcessor::editControl(bool nrpn, uint8_t number, uint8_t value) {
    const std::lock_guard lock(control_);
    const auto part = selectedPart_.load();
    const auto channel = engine_ ? engine_->partChannel(part) : std::nullopt;
    if (!channel)
        return false;
    const std::array<ControllerEvent, 3> batch{
        ControllerEvent{ControlChange{*channel, 99, 0}, uint8_t(part)},
        ControllerEvent{ControlChange{*channel, 98, number}, uint8_t(part)},
        ControllerEvent{ControlChange{*channel, 6, value}, uint8_t(part)}};
    return nrpn ? engine_->postControls(batch.data(), batch.size())
                : engine_->postControl({ControlChange{*channel, number, value}, uint8_t(part)});
}

void CommandProcessor::keyboardNote(bool on, uint8_t note, uint8_t velocity) {
    const std::lock_guard lock(control_);
    if (!engine_ || note >= keyboardChannels_.size())
        return;
    auto& key = keyboardChannels_[note];
    if (on) {
        if (const auto channel = engine_->partChannel(selectedPart_.load()))
            if (engine_->postControl({NoteOn{*channel, note, velocity}}))
                key = channel;
    } else if (key) {
        if (engine_->postControl({NoteOff{*key, note, velocity}}))
            key.reset();
    }
}

void CommandProcessor::getStateInformation(juce::MemoryBlock& result) {
    const std::lock_guard lock(control_);
    captureLocked();
    juce::XmlElement state(StateTag);
    state.setAttribute("version", 1);
    state.setAttribute("model", resources_.model == Model::KS ? "KS" : "K");
    state.setAttribute("firmware", resources_.firmware.getFullPathName());
    state.setAttribute("wave", resources_.wave.getFullPathName());
    state.setAttribute("data", resources_.data.getFullPathName());
    state.setAttribute("parallel", resources_.parallel);
    state.setAttribute("part", int(selectedPart_.load()));
    state.setAttribute("archive", juce::MemoryBlock(archive_.data(), archive_.size()).toBase64Encoding());
    if (engine_) {
        const auto metrics = engine_->metrics();
        state.setAttribute("renderedFrames", juce::String(juce::int64(metrics.renderedFrames)));
        state.setAttribute("processedFrames", juce::String(juce::int64(metrics.processedFrames)));
        state.setAttribute("missingFrames", juce::String(juce::int64(metrics.missingFrames)));
        state.setAttribute("staleFrames", juce::String(juce::int64(metrics.staleFrames)));
        state.setAttribute("hostPositions", juce::String(juce::int64(metrics.hostPositions)));
        state.setAttribute("midiInputBytes", juce::String(juce::int64(metrics.midiBytes)));
        state.setAttribute("controlEvents", juce::String(juce::int64(metrics.controls)));
        state.setAttribute("midiOutputEvents", juce::String(juce::int64(metrics.midiOutputEvents)));
        state.setAttribute("midiOutputOverflows", juce::String(juce::int64(metrics.midiOutputOverflows)));
    }
#ifdef SILVER_P12_CPU_SCOPES
    state.setAttribute("p12CpuScopes", juce::String(cmd::p12::cpuSnapshot()));
#endif
    copyXmlToBinary(state, result);
}

void CommandProcessor::setStateInformation(const void* data, int size) {
    int latency = 0;
    {
        const std::lock_guard lock(control_);
        detachLocked(false);
        resources_ = {};
        archive_.clear();
        selectPart(0);
        try {
            const auto parsed = data && size > 0 && size_t(size) <= PluginStateLimit ? getXmlFromBinary(data, size) : nullptr;
            if (!parsed)
                throw std::runtime_error("Invalid Silver-K state");
            const auto state = juce::ValueTree::fromXml(*parsed);
            if (parsed->hasTagName(StateTag) && parsed->getIntAttribute("version") == 1) {
                const auto model = parsed->getStringAttribute("model");
                if (model != "K" && model != "KS")
                    throw std::runtime_error("Invalid controller model in state");
                resources_ = {model == "KS" ? Model::KS : Model::K, specifiedFile(state, "firmware"),
                    specifiedFile(state, "wave"), specifiedFile(state, "data"), parsed->getBoolAttribute("parallel")};
                selectPart(unsigned(std::clamp(parsed->getIntAttribute("part"), 0, 3)));
                archive_ = decodeBytes(parsed->getStringAttribute("archive"), ArchiveLimit);
            } else if (parsed->hasTagName("SilverParameters")) {
                const bool k = bool(state.getProperty("kModel", false));
                resources_ = {k ? Model::K : Model::KS, specifiedFile(state, "os"), specifiedFile(state, "bank"),
                    specifiedFile(state, "flash"), bool(state.getProperty("ksTwoCoreProcessing", false))};
                if (k) {
                    archive_ = decodeBytes(state.getProperty("kState").toString(), ArchiveLimit);
                    if (archive_.empty())
                        throw std::runtime_error("K legacy state has no controller archive");
                } else {
                    auto flash = decodeBytes(state.getProperty("userFlash").toString(), KSUserFlashBytes);
                    auto program = decodeBytes(state.getProperty("program").toString(), 1038);
                    auto pending = decodeBytes(state.getProperty("pendingControlMidi").toString(), 65536);
                    const auto wantHash = state.getProperty("bankHash").toString();
                    if (baseLib::SHA256(bytes(resources_.wave, 129024)).toString() != wantHash.toStdString())
                        throw std::runtime_error("Legacy KS Wave ROM differs from the saved resource");
                    archive_ = encodeControllerArchive({Model::KS, wantHash.toStdString(), std::move(flash),
                                                         std::move(program), std::move(pending)});
                    if (archive_.empty())
                        throw std::runtime_error("Invalid legacy KS controller data");
                    selectPart(unsigned(std::clamp(int(state.getProperty("selectedPart", 0)), 0, 3)));
                }
            } else {
                throw std::runtime_error("Unsupported Silver-K state version");
            }
            bootLocked();
        } catch (const std::exception& error) {
            resources_ = {};
            archive_.clear();
            message_ = error.what();
        }
        if (engine_)
            latency = int(engine_->latency());
    }
    setLatencySamples(latency);
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new silverK::CommandProcessor(); }
