#include "ksControllerJobs.h"
namespace ks {
namespace {ControlJobsPartialTest partialBatch=nullptr;}
void setControlJobsPartialForTests(ControlJobsPartialTest fn){partialBatch=fn;}
// All part lookup now runs through the semantic tasks. The old batching ABI
// is reachable only when a comparison executable installs the historical hook.
ControllerJobs::Result ControllerJobs::service(ControllerCpu& state,uint64_t deadline){return partialBatch?partialBatch(state,deadline):Result{};}
}
