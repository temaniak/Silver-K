#pragma once
#include "ksMidiDecodeTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class MidiDecodeCalls final {
public:
 static bool candidate(unsigned p){return p>=0x2de8&&p<0x2efc;}
 bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}unsigned activeDescriptors()const{unsigned n=0;for(const auto& c:descriptors)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();initialDescriptors=activeDescriptors();started=completed=handedOff=descriptorStarted=descriptorCompleted=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();for(auto& c:descriptors)c.reset();}
 uint64_t initialActive=0,initialDescriptors=0;uint64_t started=0,completed=0,handedOff=0,descriptorStarted=0,descriptorCompleted=0,partialEntries=0;
private:
 struct Call {MidiDecodeTask task;uint32_t token,handlerReference=0;explicit Call(uint32_t t):token(t){}};
 struct Descriptor {MidiDescriptorTask task;uint32_t token;Descriptor(uint8_t byte,uint32_t t):task(byte),token(t){}};
 std::array<std::optional<Call>,16> calls{};std::array<std::optional<Descriptor>,16> descriptors{};
};
using MidiDecodePartialTest=bool(*)(ControllerCpu&);void setMidiDecodePartialForTests(MidiDecodePartialTest);
}
