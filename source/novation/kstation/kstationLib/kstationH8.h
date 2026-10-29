#pragma once

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
        enum class Stage1Mode { Original, Timers, TimersAndHdi, TimersHdiAndIrq, TimersHdiIrqAndSerialTx, Timer1AndRx };
        static constexpr uint32_t ClockHz = 16000000;
        static constexpr size_t FlashSize = 0x40000;
        struct HostCallbacks
        {
            std::function<uint8_t(uint32_t)> read;
            std::function<bool(uint32_t,uint8_t)> write;
            std::function<void(bool)> resetDSP; // P9DR bit 1, asserted low.
        };
        struct DebugState
        {
            uint32_t pc=0,sp=0;
            uint64_t cycles=0,instructions=0,hostReads=0,hostWrites=0,midiReceived=0;
            uint64_t interpretedSteps=0,nativeWaitSteps=0;
            uint64_t nativeIrqSteps=0,nativeIrqEntries=0,nativeIrqReturns=0;
            uint64_t nativeTimer3Steps=0,nativeSerialErrorSteps=0;
            uint64_t nativeSerialTxSteps=0,nativeSerialTxEntries=0,nativeSerialTxReturns=0;
            uint64_t nativeTimer1Steps=0,nativeTimer1Entries=0,nativeTimer1Returns=0;
            uint64_t nativeRxSteps=0,nativeRxEntries=0,nativeRxReturns=0,nativeRxClockRequests=0;
            uint64_t nativeClockSteps=0,nativeInternalClockCalls=0,nativeExternalClockCalls=0,nativeClockSyncUpdates=0;
            bool ramOverlay=false, mainEntered=false, initializedFromFirmware=false;
            uint8_t currentBank=0,currentProgram=0;
            std::string lcd;
        };

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
