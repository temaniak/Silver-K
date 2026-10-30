#include "kstationHardware.h"
#include "kstationH8.h"
#include "kstationNativeController.h"

#include <stdexcept>
#include <utility>
#include "h8s/h8s.hpp"

namespace kstation
{
    namespace {
        class OriginalController final : public Controller {
        public:
            OriginalController(const std::vector<uint8_t>&rom,H8::HostCallbacks callbacks,
                               const std::vector<uint8_t>&flash,H8::Stage1Mode mode)
                :core_(rom,std::move(callbacks),flash,mode){}
            void reset()override{core_.reset();}
            uint64_t exec(uint32_t n)override{return core_.exec(n);}
            bool enqueueMidi(const uint8_t*p,size_t n)override{return core_.enqueueMidi(p,n);}
            bool popMidiOut(uint8_t&v)override{return core_.popMidiOut(v);}
            bool bootReady()const override{return core_.bootReady();}
            H8::DebugState debugState()const override{return core_.debugState();}
            uint8_t readMemory(uint32_t a)const override{return core_.readMemory(a);}
            void copyCurrentPatch(uint8_t*p)const override{core_.copyCurrentPatch(p);}
            std::vector<uint8_t>saveFlash()const override{return core_.saveFlash();}
            void copyFlash(std::vector<uint8_t>&out)const override{core_.copyFlash(out);}
            uint64_t flashRevision()const override{return core_.flashRevision();}
            bool hasProgram(uint8_t b,uint8_t p)const override{return core_.hasProgram(b,p);}
            const uint8_t*diagnosticMemory()override{return core_.core().memory;}
            uint16_t diagnosticRead16(uint32_t a)override{return core_.core().read16(int(a));}
            void diagnosticWrite8(uint32_t a,uint8_t v)override{core_.core().write8(int8_t(v),int(a));}
            void diagnosticWrite16(uint32_t a,uint16_t v)override{core_.core().write16(int16_t(v),int(a));}
            void diagnosticInterrupt(int vector)override{core_.core().interrupt(vector);}
            h8state&core()override{return core_.core();}
        private:H8 core_;
        };
        std::unique_ptr<Controller> makeController(const std::vector<uint8_t>&rom,H8::HostCallbacks callbacks,const std::vector<uint8_t>&flash,H8::Stage1Mode mode){
            if(mode==H8::Stage1Mode::Native)return std::make_unique<NativeController>(rom,std::move(callbacks),flash);
            return std::make_unique<OriginalController>(rom,std::move(callbacks),flash,mode);
        }
    }

    Hardware::Hardware(const std::vector<uint8_t>& rom, const std::vector<uint8_t>& flash, H8::Stage1Mode mode)
        : m_h8(makeController(rom, H8::HostCallbacks{
            [this](uint32_t offset) { return m_dsp.hostRead(offset); },
            [this](uint32_t offset, uint8_t value) { return m_dsp.hostWrite(offset, value); },
            [this](bool asserted)
            {
                if (asserted && !m_resetAsserted)
                {
                    m_dsp.reset();
                    m_dspStarted = false;
                }
                m_resetAsserted = asserted;
            }}, flash, mode))
    {
    }

    void Hardware::advance()
    {
        using namespace baseLib::performance;
        const bool sample=m_performance && m_performance->mode()==2 && ((++m_diagnosticAdvances&255)==0);
        Scope h8Timing(m_performance,Metric::H8,0,0,true,sample);
        m_h8Cycles += m_h8->exec(128);
        h8Timing.finish();
        if (!m_dsp.bootFinished() || m_resetAsserted) return;
        Scope dspTiming(m_performance,Metric::DspA,0,0,true,sample);
        if (!m_dspStarted)
        {
            m_dspEpoch = static_cast<int64_t>(m_h8Cycles * 7) - static_cast<int64_t>(m_dsp.cycles());
            m_dspStarted = true;
        }
        const auto target = static_cast<int64_t>(m_h8Cycles * 7) - m_dspEpoch;
        for (size_t iterations = 0; static_cast<int64_t>(m_dsp.cycles()) < target; ++iterations)
        {
            if (iterations == 10000) throw std::runtime_error("K-Station DSP did not advance its clock.");
            m_dsp.step(16);
        }
    }

    bool Hardware::boot(const uint64_t maxH8Cycles)
    {
        const auto start = m_h8Cycles;
        while (m_h8Cycles - start < maxH8Cycles)
        {
            advance();
            m_dsp.discardAudio();
            if (m_h8->bootReady() && m_dsp.bootFinished())
            {
                // The main loop becomes ready before the initial DAC/FX state
                // has settled. Advance both guests normally and discard only
                // this cold-start interval, before any host MIDI is accepted.
                for (size_t i = 0; i < 4096; ++i) renderSample();
                m_dsp.discardAudio();
                return true;
            }
        }
        return false;
    }

    DSP::AudioFrame Hardware::renderSample()
    {
        DSP::AudioFrame frame{};
        const auto start = m_h8Cycles;
        while (!m_dsp.popAudioFrame(frame))
        {
            advance();
            if (m_h8Cycles - start > H8::ClockHz)
                throw std::runtime_error("K-Station ESAI did not produce an audio frame.");
        }
        return frame;
    }
}
