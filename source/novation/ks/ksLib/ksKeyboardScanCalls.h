#pragma once
#include "ksKeyboardScanTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class KeyboardScanCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x1b7e&&pc<0x1e16);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{KeyboardScanTask task;uint32_t token;Call(KeyboardScanTask::Kind kind,uint32_t token,uint32_t pageRef,uint32_t record,uint16_t current,uint16_t previous):task(kind,pageRef,record,current,previous),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using KeyboardScanPartialTest=bool(*)(ControllerCpu&);
void setKeyboardScanPartialForTests(KeyboardScanPartialTest);
}
