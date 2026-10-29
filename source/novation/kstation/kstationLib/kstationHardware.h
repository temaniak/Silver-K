#pragma once

#include "kstationDSP.h"
#include "kstationController.h"
#include "baseLib/performanceDiagnostics.h"

namespace kstation
{
    // A single owner schedules the DSP and selected controller backend.
    // Used on the plugin worker and by offline functional validation.
    class Hardware
    {
    public:
        explicit Hardware(const std::vector<uint8_t>& rom, const std::vector<uint8_t>& flash = {},
                          H8::Stage1Mode mode=H8::Stage1Mode::Original);
        bool boot(uint64_t maxH8Cycles = H8::ClockHz * 10ull);
        DSP::AudioFrame renderSample();
        bool enqueueMidi(const uint8_t* bytes, size_t count) { return m_h8->enqueueMidi(bytes, count); }
        bool popMidiOut(uint8_t& byte) { return m_h8->popMidiOut(byte); }
        Controller& controller() { return *m_h8; }
        DSP& dsp() { return m_dsp; }
        void setPerformanceDiagnostics(baseLib::performance::Diagnostics* p){m_performance=p;}

    private:
        friend struct HardwareStage1Comparison;
        baseLib::performance::Diagnostics* m_performance=nullptr;
        uint64_t m_diagnosticAdvances=0;
        void advance();
        uint64_t m_h8Cycles = 0;
        int64_t m_dspEpoch = 0;
        bool m_dspStarted = false;
        bool m_resetAsserted = true;
        DSP m_dsp;
        std::unique_ptr<Controller> m_h8;
    };
}
