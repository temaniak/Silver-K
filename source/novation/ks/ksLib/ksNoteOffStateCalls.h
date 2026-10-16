#pragma once
#include "ksNoteOffStateTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class NoteOffStateCalls final {
public:
 static bool candidate(unsigned p){return p>=0x2f92&&p<=0x3022;}
 bool step(ControllerCpu&);
 void resetMetrics(){started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0;
private:
 struct Call {NoteOffStateTask task;uint32_t token;unsigned depth=0;explicit Call(uint32_t t):token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using NoteOffStatePartialTest=bool(*)(ControllerCpu&);
void setNoteOffStatePartialForTests(NoteOffStatePartialTest);
}
