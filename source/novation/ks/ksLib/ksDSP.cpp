#include "ksDSP.h"

#include "dsp56kEmu/jitconfig.h"

#include <algorithm>

namespace ks
{
    DSP::DSP()
        : m_memory(std::make_unique<dsp56k::Memory>(m_validator, 0x80000, 0x80000, 0x20000))
        , m_peripherals(nullptr)
        , m_dsp(std::make_unique<dsp56k::DSP>(*m_memory, &m_peripherals, &m_peripheralsY))
        , m_boot(*m_dsp)
    {
        auto config = m_dsp->getJit().getConfig();
        config.aguSupportBitreverse = true;
        config.dynamicPeripheralAddressing = true;
        config.dynamicFastInterrupts = true;
        config.support16BitSCMode = true;
        config.supportBranchAtLoopEnd = true;
        config.maxInstructionsPerBlock = 32;
        // The AArch64 emitter implements the bound with TST (limit-1), whose
        // immediate cannot be zero. Two remains a bounded peripheral time slice.
        config.maxDoIterations = 2;
        config.linkJitBlocks = false;
        // Routine uploads must invalidate compiled blocks without permanently
        // reducing written P ranges to single-instruction blocks.
        config.trackVolatilePMemory = false;
        m_dsp->getJit().setConfig(config);

        auto& esai = m_peripherals.getEsai();
        // Audio's default callbacks wait on its queues. Boot/wave construction
        // also advances ESAI, so a synchronous motherboard worker must provide
        // RX and consume TX even before an audio client starts rendering.
        esai.setReadRxCallback([this](uint64_t& index, dsp56k::Audio::RxFrame& frame)
        {
            frame.resize(2);
            for (size_t slot = 0; slot < 2; ++slot) frame[slot].fill(0);
            uint32_t sample = 0;
            if (m_inputRead != m_inputWrite)
                sample = static_cast<uint32_t>(m_input[m_inputRead++ % AudioCapacity]) & 0xffffff;
            frame[0][0] = sample;
            frame[1][0] = sample;
            if(serialRead)serialRead(frame);
            ++index;
        });
        esai.setWriteTxCallback([this](uint64_t& index, const dsp56k::Audio::TxFrame& frame)
        {
            ++index;
            if (frame.size() != 2) return;
            for(size_t s=0;s<2;++s)for(size_t t=0;t<6;++t)serialTx[s][t]=frame[s][t];
            if(serialWrite)serialWrite(serialTx);
            ++m_framesProduced;
            if (availableAudioFrames() >= AudioCapacity) { ++m_framesDropped; return; }
            // Diagnostic TX0 capture; the Board selects DAC streams from DMA routing.
            m_audio[m_audioWrite++ % AudioCapacity] = {signed24(frame[0][0]), signed24(frame[1][0])};
        });
        reset();
    }

    DSP::~DSP() = default;

    void DSP::reset()
    {
        m_dsp->resetHW();
        m_boot.reset();
        auto& clock = m_peripherals.getEsaiClock();
        clock.setExternalClockFrequency(ExternalClockHz);
        clock.setSamplerate(45340); // Exact frame rate is ClockHz / 3584.
        // TCCR: TPSR bypass, TPM=6, TFP=3, 32-bit slots.
        // Fosc / [2 * (TPM+1) * (TFP+1) * 32] = one serial slot.
        clock.setCyclesPerSample(SerialSlotCycles);
        clock.setClockSource(dsp56k::EsxiClock::ClockSource::Cycles);
        clock.restartClock();
        auto& host = m_peripherals.getHDI08();
        host.clearRX();
        while (host.hasTX()) host.readTX();
        host.writePortControlRegister(1u << dsp56k::HDI08::HPCR_HEN);
        host.setRXRateLimit(0);
        host.setTransmitDataAlwaysEmpty(false);
        host.setHostFlags(0, 0);
        m_audioRead = m_audioWrite = m_inputRead = m_inputWrite = 0;
        m_framesProduced = m_framesDropped = 0;
        m_hostTxWord = m_hostRxWord = 0;
        m_hostICR = m_hostCVR = 0;
        m_hostIVR = 0x0f;
        m_hostRxLatched = false;
    }

    uint8_t DSP::hostRead(const uint32_t byteOffset)
    {
        auto& host = m_peripherals.getHDI08();
        switch (byteOffset & 15)
        {
        case 0: return m_hostICR;
        case 2: return m_hostCVR;
        case 4:
            return static_cast<uint8_t>((m_hostRxLatched || host.hasTX() ? 1 : 0)
                // Model the host holding register, not the emulator's large
                // transport FIFO: hardware TXDE/TRDY falls until the DSP has
                // consumed the previous word. Otherwise a following command
                // flag can overtake an entire queued memory upload.
                | (!m_boot.finished() || !host.hasRXData() ? 6 : 0)
                | (host.readControlRegister() & 0x18));
        case 6: return m_hostIVR;
        case 10: case 12: case 14:
            if (!m_hostRxLatched)
            {
                if (!host.hasTX()) return 0;
                m_hostRxWord = host.readTX();
                m_hostRxLatched = true;
            }
            {
                const auto offset = byteOffset & 15;
                const uint8_t value = static_cast<uint8_t>(m_hostRxWord >> (offset == 10 ? 16 : offset == 12 ? 8 : 0));
                if (offset == 14) m_hostRxLatched = false;
                return value;
            }
        default: return 0;
        }
    }

    bool DSP::hostWrite(const uint32_t byteOffset, const uint8_t value)
    {
        auto& host = m_peripherals.getHDI08();
        switch (byteOffset & 15)
        {
        case 0:
            m_hostICR = value & 0x7f; // INIT is acknowledged by the host-interface hardware.
            host.setPendingHostFlags01(value & 0x18);
            return true;
        case 2:
            m_hostCVR = value & 0x7f;
            if (value & 0x80) m_dsp->injectExternalInterrupt((value & 0x1f) << 1);
            return true;
        case 6: m_hostIVR = value; return true;
        case 10: m_hostTxWord = (m_hostTxWord & 0xffff) | (uint32_t(value) << 16); return true;
        case 12: m_hostTxWord = (m_hostTxWord & 0xff00ff) | (uint32_t(value) << 8); return true;
        case 14:
            m_hostTxWord = (m_hostTxWord & 0xffff00) | value;
            return writeHostWord(m_hostTxWord);
        default: return true;
        }
    }

    bool DSP::writeHostWord(const uint32_t word)
    {
        if (!m_boot.finished()) { m_boot.hdiWriteTX(word & 0xffffff); return true; }
        auto& host = m_peripherals.getHDI08();
        if (host.hasRXData()) return false;
        host.writeRX(&word, 1);
        return true;
    }

    uint64_t DSP::step(const uint32_t instructionBudget)
    {
        if (!m_boot.finished() || !instructionBudget) return 0;
        const auto initialCycles = cycles();
        const auto initialInstructions = instructions();
        while (instructions() - initialInstructions < instructionBudget
            && availableAudioFrames() < AudioCapacity - 1)
            m_dsp->execJit();
        return cycles() - initialCycles;
    }

    void DSP::catchUpToCycle(const int64_t target)
    {
        while (int64_t(cycles()) < target)
        {
            discardAudio();
            step(1);
        }
    }

    bool DSP::popAudioFrame(AudioFrame& frame)
    {
        if (m_audioRead == m_audioWrite) return false;
        frame = m_audio[m_audioRead++ % AudioCapacity];
        return true;
    }

    void DSP::discardAudio() { m_audioRead = m_audioWrite; }

    bool DSP::pushAudioInput(const int32_t mono24)
    {
        if (m_inputWrite - m_inputRead >= AudioCapacity) return false;
        m_input[m_inputWrite++ % AudioCapacity] = mono24;
        return true;
    }

    int32_t DSP::signed24(const uint32_t word)
    {
        return word & 0x800000 ? static_cast<int32_t>(word & 0xffffff) - 0x1000000 : static_cast<int32_t>(word & 0xffffff);
    }
}
