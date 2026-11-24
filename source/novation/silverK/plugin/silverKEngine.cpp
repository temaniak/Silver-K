#include "silverKEngine.h"

#include "../../controller/command/controllerArchive.h"
#include "baseLib/sha256.h"
#include "ksLib/command/ksCommandDevice.h"
#include "ksLib/command/ksProgramFlash.h"
#include "ksLib/ksDspWorkers.h"
#include "ksLib/ksRom.h"
#include "kstationLib/command/kCommandDevice.h"
#include "kstationLib/kstationRomLoader.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace silverK {
namespace {
namespace command = novation::controller::command;

unsigned automationHoldoffFrames(double rate) {
    const auto* value = std::getenv("SILVER_EXPERIMENT_ARP_CONTROL_GUARD");
    return value && std::strcmp(value, "1") == 0 ? unsigned(rate * .040) : 0;
}

std::vector<uint8_t> ksStartupState(const std::vector<uint8_t>& wave, const std::vector<uint8_t>& flash,
                                    const std::vector<uint8_t>& program, const std::vector<uint8_t>& archive) {
    if (!archive.empty())
        return archive;
    if (program.empty())
        return flash;

    constexpr auto offset = command::KSProgramFlash::UserOffset;
    if (flash.size() != command::KSProgramFlash::FullBytes)
        throw std::runtime_error("Invalid KS Flash image");
    auto saved = command::encodeControllerArchive({
        command::Model::KS, baseLib::SHA256(wave).toString(),
        {flash.begin() + offset, flash.begin() + offset + command::KSUserFlashBytes}, program, {}, {}});
    if (saved.empty())
        throw std::runtime_error("Invalid saved KS program");
    return saved;
}
}

CommandEngine::CommandEngine(std::unique_ptr<Device> device, unsigned block, double rate)
    : device_(std::move(device)), maximumBlock_(block), hostRate_(rate) {}

unsigned CommandEngine::blockSize(int block) noexcept {
    return unsigned(std::clamp(block, 1, int(Device::MaximumHostBlock)));
}

bool CommandEngine::midi(const uint8_t* data, size_t size, uint32_t offset) noexcept {
    if (!data || !size)
        return false;
    const bool merge = eventCount_ && events_[eventCount_ - 1].size && events_[eventCount_ - 1].frame == offset;
    if (size > MidiCapacity - byteCount_ - typedCount_ || (!merge && eventCount_ == events_.size())) {
        rejected_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    std::copy_n(data, size, bytes_.begin() + byteCount_);
    if (merge)
        events_[eventCount_ - 1].size += unsigned(size);
    else
        events_[eventCount_++] = {offset, byteCount_, unsigned(size)};
    byteCount_ += unsigned(size);
    return true;
}

bool CommandEngine::orderedControl(ControllerEvent event, uint32_t offset) noexcept {
    if(byteCount_+typedCount_==MidiCapacity || eventCount_==events_.size()) {
        rejected_.fetch_add(1,std::memory_order_relaxed);
        return false;
    }
    events_[eventCount_++]={offset,0,0,event};
    ++typedCount_;
    return true;
}

bool CommandEngine::partMidi(unsigned part, const uint8_t* data, size_t size) noexcept {
    if (!data || !size || size % 3 || part >= ks::PartCount)
        return false;
    const auto count = size / 3;
    if (count > MidiCapacity - byteCount_ - typedCount_ || count > events_.size() - eventCount_) {
        rejected_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    for (size_t i = 0; i < size; i += 3)
        if ((data[i] & 0xf0) != 0xb0 || data[i + 1] >= 128 || data[i + 2] >= 128)
            return false;
    for (size_t i = 0; i < size; i += 3) {
        const command::ControlChange cc{uint8_t(data[i] & 15), data[i + 1], data[i + 2]};
        events_[eventCount_++] = {0, 0, 0, {cc, uint8_t(part)}};
    }
    typedCount_ += unsigned(count);
    return true;
}

void CommandEngine::render(const std::array<float*, 4>& output, unsigned frames, bool offline,
                           const float* left, const float* right,
                           HostClock::Position position, std::optional<HostLoop> loop) noexcept {
    struct Cursor {
        const CommandEngine& engine;
        unsigned index = 0, start = 0, end = 0, frames;
    } cursor{*this, 0, 0, 0, frames};

    const Device::MidiInput midi{&cursor, [](void* context, Device::MidiEvent& next) noexcept {
        auto& c = *static_cast<Cursor*>(context);
        if (c.index == c.engine.eventCount_)
            return false;
        const auto& event = c.engine.events_[c.index];
        const auto frame = std::min(event.frame, c.frames - 1);
        if (frame >= c.end)
            return false;
        ++c.index;
        next = {frame - c.start, event.size ? c.engine.bytes_.data() + event.firstByte : nullptr,
                event.size, event.size ? nullptr : &event.control};
        return true;
    }};
    const bool advancePosition = position.hasPpq && position.playing;
    const double ppqPerSample = loop && advancePosition && position.available ? position.bpm / (60 * hostRate_) : 0.;
    const bool looping = loop && ppqPerSample > 0 && std::isfinite(ppqPerSample) &&
                         std::isfinite(position.ppq) && std::isfinite(loop->startPpq) && std::isfinite(loop->endPpq) &&
                         loop->endPpq - loop->startPpq >= ppqPerSample && position.ppq <= loop->endPpq;
    unsigned positionStart = 0;
    while (cursor.start < frames) {
        auto count = std::min(frames - cursor.start, maximumBlock_);
        auto slicePosition = position;
        if (advancePosition)
            slicePosition.ppq += (cursor.start - positionStart) * position.bpm / (60 * hostRate_);
        bool wraps = false;
        if (looping) {
            // VST3 supplies a single position for the whole block. Publish
            // the loop start before admitting MIDI and clock ticks at the seam.
            if (loop->endPpq - slicePosition.ppq < ppqPerSample * 0.5) {
                slicePosition.ppq = position.ppq = loop->startPpq;
                positionStart = cursor.start;
            }
            const auto untilLoop = (loop->endPpq - slicePosition.ppq) / ppqPerSample;
            if (untilLoop < count + 0.5) {
                count = static_cast<unsigned>(std::lround(untilLoop));
                wraps = true;
            }
        }
        cursor.end = cursor.start + count;
        auto channels = output;
        for (auto*& channel : channels)
            if (channel)
                channel += cursor.start;
        device_->process({channels, left ? left + cursor.start : nullptr, right ? right + cursor.start : nullptr,
                          count, slicePosition, offline}, midi);
        cursor.start = cursor.end;
        if (wraps) {
            position.ppq = loop->startPpq;
            positionStart = cursor.start;
        }
    }
    if (frames)
        byteCount_ = eventCount_ = typedCount_ = 0;
}

std::optional<CommandEngine::EditorState> CommandEngine::editorState(bool audio) const noexcept {
    return audio ? device_->audioEditorState() : device_->editorState();
}

std::vector<uint8_t> CommandEngine::saveArchive() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    do {
        if (auto state = device_->saveState())
            return std::move(*state);
        if (device_->error() != Device::Error::None)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    return {};
}

uint64_t CommandEngine::overflows() const noexcept {
    return rejected_.load(std::memory_order_relaxed) + (device_->error() == Device::Error::MidiCapacity);
}

uint64_t CommandEngine::inputOverflows() const noexcept {
    return device_->error() == Device::Error::AudioCapacity;
}

std::string CommandEngine::failure() const {
    using Error = Device::Error;
    switch (device_->error()) {
    case Error::None: return {};
    case Error::InvalidBlock: return "Unsupported host audio block";
    case Error::InvalidMidi: return "Invalid host MIDI packet";
    case Error::MidiCapacity: return "Controller MIDI queue is full";
    case Error::AudioCapacity: return "Controller audio queue is full";
    case Error::ClockCapacity: return "Controller transport queue is full";
    case Error::Worker:
        try { device_->rethrowWorkerError(); }
        catch (const std::exception& error) { return error.what(); }
        catch (...) {}
        return "Controller worker stopped";
    }
    return "Controller worker stopped";
}

KEngine::KEngine(const std::vector<uint8_t>& rom, double rate, int block,
                 const std::vector<uint8_t>& state, unsigned floor)
    : CommandEngine(std::make_unique<kstation::KCommandDevice>(
                        Device::Format{rate, blockSize(block), floor}, kstation::Rom(rom), state),
                    blockSize(block), rate), arpAutomationInterval(automationHoldoffFrames(rate)) {}

bool KEngine::midi(const uint8_t* data, size_t size, uint32_t offset) noexcept {
    if (cutoffMidi_.duplicate(data, size))
        return true;
    const bool accepted = CommandEngine::midi(data, size, offset);
    if (accepted)
        cutoffMidi_.accepted(data, size);
    else
        cutoffMidi_.reset();
    return accepted;
}

bool KEngine::patch(std::array<uint8_t, 128>& patch, bool audio) const noexcept {
    const auto state = editorState(audio);
    if (!state || state->parts[0].parameterBytes != patch.size())
        return false;
    std::copy_n(state->parts[0].parameters.begin(), patch.size(), patch.begin());
    return true;
}

KsEngine::KsEngine(const std::vector<uint8_t>& os, const std::vector<uint8_t>& wave,
                   const std::vector<uint8_t>& flash, const std::vector<uint8_t>& program,
                   const std::vector<uint8_t>& archive, double rate, int block, unsigned floor, bool parallel,
                   const command::KSUserDrumPatterns& userDrums)
    : CommandEngine(std::make_unique<ks::KSCommandDevice>(Device::Format{rate, blockSize(block), floor, parallel},
                        ks::Rom(os), wave, ksStartupState(wave, flash, program, archive), userDrums), blockSize(block), rate),
      parallel_(parallel) {}

bool KsEngine::hostMidi(const uint8_t* data, size_t size, uint32_t offset) noexcept {
    if (data && size == 3 && data[1] < 128 && data[2] < 128) {
        // DAW notes are already complete messages. Admit them at their
        // sample time so a note on a clock edge reaches both parts in time.
        const auto channel = uint8_t(data[0] & 15);
        switch (data[0] & 0xf0) {
            case 0x80: return orderedControl({command::NoteOff{channel, data[1], data[2]}}, offset);
            case 0x90: return orderedControl({command::NoteOn{channel, data[1], data[2]}}, offset);
        }
    }
    return midi(data, size, offset);
}

bool KsEngine::loadDrumMap(const command::KSDesktopDrumMap& map,const std::function<bool(uint8_t)>& enqueue,
                          std::shared_ptr<command::KSDesktopPatchExtras> extras) {
    return static_cast<ks::KSCommandDevice&>(backend()).loadDrumMap(map,enqueue,std::move(extras));
}

void KsEngine::setDrumEditorSlot(uint8_t slot) noexcept {
    if(slot!=drumEditorSlot_ && orderedControl({command::KSDrumEditMode{slot}}))
        drumEditorSlot_=slot;
}

bool KsEngine::loadPreset(const command::KSDesktopPreset& preset,const std::function<bool(uint8_t)>& enqueue,
                         std::shared_ptr<command::KSDesktopPatchExtras> extras) {
    return static_cast<ks::KSCommandDevice&>(backend()).loadPreset(preset,enqueue,std::move(extras));
}

bool KsEngine::switchPart(command::KSDesktopPartSwitch edit,const std::function<bool(uint8_t)>& enqueue) {
    return static_cast<ks::KSCommandDevice&>(backend()).switchPart(edit,enqueue);
}

bool KsEngine::partSwitchPending(uint64_t snapshotRevision) const noexcept {
    return static_cast<const ks::KSCommandDevice&>(backend()).partSwitchPending(snapshotRevision);
}

void KsEngine::enqueueSoundLoad(uint8_t index) noexcept {
    auto& request=static_cast<ks::KSCommandDevice&>(backend()).soundLoad(index);
    if(!orderedControl({command::KSDesktopSoundCommand{&request}})) {
        if(request.extras)request.extras->status.store(command::KSDesktopLoadStatus::Rejected,std::memory_order_release);
        request.status.store(command::KSDesktopLoadStatus::Rejected,std::memory_order_release);
    }
}

std::optional<command::KSDrumBank> KsEngine::drumBank(uint8_t bank) {
    return static_cast<ks::KSCommandDevice&>(backend()).drumBank(bank);
}

command::KSDesktopLoadStatus KsEngine::drumLoadStatus() const noexcept {
    return static_cast<const ks::KSCommandDevice&>(backend()).drumLoadStatus();
}

command::KSDesktopLoadStatus KsEngine::presetLoadStatus() const noexcept {
    return static_cast<const ks::KSCommandDevice&>(backend()).presetLoadStatus();
}

ks::State KsEngine::fullSnapshot(bool audio) const noexcept {
    ks::State result;
    if (const auto state = editorState(audio)) {
        result.performance = state->performance;
        result.activeMask = state->activeMask;
        result.globalChannel = state->globalChannel;
        for (unsigned part = 0; part < ks::PartCount; ++part)
            result.parts[part] = state->parts[part].parameters;
    }
    return result;
}

std::vector<uint8_t> KsEngine::dumpProgram() const {
    auto state = fullSnapshot();
    for (auto& part : state.parts)
        ks::packMirrors(part);
    return ks::stateMessage(state);
}

std::vector<uint8_t> KsEngine::saveFlash() {
    if (auto state = command::decodeControllerArchive(saveArchive()))
        return std::move(state->userFlash);
    return {};
}

std::array<uint64_t, 3> KsEngine::diagnostics() const noexcept {
    const auto state = metrics();
    return {state.renderedFrames, state.processedFrames, state.midiBytes};
}

const char* KsEngine::dspExecutionBackend() const noexcept {
    return parallel_ ? ks::DspWorkers::backendName() : "sequential";
}

bool KsEngine::setParallelDsp(bool enabled) {
    if (!backend().setParallelDsp(enabled)) return false;
    parallel_ = enabled;
    return true;
}

}
