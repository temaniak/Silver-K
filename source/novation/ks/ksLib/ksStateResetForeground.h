#pragma once
#include "ksStateResetService.h"
#include <array>
#include <optional>

namespace ks {
class ControllerCpu;
// Narrow entry/return edge to the remaining compatibility foreground. The
// reset graph itself owns no register file or internal machine call frames.
class StateResetForeground final {
public:
    bool step(ControllerCpu&);
    unsigned active()const{return job_?1u:0u;}
    bool at(StateResetService::Component component)const{return job_&&job_->service.component()==component;}
    void resetMetrics(){initialActive=active();started=completed=phases=0;components=0;kinds.fill(0);}
    uint64_t started=0,completed=0,phases=0,initialActive=0;
    uint32_t components=0;
    std::array<uint64_t,4> kinds{};
private:
    struct Job {
        StateResetService service;
        uint32_t caller;
        int depth;
        uint8_t flags;
        bool childStart;
        Job(StateResetService::Kind kind,uint32_t stack,int level,uint8_t resultFlags)
            :service(kind),caller(stack),depth(level),flags(resultFlags),childStart(kind!=StateResetService::Kind::State){}
    };
    std::optional<Job> job_;
};
}
