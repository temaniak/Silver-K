#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace kstation {
inline constexpr uint32_t ControllerClockHz=16000000;
inline constexpr size_t ControllerFlashSize=0x40000;
enum class ControllerMode { Original, Timers, TimersAndHdi, TimersHdiAndIrq, TimersHdiIrqAndSerialTx, Timer1AndRx, ControllerDevelopment, Native };
        struct ControllerHostCallbacks
        {
            std::function<uint8_t(uint32_t)> read;
            std::function<bool(uint32_t,uint8_t)> write;
            std::function<void(bool)> resetDSP; // P9DR bit 1, asserted low.
        };
        struct ControllerDebugState
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
            uint64_t nativeAdcSteps=0,nativeAdcEntries=0,nativeAdcReturns=0;
            uint64_t nativeDspTransferSteps=0,nativeForegroundSteps=0,nativeMainSteps=0,nativeMidiSteps=0,nativeTickSteps=0;
            uint64_t nativeStartupSteps=0,nativeFlashStateSteps=0,nativePatchUnpackSteps=0;
            uint64_t nativeControllerServiceSteps=0;
            uint64_t nativeMidiEventSteps=0,nativeVoiceAllocationSteps=0,nativeVoiceUpdateSteps=0;
            bool ramOverlay=false, mainEntered=false, initializedFromFirmware=false;
            uint8_t currentBank=0,currentProgram=0;
            std::string lcd;
        };

}
