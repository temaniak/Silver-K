#pragma once
#include "ksMidiOutputTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class MidiOutputCalls final {
public:
    static bool candidate(unsigned pc){return pc>=0x2a58&&pc<0x2c30;}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{MidiOutputTask task;uint32_t token;Call(MidiOutputTask::Kind kind,uint32_t value,uint16_t event,uint16_t note,uint8_t control):task(kind,event,note,control),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using MidiOutputPartialTest=bool(*)(ControllerCpu&);
void setMidiOutputPartialForTests(MidiOutputPartialTest);
}
