#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <functional>

#include "dsp56kEmu/dsp.h"
#include "dsp56kEmu/dspBootCode.h"

namespace ks
{
    // Owned and advanced by one emulation worker. The host audio callback must
    // not execute step(): initial JIT compilation belongs to the worker.
    class DSP final
    {
    public:
        using AudioFrame = std::array<int32_t, 2>;
        static constexpr uint64_t ExternalClockHz = 25000000;
        // Original PCTL=0x1d000c: MF+1=13, PD+1=2, DF=1.
        static constexpr uint64_t ClockHz = ExternalClockHz * 13 / 2;
        static constexpr uint32_t SerialSlotCycles = 2 * 7 * 4 * 32;
        static constexpr double SampleRate = double(ClockHz) / (double(SerialSlotCycles) * 2);
        static constexpr size_t AudioCapacity = 8192;

        DSP();
        using SerialFrame = std::array<std::array<uint32_t,6>,2>;
        SerialFrame serialTx{};
        std::function<void(dsp56k::Audio::RxFrame&)> serialRead;
        std::function<void(const SerialFrame&)> serialWrite;
        ~DSP();
        DSP(const DSP&) = delete;
        DSP& operator=(const DSP&) = delete;

        void reset();
        uint8_t hostRead(uint32_t byteOffset);
        // False means a data word was rejected because the host ignored TXDE.
        bool hostWrite(uint32_t byteOffset, uint8_t value);
        uint64_t step(uint32_t instructionBudget = 256);
        // Same step(1)/discard sequence, contained in one translation unit so
        // the hot catch-up loop need not cross the wrapper boundary per block.
        void catchUpToCycle(int64_t target);
        bool bootFinished() const { return m_boot.finished(); }

        bool popAudioFrame(AudioFrame& frame);
        size_t availableAudioFrames() const { return m_audioWrite - m_audioRead; }
        void discardAudio();
        // RX0 is the mono external input. A missing input frame supplies silence.
        bool pushAudioInput(int32_t mono24);
        uint64_t audioFramesProduced() const { return m_framesProduced; }
        uint64_t audioFramesDropped() const { return m_framesDropped; }
        uint64_t cycles() const { return m_dsp->getCycles(); }
        uint64_t instructions() const { return m_dsp->getInstructionCounter(); }

        dsp56k::DSP& core() { return *m_dsp; }
        dsp56k::Memory& memory() { return *m_memory; }
        dsp56k::Peripherals56362& peripherals() { return m_peripherals; }

    private:
        bool writeHostWord(uint32_t word);
        static int32_t signed24(uint32_t word);

        dsp56k::DefaultMemoryValidator m_validator;
        std::unique_ptr<dsp56k::Memory> m_memory;
        dsp56k::Peripherals56362 m_peripherals;
        dsp56k::PeripheralsNop m_peripheralsY;
        std::unique_ptr<dsp56k::DSP> m_dsp;
        dsp56k::DspBoot m_boot;
        std::array<AudioFrame, AudioCapacity> m_audio{};
        std::array<int32_t, AudioCapacity> m_input{};
        size_t m_audioRead = 0, m_audioWrite = 0, m_inputRead = 0, m_inputWrite = 0;
        uint64_t m_framesProduced = 0, m_framesDropped = 0;
        uint32_t m_hostTxWord = 0, m_hostRxWord = 0;
        uint8_t m_hostICR = 0, m_hostCVR = 0, m_hostIVR = 0x0f;
        bool m_hostRxLatched = false;
    };
}
