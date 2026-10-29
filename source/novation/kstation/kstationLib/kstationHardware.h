#pragma once

#include "kstationDSP.h"
#include "kstationH8.h"
#include "baseLib/performanceDiagnostics.h"

namespace kstation
{
    // A single owner schedules both original processors. Used on the plugin's
    // worker and by offline validation; it never calls guest entry points.
    class Hardware
    {
    public:
        explicit Hardware(const std::vector<uint8_t>& rom, const std::vector<uint8_t>& flash = {},
                          H8::Stage1Mode mode=H8::Stage1Mode::Original);
        bool boot(uint64_t maxH8Cycles = H8::ClockHz * 10ull);
        DSP::AudioFrame renderSample();
        bool enqueueMidi(const uint8_t* bytes, size_t count) { return m_h8.enqueueMidi(bytes, count); }
        bool popMidiOut(uint8_t& byte) { return m_h8.popMidiOut(byte); }
        H8& controller() { return m_h8; }
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
        H8 m_h8;
    };
}
