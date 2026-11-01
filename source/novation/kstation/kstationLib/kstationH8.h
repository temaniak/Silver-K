#pragma once
#include "kstationControllerTypes.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class h8state;

namespace kstation
{
    // Continuous original H8 firmware, including reset and the UART dispatcher.
    // The caller schedules DSP/H8 time; byte callbacks access actual HDI08.
    class H8
    {
    public:
        using Stage1Mode=ControllerMode;
        static constexpr uint32_t ClockHz = 16000000;
        static constexpr size_t FlashSize = 0x40000;
        using HostCallbacks=ControllerHostCallbacks;
        using DebugState=ControllerDebugState;

        H8(const std::vector<uint8_t>& decodedOS, HostCallbacks callbacks,
           const std::vector<uint8_t>& initialFlash={}, Stage1Mode mode=Stage1Mode::Original);
        ~H8();
        H8(const H8&)=delete;
        H8& operator=(const H8&)=delete;
        void reset();
        uint64_t exec(uint32_t cycleBudget);
        bool enqueueMidi(const uint8_t* bytes,size_t size);
        bool popMidiOut(uint8_t& byte);
        bool bootReady() const;
        bool ready() const {return bootReady();}
        DebugState debugState() const;
        uint8_t readMemory(uint32_t address) const;
        // Canonical packed representation from current guest decoded mirrors;
        // does not mutate the guest or invoke its serialization entry points.
        void copyCurrentPatch(uint8_t* destination128) const;
        std::vector<uint8_t> saveFlash() const;
        void copyFlash(std::vector<uint8_t>&) const;
        uint64_t flashRevision() const;
        bool hasProgram(uint8_t bank,uint8_t program) const;
        h8state& core(); // Diagnostics only; not a reset/entry-point bypass.
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
