#pragma once
#include "ksDspStartupTasks.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
// Temporary compatibility projection for semantic startup and port selection.
class DspStartupCalls final {
public:
 static bool candidate(unsigned p){return (p>=0x136e && p<=0x13ac)||(p>=0x230c && p<=0x2330);}
 bool step(ControllerCpu&);
 void resetMetrics(){resetStarted=resetCompleted=startupStarted=startupCompleted=partialEntries=0;}
 void discardCalls(){for(auto& c:resets)c.reset();for(auto& c:startups)c.reset();}
 uint64_t resetStarted=0,resetCompleted=0,startupStarted=0,startupCompleted=0,partialEntries=0;
private:
 struct Reset {DspResetTask task;uint32_t token;explicit Reset(uint32_t v):token(v){}};
 struct Startup {DspStartupTask task;uint32_t token;explicit Startup(uint32_t v):token(v){}};
 std::array<std::optional<Reset>,16> resets{};
 std::array<std::optional<Startup>,16> startups{};
};
class DspSelectCalls final {
public:
 static bool candidate(unsigned p){return p>=0x248e && p<=0x24b0;}
 bool step(ControllerCpu&);
 void resetMetrics(){started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0;
private:
 struct Call {DspSelectTask task;uint32_t token;Call(DspPort port,uint32_t v):task(port),token(v){}};
 std::array<std::optional<Call>,16> calls{};
};
using DspStartupPartialTest=bool(*)(ControllerCpu&);
void setDspStartupPartialForTests(DspStartupPartialTest);
}
