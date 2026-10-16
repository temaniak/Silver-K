#include "ksDspTransport.h"
namespace ks {
namespace {DspTransportPartialBatchTest partialBatch=nullptr;}
void setDspTransportPartialBatchForTests(DspTransportPartialBatchTest fn){partialBatch=fn;}
// Production exchange is owned by typed native tasks. This retired batch ABI
// exists only for comparison executables admitting fabricated partial frames.
bool DspTransport::candidate(unsigned){return false;}
unsigned DspTransport::service(ControllerCpu& h,uint64_t deadline){return partialBatch?partialBatch(h,deadline):0;}
}
