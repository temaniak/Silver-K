#pragma once

#include "synthLib/device.h"
#include "kstationControllerTypes.h"
#include <memory>
#include <array>
#include "baseLib/performanceDiagnostics.h"

namespace kstation
{
    class Device final : public synthLib::Device
    {
    public:
        explicit Device(const synthLib::DeviceCreateParams& params,baseLib::performance::Diagnostics* performance=nullptr,
                        ControllerMode controller=ControllerMode::Original);
        ~Device() override;
        bool isValid() const override;
        std::string error() const;
        float getSamplerate() const override;
        uint32_t getChannelCountIn() override { return 1; }
        uint32_t getChannelCountOut() override { return 2; }
        uint32_t getInternalLatencyMidiToOutput() const override;
        uint32_t getInternalLatencyInputToOutput() const override;
        bool setDspClockPercent(uint32_t percent = 100) override { return percent == 100; }
        uint32_t getDspClockPercent() const override { return 100; }
        uint64_t getDspClockHz() const override;

#if SYNTHLIB_DEMO_MODE == 0
        bool getState(std::vector<uint8_t>& state, synthLib::StateType type) override;
        bool setState(const std::vector<uint8_t>& state, synthLib::StateType type) override;
#endif

        uint64_t getUnderrunCount() const;
        uint64_t getMidiOverflowCount() const;
        uint64_t getInputOverflowCount() const;
        const char* workerWaitBackend() const;
        const char* controllerBackend() const;
        void setBufferFloor(unsigned nativeFrames);
        void setAudioBuffer(uint32_t hostBlock,float hostRate,unsigned nativeFloor);
        void ensureAudioBlock(uint32_t hostBlock,float hostRate) noexcept;
        // Queue-only bridge for hosts that already own bounded MIDI storage.
        bool enqueueMidiBytes(const uint8_t*, size_t, uint32_t nativeOffset = 0);
        void setNonRealtime(bool enabled) noexcept;
        // Cold host preparation only: processing must be stopped. Refills on
        // the worker's suspended boundary; the audio callback never waits.
        bool prepareAudio(uint32_t maxHostBlock, float hostSamplerate);
        // Cached original H8 control state. Bounded atomic reads, no guest pause.
        bool getControlSnapshot(std::array<uint8_t,128>& patch,
                                std::array<uint8_t,16 * 128>& controllers,
                                uint64_t& revision) const noexcept;
        void process(const synthLib::TAudioInputs& inputs, const synthLib::TAudioOutputs& outputs, size_t samples,
                     const std::vector<synthLib::SMidiEvent>& midiIn, std::vector<synthLib::SMidiEvent>& midiOut) override;

    protected:
        void processAudio(const synthLib::TAudioInputs& inputs, const synthLib::TAudioOutputs& outputs, size_t samples) override;
        bool sendMidi(const synthLib::SMidiEvent& event, std::vector<synthLib::SMidiEvent>& response) override;
        void readMidiOut(std::vector<synthLib::SMidiEvent>& midiOut) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
