#include "ksRuntimeController.h"
#include "ksControllerState.h"
#include <stdexcept>

namespace ks {
bool RuntimeController::serviceInterrupt(ControllerCpu& h) {
    // This edge is confined to the authenticated native runtime. Boundary
    // comparison executables and arbitrary partial CPU frames retain their
    // existing explicit compatibility adapter.
    if (!h.nativeFetchClocksOnly || !h.nativeLowBase || !h.interruptMode2) return false;
    const auto selection=InterruptScheduler::select(h.pending_irqs,h.pending_irqs_high,h.interruptPriority,h.exr);
    const int selected=selection.vector;const auto priority=selection.priority;
    if (selected>=0) {
        const auto vector=unsigned(selected)*4;
        if (vector+4>h.nativeLowSize) return false;
        uint32_t target=0;
        for (unsigned n=0;n<4;++n) target=(target<<8)|h.nativeLowBase[vector+n];
        target&=0xffffff;
        using K=InterruptService::Kind;
        K kind;
        if (selected==28 && target==0x165c) kind=K::Adc;
        else if (selected==89 && target==0x1072) kind=K::SerialReceive;
        else if (selected==32 && target==0x12b0) kind=K::Tpu0;
        else if (selected==88 && target==0x105e) kind=K::SerialError;
        else if (selected==90 && target==0x114e) kind=K::SerialTransmit;
        else if ((selected==42 || selected==91) && target==0x170) kind=K::Unused;
        else return false; // Other lifecycles still use their semantic Calls bridge.
        // A malformed ring can alias the old CPU scratch stack. Keep that
        // diagnostic case in its explicit compatibility bridge; a valid UART
        // consumer is published modulo 256 by its sole interrupt owner.
        if(kind==K::SerialTransmit && h.memory[0xffed6e])return false;
        if(kind==K::Tpu0 && h.memory[0xffed6c])return false;
        if(kind==K::SerialReceive && h.memory[0xffec64])return false;
        // The remaining compatibility stack must be in ordinary internal RAM
        // for its old entry/return clock credits to apply. No bytes are pushed.
        if ((h.getSP()&1) || h.getSP()<0xfff400 || h.getSP()>0xfffc00) return false;
        const auto level=interruptScheduler.depth();
        if(!interruptScheduler.admit(kind,priority,h.exr))throw std::logic_error("Native interrupt capacity/priority exceeded");
        compatibilityIrqDepth[level]=h.indent+1;
        if (selected<64) h.pending_irqs&=~(1ull<<selected);
        else h.pending_irqs_high&=~(1ull<<(selected-64));
        h.exr=uint8_t(priority); ++h.indent;
        // Original mode-2 admission: 12+6 save clocks, 4 internal clocks,
        // six vector-read clocks. The native continuation stays in C++.
        h.cycles+=InterruptScheduler::AdmissionClocks; h.lastwrite=-1;
        ++interruptEntries; ++nativeInterruptStarts; ++nativeInterruptKinds[unsigned(kind)];
    }
    if(!interruptScheduler.active()||compatibilityIrqDepth[interruptScheduler.depth()-1]!=h.indent)return false;
    const auto cost=interruptScheduler.nextClocks();const auto start=h.cycles;
    const auto history=[](uint32_t address) {
        return address<0x400000 || (address>=0xfffd10 && address<0xffffa0) ? int(address) : -1;
    };
    const auto result=interruptScheduler.advanceBefore(cost,[&](uint32_t address,unsigned offset) {
        h.cycles=start+offset; h.lastread=history(address);
        return h.readResource(address);
    },[&](uint32_t address,uint8_t value,unsigned offset) {
        h.cycles=start+offset; h.lastwrite=history(address);
        h.writeResource(address,value);
    });
    if (result.clocks!=cost || !cost) throw std::logic_error("Native interrupt admission differs");
    h.cycles=start+cost; ++nativeInterruptPhases;
    h.exr=result.mask;
    nativeTpuComponents|=result.timer;
    nativeAdcComponents|=result.adc;
    nativeSerialReceiveComponents|=result.receive;
    if (result.returned) {
        --h.indent;
        // Only bus history crosses back into the migration ABI. No register,
        // CCR, SP or PC was changed by this closed interrupt lifecycle.
        h.lastread=-1;
        ++nativeInterruptReturns;
    }
    return true;
}
}
