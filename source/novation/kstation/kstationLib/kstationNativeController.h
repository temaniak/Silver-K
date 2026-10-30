#pragma once
#include "kstationController.h"
namespace kstation {
// Independent controller backend. Only the optional original backend owns H8.
class NativeController final:public Controller {
public:
 NativeController(const std::vector<uint8_t>&image,ControllerHostCallbacks callbacks,const std::vector<uint8_t>&flash={});
 ~NativeController()override;
 void reset()override;uint64_t exec(uint32_t clocks)override;
 bool enqueueMidi(const uint8_t*,size_t)override;bool popMidiOut(uint8_t&)override;
 bool bootReady()const override;ControllerDebugState debugState()const override;
 uint8_t readMemory(uint32_t)const override;void copyCurrentPatch(uint8_t*)const override;
 std::vector<uint8_t>saveFlash()const override;void copyFlash(std::vector<uint8_t>&)const override;uint64_t flashRevision()const override;bool hasProgram(uint8_t,uint8_t)const override;
 const uint8_t*diagnosticMemory()override;uint16_t diagnosticRead16(uint32_t)override;void diagnosticWrite8(uint32_t,uint8_t)override;void diagnosticWrite16(uint32_t,uint16_t)override;void diagnosticInterrupt(int)override;
 bool isNative()const override{return true;}
private:struct Impl;std::unique_ptr<Impl>impl_;
};
}
