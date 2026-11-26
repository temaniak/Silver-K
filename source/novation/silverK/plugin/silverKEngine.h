#pragma once

#include "../../controller/command/commandAudioDevice.h"
#include "../../ks/ksLib/command/ksUserDrumPattern.h"
#include "../../ks/ksLib/command/ksDesktopDrumMap.h"
#include "ksLib/ksState.h"
#include "silverClock.h"
#include "silverKCutoffMidi.h"

#include <array>
#include <atomic>
#include <optional>
#include <string>

namespace silverK {

class CommandEngine {
public:
    static constexpr std::array<unsigned, 4> BufferFloors{0, 512, 1024, 2048};

    bool midi(const uint8_t*, size_t, uint32_t offset = 0) noexcept;
    bool partMidi(unsigned part, const uint8_t*, size_t) noexcept;
    void render(const std::array<float*, 4>&, unsigned frames, bool offline,
                const float* left, const float* right,
                HostClock::Position position = {}, std::optional<HostLoop> loop = {}) noexcept;
    std::vector<uint8_t> saveArchive();

    unsigned latency() const noexcept { return device_->latency(); }
    uint64_t underruns() const noexcept { return device_->metrics().missingFrames; }
    uint64_t overflows() const noexcept;
    uint64_t inputOverflows() const noexcept;
    const char* workerWaitBackend() const noexcept { return device_->workerWaitBackend(); }
    std::string failure() const;

protected:
    using Device = novation::controller::command::CommandAudioDevice;
    using ControllerEvent = novation::controller::command::ControllerEvent;
    using EditorState = novation::controller::command::NativeEditorState;

    CommandEngine(std::unique_ptr<Device>, unsigned block, double rate);
    std::optional<EditorState> editorState(bool audio) const noexcept;
    bool postControl(const ControllerEvent& event) { return device_->postControl(event); }
    Device& backend() const noexcept { return *device_; }
    bool orderedControl(ControllerEvent, uint32_t offset = 0) noexcept;
    Device::Metrics metrics() const noexcept { return device_->metrics(); }
    static unsigned blockSize(int block) noexcept;

private:
    // One producer at a time: initial cold setup, then the audio callback.
    // Keep same-sample packets contiguous and reuse this storage every block.
    struct Event {
        uint32_t frame;
        unsigned firstByte;
        unsigned size;
        ControllerEvent control{};
    };
    static constexpr unsigned MidiCapacity = 32767;
    std::array<uint8_t, MidiCapacity> bytes_;
    std::array<Event, 8192> events_;
    unsigned byteCount_ = 0, eventCount_ = 0, typedCount_ = 0;
    std::unique_ptr<Device> device_;
    unsigned maximumBlock_;
    double hostRate_;
    std::atomic<uint64_t> rejected_{0};
};

class KEngine final : public CommandEngine {
public:
    using CommandEngine::editorState;
    KEngine(const std::vector<uint8_t>& rom, double rate, int block,
            const std::vector<uint8_t>& state, unsigned floor);
    bool midi(const uint8_t*, size_t, uint32_t offset = 0) noexcept;
    bool patch(std::array<uint8_t, 128>&, bool audio = false) const noexcept;
    static constexpr const char* controllerBackend() { return "native-k-command"; }

    const unsigned arpAutomationInterval;
    std::array<unsigned, 4> arpAutomationRemaining{};
    size_t nextArpAutomation = 0;
    unsigned arpAutomationSpacing = 0;

private:
    KCutoffMidi cutoffMidi_;
};

class KsEngine final : public CommandEngine {
public:
    using CommandEngine::editorState;
    KsEngine(const std::vector<uint8_t>& os, const std::vector<uint8_t>& wave,
             const std::vector<uint8_t>& flash, const std::vector<uint8_t>& program,
             const std::vector<uint8_t>& archive, double rate, int block, unsigned floor, bool parallel,
             const novation::controller::command::KSUserDrumPatterns& userDrums = {});
    bool hostMidi(const uint8_t*, size_t, uint32_t offset) noexcept;
    bool editDrumGrid(novation::controller::command::KSDrumGridEdit edit) { return postControl({edit}); }
    void setDrumEditorSlot(uint8_t slot) noexcept;
    bool editDrumVoice(novation::controller::command::KSDrumVoiceEdit edit) noexcept { return orderedControl({edit}); }
    bool loadDrumMap(const novation::controller::command::KSDesktopDrumMap&,
                     const std::function<bool(uint8_t)>& enqueue,
                     std::shared_ptr<novation::controller::command::KSDesktopPatchExtras> = {});
    bool loadPreset(const novation::controller::command::KSDesktopPreset&,
                    const std::function<bool(uint8_t)>& enqueue,
                    std::shared_ptr<novation::controller::command::KSDesktopPatchExtras> = {});
    bool switchPart(novation::controller::command::KSDesktopPartSwitch,
                    const std::function<bool(uint8_t)>& enqueue);
    bool partSwitchPending(uint64_t snapshotRevision) const noexcept;
    void enqueueSoundLoad(uint8_t index) noexcept;
    std::optional<novation::controller::command::KSDrumBank> drumBank(uint8_t);
    novation::controller::command::KSDesktopLoadStatus drumLoadStatus() const noexcept;
    novation::controller::command::KSDesktopLoadStatus presetLoadStatus() const noexcept;
    auto drumPlayhead(unsigned part) const noexcept { return backend().drumPlayhead(part); }
    ks::State fullSnapshot(bool audio = false) const noexcept;
    std::vector<uint8_t> dumpProgram() const;
    std::vector<uint8_t> saveFlash();
    std::array<uint64_t, 3> diagnostics() const noexcept;
    const char* dspExecutionBackend() const noexcept;
    bool setParallelDsp(bool);
    static constexpr const char* controllerBackend() { return "native-ks-command"; }

private:
    bool parallel_;
    uint8_t drumEditorSlot_ = novation::controller::command::KSDrumEditMode::Off;
};

}
