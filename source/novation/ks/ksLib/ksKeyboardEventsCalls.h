#pragma once
#include "ksKeyboardEventsTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class KeyboardEventsCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x1e16&&pc<0x1f84);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{KeyboardEventsTask task;uint32_t token;Call(KeyboardEventsTask::Kind kind,uint32_t token,uint32_t record,uint16_t input):task(kind,record,input),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using KeyboardEventsPartialTest=bool(*)(ControllerCpu&);
void setKeyboardEventsPartialForTests(KeyboardEventsPartialTest);
}
