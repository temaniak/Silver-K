#include "ksRuntimeController.h"
#include "ksControllerState.h"
#include <stdexcept>

namespace ks {
bool RuntimeController::serviceInterrupt(ControllerCpu& h) {
    // This edge is confined to the authenticated native runtime. Boundary
    // comparison executables and arbitrary partial CPU frames retain their
    // existing explicit compatibility adapter.
    if (!h.nativeFetchClocksOnly || !h.nativeLowBase || !h.interruptMode2) return false;
    int selected=-1,priority=h.exr&7;
    for (unsigned base=0;base<=64;base+=64) {
        auto pending=base?h.pending_irqs_high:h.pending_irqs;
        while (pending) {
            unsigned bit=0; auto word=pending;
            while (!(word&1)) { ++bit; word>>=1; }
            if (h.interruptPriority[base+bit]>priority) {
                selected=int(base+bit); priority=h.interruptPriority[selected];
            }
            pending&=pending-1;
        }
    }
    if (selected>=0) {
        const auto vector=unsigned(selected)*4;
        if (vector+4>h.nativeLowSize) return false;
        uint32_t target=0;
        for (unsigned n=0;n<4;++n) target=(target<<8)|h.nativeLowBase[vector+n];
        target&=0xffffff;
        using K=InterruptService::Kind;
        K kind;
        if (selected==32 && target==0x12b0) kind=K::Tpu0;
        else if (selected==88 && target==0x105e) kind=K::SerialError;
        else if (selected==90 && target==0x114e) kind=K::SerialTransmit;
        else if ((selected==42 || selected==91) && target==0x170) kind=K::Unused;
        else return false; // Other lifecycles still use their semantic Calls bridge.
        // A malformed ring can alias the old CPU scratch stack. Keep that
        // diagnostic case in its explicit compatibility bridge; a valid UART
        // consumer is published modulo 256 by its sole interrupt owner.
        if(kind==K::SerialTransmit && h.memory[0xffed6e])return false;
        if(kind==K::Tpu0 && h.memory[0xffed6c])return false;
        // The remaining compatibility stack must be in ordinary internal RAM
        // for its old entry/return clock credits to apply. No bytes are pushed.
        if ((h.getSP()&1) || h.getSP()<0xfff400 || h.getSP()>0xfffc00) return false;
        if (interruptCount==interrupts.size()) throw std::logic_error("Native interrupt capacity exceeded");
        auto& slot=interrupts[interruptCount];
        slot.emplace(kind,h.exr,uint8_t(priority),h.indent+1); ++interruptCount;
        if (selected<64) h.pending_irqs&=~(1ull<<selected);
        else h.pending_irqs_high&=~(1ull<<(selected-64));
        h.exr=uint8_t(priority); ++h.indent;
        // Original mode-2 admission: 12+6 save clocks, 4 internal clocks,
        // six vector-read clocks. The native continuation stays in C++.
        h.cycles+=28; h.lastwrite=-1;
        ++interruptEntries; ++nativeInterruptStarts; ++nativeInterruptKinds[unsigned(kind)];
    }
    if (!interruptCount || interrupts[interruptCount-1]->depth!=h.indent) return false;
    auto& frame=*interrupts[interruptCount-1];
    auto& service=frame.service;
    const auto cost=service.nextClocks(); const auto start=h.cycles;
    const auto history=[](uint32_t address) {
        return address<0x400000 || (address>=0xfffd10 && address<0xffffa0) ? int(address) : -1;
    };
    const auto used=service.advanceBefore(cost,[&](uint32_t address,unsigned offset) {
        h.cycles=start+offset; h.lastread=history(address);
        return h.readResource(address);
    },[&](uint32_t address,uint8_t value,unsigned offset) {
        h.cycles=start+offset; h.lastwrite=history(address);
        h.writeResource(address,value);
    });
    if (used!=cost || !cost) throw std::logic_error("Native interrupt admission differs");
    h.cycles=start+cost; ++nativeInterruptPhases;
    h.exr=service.critical()?7:frame.priority;
    nativeTpuComponents|=service.timerComponents();
    if (service.complete()) {
        h.exr=frame.previousPriority; --h.indent;
        // Only bus history crosses back into the migration ABI. No register,
        // CCR, SP or PC was changed by this closed interrupt lifecycle.
        h.lastread=-1;
        interrupts[--interruptCount].reset(); ++nativeInterruptReturns;
    }
    return true;
}
}
