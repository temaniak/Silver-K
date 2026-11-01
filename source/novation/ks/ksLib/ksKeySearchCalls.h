#pragma once
#include "ksKeyboard.h"
#include <array>
#include <optional>
#include <cstdint>
namespace ks {
class ControllerCpu;
// Temporary caller projection around a processor-independent native task.
// Only the bridge knows the old caller ABI. Delete it with those callers;
// KeySearchTask itself requires no CPU registers, status, stack or address PC.
class KeySearchCalls final {
public:
    static bool candidate(unsigned p){return p>=0x317a && p<=0x31da;}
    bool step(ControllerCpu&);
    void resetMetrics(){started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0;
private:
    struct Call {
        nativeControl::KeySearchTask task;
        uint32_t storage,callerToken;
        Call(bool next,unsigned key,uint32_t base,uint32_t token)
            :task(next?nativeControl::KeySearchTask::Direction::Next:nativeControl::KeySearchTask::Direction::Previous,key),storage(base),callerToken(token){}
    };
    // Original mode-2 priority nesting is at most eight. Extra fixed capacity
    // also admits mode-0/reference transitions without allocating on the worker.
    std::array<std::optional<Call>,16> calls{};
};
using KeySearchPartialTest=bool(*)(ControllerCpu&);
// Registered only by the original-H8 test object. Normal plugin builds have
// no implementation and cannot resume an arbitrary fabricated partial frame.
void setKeySearchPartialForTests(KeySearchPartialTest);
}
