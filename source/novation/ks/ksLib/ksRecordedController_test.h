#pragma once
#include "ksControllerTrace.h"
#include <functional>
namespace ks {
// First C++ controller prototype: a checked, bounded recorded scenario, NOT a
// live MIDI controller. No H8 instructions run in advance(). It never invents
// responses or skips polling validation; exhaustion/mismatch stops the render.
class RecordedController final {
public:
    using Read=std::function<uint8_t(unsigned,unsigned)>;
    using Write=std::function<bool(unsigned,unsigned,uint8_t)>;
    RecordedController(std::shared_ptr<const ControllerTape>,uint64_t checkpoint,uint64_t cycle);
    uint64_t advance(unsigned budget,const Read&,const Write&);
    uint64_t cycles()const{return clock;}
    uint64_t checkedPorts()const{return checked;}
    size_t consumedSlices()const{return cursor;}
private:
    std::shared_ptr<const ControllerTape> tape;
    size_t cursor=0;
    uint64_t clock=0,checked=0;
    bool failed=false;
};
}
