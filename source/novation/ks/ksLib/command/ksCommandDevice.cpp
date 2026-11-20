#include "ksCommandDevice.h"

#include "ksCommandStartup.h"
#include "../ksRom.h"
#include "../../../controller/command/dspSessionStop.h"
#ifdef SILVER_P12_STATE_SIZE
#include "../../../controller/command/p12DspStateSize.h"
#endif

#include <stdexcept>

namespace ks {
namespace {
using namespace novation::controller::command;

class KSCommandEngine final : public CommandAudioEngine {
public:
    KSCommandEngine(CommandAudioFormat format, const Rom& rom, const std::vector<uint8_t>& wave, std::vector<uint8_t> bytes)
        : state_(readKSCommandState(std::move(bytes), wave)), flash_(state_.flash), pair_(std::make_unique<PhysicalDspPair>()) {
        const auto seed = bootKSCommandController(*pair_, rom, wave, state_, flash_);
        controller_ = std::make_unique<research::PhysicalCandidate>(*pair_, seed, flash_);
        controller_->enableArpeggiator();
        if (state_.checkpoint)
            controller_->restoreCheckpoint(*state_.checkpoint);
        if (format.parallelDsp)
            pair_->enableWorkers(int(format.sampleRate), format.maximumBlock,
                                 PhysicalDspPair::WorkerDispatch::SerialSlots);
#ifdef SILVER_P12_STATE_SIZE
        size_t retainedCheckpoint = 0;
        size_t retainedPrefix = 0;
        if (state_.checkpoint) {
            const auto& saved = state_.checkpoint->midi;
            retainedCheckpoint = saved.capacity() * sizeof(KSQueuedMidi);
            for (const auto& item : saved)
                retainedCheckpoint += item.message.capacity();
            retainedPrefix = state_.checkpoint->decoder.sysExPrefix.capacity();
        }
        p12StateSizes("initialized", "KS-device", {
            {"mutable", "engine", sizeof(KSCommandEngine)}, {"mutable", "audio_facade", sizeof(KSCommandDevice)},
            {"mutable", "controller_facade", sizeof(research::PhysicalCandidate)},
            {"mutable", "resource_fingerprint", state_.resourceHash.capacity() + 1},
            {"mutable", "active_program_payload", state_.activeProgram.capacity()},
            {"mutable", "restored_midi", state_.pendingControl.capacity()},
            {"mutable", "retained_checkpoint_queue", retainedCheckpoint},
            {"mutable", "retained_decoder_prefix", retainedPrefix},
            {"preset", "flash", state_.flash.capacity()}, {"dsp", "physical_pair", sizeof(PhysicalDspPair)},
            {"detail", "state_inline", sizeof(KSCommandState)}, {"detail", "queued_midi_record", sizeof(KSQueuedMidi)},
            {"detail", "parallel_dsp", size_t(format.parallelDsp)}});
        reportDspSizes("initialized");
#endif
    }

    double nativeRate() const noexcept override { return DSP::SampleRate; }
    std::array<int32_t, 4> render(int32_t adc) override {
        for (unsigned budget = 32; budget && pending_ < state_.pendingControl.size(); --budget) {
            if (!controller_->enqueueMidiBytes(pair_->controllerCycle(), &state_.pendingControl[pending_], 1))
                break;
            ++pending_;
        }
        if (!pair_->dsp(0).pushAudioInput(adc) || !pair_->dsp(1).pushAudioInput(adc))
            throw std::runtime_error("KS command DSP input overflow");
        const auto frame = controller_->render();
        return {frame[0], frame[1], frame[2], frame[3]};
    }
    bool enqueueMidiByte(uint8_t byte) override {
        return pending_ == state_.pendingControl.size() && controller_->enqueueMidiBytes(pair_->controllerCycle(), &byte, 1);
    }
    bool enqueueControl(const ControllerEvent& event) override {
        return pending_ == state_.pendingControl.size() && controller_->enqueueControl(event);
    }
    bool enqueueHostPosition(novation::controller::MusicalClock::Position value) override {
        return controller_->enqueueHostPosition(value);
    }
    void setMidiSink(CommandMidiSink sink) noexcept override { controller_->setMidiSink(sink); }
    NativeEditorState editorState() const override { return controller_->editorState(); }
    bool checkpointReady() const noexcept override {
        return pending_ == state_.pendingControl.size() && controller_->checkpointReady();
    }
    std::vector<uint8_t> saveState() const override {
        return saveKSCommandState(state_.resourceHash, flash_, *controller_, {}, controller_->checkpoint());
    }
    void stop() override {
#ifdef SILVER_P12_STATE_SIZE
        reportDspSizes("stopping");
#endif
        controller_->shutdown();
    }

private:
#ifdef SILVER_P12_STATE_SIZE
    void reportDspSizes(const char* boundary) {
        p12DspStateSizes(boundary, "KS-DSP-A", pair_->dsp(0));
        p12DspStateSizes(boundary, "KS-DSP-B", pair_->dsp(1));
    }
#endif
    KSCommandState state_;
    KSProgramFlash flash_;
    std::unique_ptr<PhysicalDspPair> pair_;
    std::unique_ptr<research::PhysicalCandidate> controller_;
    size_t pending_ = 0;
};
}

KSCommandDevice::KSCommandDevice(Format format, Rom rom, std::vector<uint8_t> wave, std::vector<uint8_t> state)
    : CommandAudioDevice(format, [rom = std::move(rom), wave = std::move(wave), state = std::move(state)](Format f) mutable {
#ifdef SILVER_P12_STATE_SIZE
        p12StateSizes("initialized", "KS-factory", {{"immutable", "firmware_image", rom.data().capacity()},
                                                     {"immutable", "wave_rom", wave.capacity()}});
#endif
        return std::make_unique<KSCommandEngine>(f, rom, wave, std::move(state));
    }) {}
}
