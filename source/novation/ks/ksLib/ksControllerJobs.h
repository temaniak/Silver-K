#pragma once
#include <cstdint>

namespace ks {
class ControllerCpu;
// First live control jobs outside HDI. These preserve the compatibility frame
// at every yield; the full semantic controller migration is still incomplete.
class ControllerJobs final {
public:
    struct Result {unsigned steps=0,noteSteps=0;};
    static bool candidate(unsigned pc){
        return (pc>=0x4000 && pc<=0x4014)||(pc>=0xa66a && pc<=0xa67c);
    }
    static Result service(ControllerCpu&,uint64_t deadline);
};
using ControlShiftPartialTest=ControllerJobs::Result(*)(ControllerCpu&,uint64_t);
void setControlShiftPartialForTests(ControlShiftPartialTest);
}
