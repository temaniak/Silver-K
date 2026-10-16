#pragma once
#include <cstdint>

namespace ks {
class ControllerCpu;
// First event-controller stage: live HDI protocol jobs, not recorded traffic.
// The compatibility frame is materialized at job boundaries for IRQs, slices
// and switching back to H8. No job may cross the caller's next event deadline.
class DspTransport final {
public:
    static bool candidate(unsigned pc);
    // Returns the number of equivalent reference boundaries, or zero without
    // changing anything when the deadline/frame requires the compatibility path.
    static unsigned service(ControllerCpu&, uint64_t deadline);
};
}
