#include "ksDSP.h"

#include "dsp56kEmu/jitconfig.h"
#ifdef SILVER_P12_ACTION_TRACE
#include "../../controller/command/p12ActionTrace.h"
#endif
#ifdef SILVER_P12_NOTE_TRACE
#include "../../controller/command/p12NoteTrace.h"
#endif

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
#ifdef SILVER_KS_DSP_JIT_NO_OPTIMIZER
        config.enableOptimizer = false;
#endif
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
        m_stagingWritesElided = 0;
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
                if (offset == 14) {
                    m_hostRxLatched = false;
#ifdef SILVER_P12_ACTION_TRACE
                    auto& action = novation::controller::command::P12ActionTrace::worker();
                    action.hdiReply(action.ksEndpoint(this), cycles(), m_hostRxWord);
#endif
                }
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
#ifdef SILVER_P12_ACTION_TRACE
        auto& action = novation::controller::command::P12ActionTrace::worker();
        action.hdiWord(action.ksEndpoint(this), cycles(), word, m_hostICR);
#endif
        return true;
    }

    uint64_t DSP::step(const uint32_t instructionBudget)
    {
        if (!m_boot.finished() || !instructionBudget) return 0;
        const auto initialCycles = cycles();
        const auto initialInstructions = instructions();
#ifdef SILVER_P12_ACTION_TRACE
        auto& action = novation::controller::command::P12ActionTrace::worker();
#endif
#ifdef SILVER_P12_NOTE_TRACE
        using novation::controller::command::P12NoteTrace;
        auto& trace = P12NoteTrace::worker();
#endif
        while (instructions() - initialInstructions < instructionBudget
            && availableAudioFrames() < AudioCapacity - 1)
        {
#ifdef SILVER_P12_NOTE_TRACE
            const auto mixer = trace.ksMixerStateAt(m_framesProduced);
            const auto beginPc = mixer ? m_dsp->getPC().toWord() : 0;
            const auto beginCycle = mixer ? cycles() : 0;
#endif
#ifdef SILVER_P12_ACTION_TRACE
            const auto watchingStore = action.hdiStoreEnabled() && !(m_hostICR & 8);
            const auto cursor = watchingStore ? m_memory->get(dsp56k::MemArea_X, 0) : 0u;
#endif
            m_dsp->execJit();
#ifdef SILVER_P12_ACTION_TRACE
            if (watchingStore && cursor < 0x8000 && m_memory->get(dsp56k::MemArea_X, 0) == cursor + 1) {
                const auto y = m_memory->get(dsp56k::MemArea_X, 1) & 0x8000;
                const auto word = m_memory->get(y ? dsp56k::MemArea_Y : dsp56k::MemArea_X, cursor);
                action.hdiStore(action.ksEndpoint(this), cycles(), cursor | y, word);
            }
            if (action.program() && !action.voiceDsp)
                action.mailbox(cycles(), m_framesProduced, m_memory->get(dsp56k::MemArea_Y, 0x41));
#endif
#ifdef SILVER_P12_NOTE_TRACE
            if (mixer) {
                // B's per-voice parameter, control and current mixer blocks have distinct strides.
                const auto slot = trace.ksMixerSlot();
                const auto parameterOffset = 101 * slot;
                const auto controlOffset = 22 * slot;
                const auto readX = [this](unsigned at) { return m_memory->get(dsp56k::MemArea_X, at); };
                const auto readY = [this](unsigned at) { return m_memory->get(dsp56k::MemArea_Y, at); };
                trace.ksMixerState(cycles(), m_framesProduced, beginCycle, beginPc, m_dsp->getPC().toWord(),
                                   {readX(0x55 + parameterOffset), readY(0x81 + controlOffset),
                                    readX(0x5cd + 0x2b * slot), readX(0x72 + parameterOffset),
                                    readX(0x7a + parameterOffset), readY(0x85 + controlOffset)});
            }
            if (trace.ksVoiceStateAt(m_framesProduced))
                trace.ksVoiceState(cycles(), m_framesProduced, m_dsp->getPC().toWord(),
                                   m_memory->get(dsp56k::MemArea_Y, 0x81),
                                   m_memory->get(dsp56k::MemArea_Y, 0x1ba));
            if (trace.active && trace.ksVoiceDsp)
                for (unsigned slot = 0; slot < 14; ++slot)
                {
                    const auto gate = 0x2b + 101 * slot;
                    const auto status = 0x85 + 22 * slot;
                    const auto read = [&](unsigned at) { return m_memory->get(dsp56k::MemArea_X, at); };
                    if (trace.word(P12NoteTrace::Word::KSGate, slot, gate,
                                   read(gate), cycles(), m_framesProduced))
                        trace.voiceImage(gate, 100, cycles(), m_framesProduced, read);
                    if (!trace.transport())
                        trace.word(P12NoteTrace::Word::KSStatus, slot, status,
                                   m_memory->get(dsp56k::MemArea_Y, status), cycles(), m_framesProduced);
                }
#endif
        }
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
