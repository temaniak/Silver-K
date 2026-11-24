#include "kCommandDevice.h"

#include "kCommandRuntime.h"
#include "kCommandStartup.h"
#include "../kstationRomLoader.h"
#ifdef SILVER_P12_STATE_SIZE
#include "../../../controller/command/p12DspStateSize.h"
#endif

namespace kstation {
namespace {
using namespace novation::controller::command;
using command::runtime::KCommandRuntime;

class KCommandEngine final : public CommandAudioEngine {
public:
    KCommandEngine(const Rom& rom, const std::vector<uint8_t>& archive)
        : state_(command::readKCommandState(rom, archive)), flash_(state_.flash) {
        if (!parameters_.bind(rom.data().data(), rom.data().size()))
            throw std::runtime_error("K immutable parameter table is invalid");
        bootKCommandDsp(physical_, rom.data());
        controller_ = std::make_unique<KCommandRuntime>(physical_, state_.seed(physical_.cycle()), flash_,
            state_.flash, state_.tables, parameters_, state_.selection);
        for (unsigned i = 0; !controller_->coldReady(); ++i) {
            if (i == 2048)
                throw std::runtime_error("K cold controller did not reach its ready boundary");
            controller_->render();
        }
#ifdef SILVER_P12_STATE_SIZE
        controller_->reportStateSize();
        p12StateSizes("initialized", "K-device", {
            {"mutable", "engine", sizeof(KCommandEngine)}, {"mutable", "audio_facade", sizeof(KCommandDevice)},
            {"mutable", "resource_fingerprint", state_.resourceHash.capacity() + 1},
            {"mutable", "restored_midi", state_.pending.capacity()}, {"preset", "flash", state_.flash.capacity()},
            {"detail", "state_inline", sizeof(command::KCommandState)}, {"detail", "rom_tables_inline", sizeof(state_.tables)}});
        p12DspStateSizes("initialized", "K-DSP", physical_.dsp());
#endif
    }

    double nativeRate() const noexcept override { return DSP::SampleRate; }
    std::array<int32_t, 4> render(int32_t adc) override {
        for (unsigned budget = 32; budget && pending_ < state_.pending.size(); --budget) {
            if (!controller_->enqueueMidiByte(state_.pending[pending_]))
                break;
            ++pending_;
        }
        if (!physical_.pushAudioInput(adc))
            throw std::runtime_error("K command DSP input overflow");
        const auto frame = controller_->render();
        return {frame[0], frame[1], 0, 0};
    }
    bool enqueueMidiByte(uint8_t byte) override {
        return pending_ == state_.pending.size() && controller_->enqueueMidiByte(byte);
    }
    bool enqueueControl(const ControllerEvent& event) override {
        return pending_ == state_.pending.size() && controller_->enqueueControl(event);
    }
    bool enqueueHostPosition(novation::controller::MusicalClock::Position value) override {
        return controller_->enqueueHostPosition(value);
    }
    void setMidiSink(CommandMidiSink sink) noexcept override { controller_->setMidiSink(sink); }
    NativeEditorState editorState() const override { return controller_->editorState(); }
    bool checkpointReady() const noexcept override {
        return pending_ == state_.pending.size() && controller_->checkpointReady();
    }
    std::vector<uint8_t> saveState() const override {
        return command::saveKCommandState(state_, controller_->selection(), controller_->program(), controller_->archiveMidi());
    }
    void stop() override {
#ifdef SILVER_P12_STATE_SIZE
        p12DspStateSizes("stopping", "K-DSP", physical_.dsp());
#endif
        controller_->shutdown();
    }

private:
    command::KCommandState state_;
    KProgramFlash flash_;
    KMidiParameters parameters_;
    command::PhysicalTimeline physical_;
    std::unique_ptr<KCommandRuntime> controller_;
    size_t pending_ = 0;
};
}

KCommandDevice::KCommandDevice(Format format, Rom rom, std::vector<uint8_t> archive)
    : CommandAudioDevice(format, [rom = std::move(rom), archive = std::move(archive)](Format) {
#ifdef SILVER_P12_STATE_SIZE
        p12StateSizes("initialized", "K-factory", {{"immutable", "firmware_image", rom.data().capacity()},
                                                    {"archive", "configuration_at_start", archive.capacity()}});
#endif
        return std::make_unique<KCommandEngine>(rom, archive);
    }) {}
}
