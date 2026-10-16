#pragma once
#include <cstdint>

namespace ks {
class ControllerCpu;
// Retired batch ABI for comparison tests. Normal exchange uses native tasks.
using DspTransportPartialBatchTest=unsigned(*)(ControllerCpu&,uint64_t);
void setDspTransportPartialBatchForTests(DspTransportPartialBatchTest);
class DspTransport final {
public:
    static bool candidate(unsigned pc);
    // Returns the number of equivalent reference boundaries, or zero without
    // changing anything when the deadline/frame requires the compatibility path.
    static unsigned service(ControllerCpu&, uint64_t deadline);
};
}
