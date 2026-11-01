#pragma once
#include "ksInternalMidiQueueTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class InternalMidiQueueCalls final {
public:
    static bool candidate(unsigned pc){return pc>=0x2c30&&pc<0x2ccc;}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{InternalMidiQueueTask task;uint32_t token;Call(InternalMidiQueueTask::Kind kind,uint32_t value,uint8_t byte):task(kind,byte),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using InternalMidiQueuePartialTest=bool(*)(ControllerCpu&);
void setInternalMidiQueuePartialForTests(InternalMidiQueuePartialTest);
}
