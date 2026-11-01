#pragma once
#include "ksMidiPartInitTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class MidiPartInitCalls final {
public:
 static bool candidate(unsigned p){return p>=0x31dc&&p<=0x328c;}
 bool step(ControllerCpu&);unsigned activeParts()const{unsigned n=0;for(const auto&c:parts)if(c)++n;return n;}unsigned activeControls()const{unsigned n=0;for(const auto&c:controls)if(c)++n;return n;}void resetMetrics(){initialParts=activeParts();initialControls=activeControls();partStarted=partCompleted=controlStarted=controlCompleted=partialEntries=0;}void discardCalls(){for(auto& c:parts)c.reset();for(auto& c:controls)c.reset();}
 uint64_t initialParts=0,initialControls=0,partStarted=0,partCompleted=0,controlStarted=0,controlCompleted=0,partialEntries=0;
private:
 template<class Task>struct Call {Task task;uint32_t token;explicit Call(uint32_t t):token(t){}};
 std::array<std::optional<Call<MidiPartInitTask>>,16> parts{};std::array<std::optional<Call<MidiControlInitTask>>,16> controls{};
};
using MidiPartInitPartialTest=bool(*)(ControllerCpu&);void setMidiPartInitPartialForTests(MidiPartInitPartialTest);
}
