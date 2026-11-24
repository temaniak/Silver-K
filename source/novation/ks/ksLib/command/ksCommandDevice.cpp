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
    KSCommandEngine(CommandAudioFormat format, const Rom& rom, const std::vector<uint8_t>& wave,
                    std::vector<uint8_t> bytes, const KSUserDrumPatterns& userDrums,
                    std::shared_ptr<KSDesktopDrums> drums)
        : format_(format), state_(readKSCommandState(std::move(bytes), wave)), flash_(state_.flash),
          pair_(std::make_unique<PhysicalDspPair>()), drums_(std::move(drums)) {
        auto seed = bootKSCommandController(*pair_, rom, wave, state_, flash_);
        seed.userDrums = userDrums;
        controller_ = std::make_unique<research::PhysicalCandidate>(*pair_, seed, flash_);
        controller_->enableArpeggiator(format.sampleRate);
        if (state_.checkpoint)
            controller_->restoreCheckpoint(*state_.checkpoint);
        if (format.parallelDsp) setParallelDsp(true);
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
    NativeEditorState editorState() const override {
        for(size_t index=0;index<drums_->banks.size();++index) {
            const auto revision=flash_.drumBankRevision(index);
            if(revision==publishedDrums_[index])continue;
            std::optional<KSDrumBank> bank(std::in_place);
            std::memcpy(bank->data(),flash_.drumBankData(uint8_t(index+5)),sizeof(KSDrumBank));
            drums_->banks[index].publish(bank);
            publishedDrums_[index]=revision;
        }
        return controller_->editorState();
    }
    bool checkpointReady() const noexcept override {
        return pending_ == state_.pendingControl.size() && controller_->checkpointReady();
    }
    std::vector<uint8_t> saveState() const override {
        return saveKSCommandState(state_.resourceHash, flash_, *controller_, {}, controller_->checkpoint());
    }
    bool setParallelDsp(bool enabled) override {
        if (enabled)
            pair_->enableWorkers(int(format_.sampleRate), format_.maximumBlock,
                                 PhysicalDspPair::WorkerDispatch::SerialSlots);
        else
            pair_->disableWorkers();
        return true;
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
    const CommandAudioFormat format_;
    KSCommandState state_;
    KSProgramFlash flash_;
    std::unique_ptr<PhysicalDspPair> pair_;
    const std::shared_ptr<KSDesktopDrums> drums_;
    mutable std::array<uint64_t,4> publishedDrums_{};
    std::unique_ptr<research::PhysicalCandidate> controller_;
    size_t pending_ = 0;
};
}

KSCommandDevice::KSCommandDevice(Format format, Rom rom, std::vector<uint8_t> wave, std::vector<uint8_t> state,
                                 KSUserDrumPatterns userDrums)
    : KSCommandDevice(format,std::move(rom),std::move(wave),std::move(state),std::move(userDrums),
                      std::make_shared<KSDesktopDrums>()) {}

KSCommandDevice::KSCommandDevice(Format format,Rom rom,std::vector<uint8_t> wave,std::vector<uint8_t> state,
                                 KSUserDrumPatterns userDrums,std::shared_ptr<KSDesktopDrums> drums)
    : CommandAudioDevice(format, [rom = std::move(rom), wave = std::move(wave), state = std::move(state),
                                  userDrums,drums](Format f) mutable {
#ifdef SILVER_P12_STATE_SIZE
        p12StateSizes("initialized", "KS-factory", {{"immutable", "firmware_image", rom.data().capacity()},
                                                     {"immutable", "wave_rom", wave.capacity()}});
#endif
        return std::make_unique<KSCommandEngine>(f, rom, wave, std::move(state), userDrums, drums);
    }), drums_(std::move(drums)) {}

bool KSCommandDevice::loadDrumMap(const KSDesktopDrumMap& map,const std::function<bool(uint8_t)>& enqueue,
                                 std::shared_ptr<KSDesktopPatchExtras> extras) {
    return loadSound(map,enqueue,lastDrumLoad_,std::move(extras));
}

bool KSCommandDevice::loadPreset(const KSDesktopPreset& preset,const std::function<bool(uint8_t)>& enqueue,
                                std::shared_ptr<KSDesktopPatchExtras> extras) {
    return loadSound(preset,enqueue,lastPresetLoad_,std::move(extras));
}

bool KSCommandDevice::switchPart(KSDesktopPartSwitch edit,const std::function<bool(uint8_t)>& enqueue) {
    return loadSound(edit,enqueue,lastPartSwitch_);
}

bool KSCommandDevice::loadSound(const KSDesktopSound& sound,const std::function<bool(uint8_t)>& enqueue,
                                SoundLoad*& lastLoad,std::shared_ptr<KSDesktopPatchExtras> extras) {
    if(error()!=Error::None)return false;
    for(uint8_t index=0;index<drums_->loads.size();++index) {
        auto& slot=drums_->loads[index];
        if(&slot==lastDrumLoad_ || &slot==lastPresetLoad_ || &slot==lastPartSwitch_ ||
           slot.status.load(std::memory_order_acquire)==KSDesktopLoadStatus::Loading)continue;
        slot.sound=sound;
        slot.extras=std::move(extras);
        slot.status.store(KSDesktopLoadStatus::Loading,std::memory_order_release);
        if(!enqueue(index)) {
            slot.status.store(KSDesktopLoadStatus::Idle,std::memory_order_release);
            return false;
        }
        lastLoad=&slot;
        return true;
    }
    return false;
}

std::optional<KSDrumBank> KSCommandDevice::drumBank(uint8_t bank) {
    return bank>=5 && bank<=8 ? drums_->banks[bank-5].read() : std::nullopt;
}

KSDesktopLoadStatus KSCommandDevice::drumLoadStatus() const noexcept {
    return loadStatus(lastDrumLoad_);
}

KSDesktopLoadStatus KSCommandDevice::presetLoadStatus() const noexcept {
    return loadStatus(lastPresetLoad_);
}

bool KSCommandDevice::partSwitchPending(uint64_t snapshotRevision) const noexcept {
    const auto status=loadStatus(lastPartSwitch_);
    return status==KSDesktopLoadStatus::Loading ||
        (status==KSDesktopLoadStatus::Applied && snapshotRevision<lastPartSwitch_->appliedRevision);
}

KSDesktopLoadStatus KSCommandDevice::loadStatus(const SoundLoad* request) const noexcept {
    if(request && error()!=Error::None)return KSDesktopLoadStatus::Rejected;
    return request?request->status.load(std::memory_order_acquire):KSDesktopLoadStatus::Idle;
}
}
