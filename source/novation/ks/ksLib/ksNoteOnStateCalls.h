#pragma once
#include "ksNoteOnStateTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class NoteOnStateCalls final {
public:
 static bool candidate(unsigned p){return p>=0x2efc&&p<=0x2f90;}
 bool step(ControllerCpu&);void resetMetrics(){started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0;
private:
 struct Call {NoteOnStateTask task;uint32_t token;unsigned depth=0;explicit Call(uint32_t t):token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using NoteOnStatePartialTest=bool(*)(ControllerCpu&);
void setNoteOnStatePartialForTests(NoteOnStatePartialTest);
}
