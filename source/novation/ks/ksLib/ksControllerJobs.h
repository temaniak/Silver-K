#pragma once
#include <cstdint>

namespace ks {
class ControllerCpu;
// Retired compatibility-batch API, closed by default in production.
class ControllerJobs final {
public:
    struct Result {unsigned steps=0,noteSteps=0;};
    static bool candidate(unsigned){return false;}
    static Result service(ControllerCpu&,uint64_t deadline);
};
using ControlJobsPartialTest=ControllerJobs::Result(*)(ControllerCpu&,uint64_t);
void setControlJobsPartialForTests(ControlJobsPartialTest);
}
