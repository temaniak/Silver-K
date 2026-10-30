#pragma once
#include "kstationControllerTypes.h"
class h8state;
#include <stdexcept>
namespace kstation {
// Board-facing controller contract. The original oracle and CPU-independent
// service graph can be selected without constructing the other backend.
class Controller {
public:
 virtual ~Controller()=default;
 virtual void reset()=0;
 virtual uint64_t exec(uint32_t clocks)=0;
 virtual bool enqueueMidi(const uint8_t*bytes,size_t size)=0;
 virtual bool popMidiOut(uint8_t&byte)=0;
 virtual bool bootReady()const=0;
 bool ready()const{return bootReady();}
 virtual ControllerDebugState debugState()const=0;
 virtual uint8_t readMemory(uint32_t address)const=0;
 virtual void copyCurrentPatch(uint8_t*destination128)const=0;
 virtual std::vector<uint8_t> saveFlash()const=0;
 virtual void copyFlash(std::vector<uint8_t>&destination)const=0;
 virtual uint64_t flashRevision()const=0;
 virtual bool hasProgram(uint8_t bank,uint8_t program)const=0;
 static constexpr size_t DiagnosticMemorySize=1u<<24;
 virtual const uint8_t*diagnosticMemory()=0;
 virtual uint16_t diagnosticRead16(uint32_t address)=0;
 virtual void diagnosticWrite8(uint32_t address,uint8_t value)=0;
 virtual void diagnosticWrite16(uint32_t address,uint16_t value)=0;
 virtual void diagnosticInterrupt(int vector)=0;
 virtual bool isNative()const{return false;}
 virtual const char*faultMessage()const noexcept{return nullptr;}
 // Reference-only diagnostics. A native backend has no architectural core.
 virtual h8state&core(){throw std::logic_error("Native K controller has no H8 core");}
};
}
