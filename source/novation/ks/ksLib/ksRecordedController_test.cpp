#include "ksRecordedController_test.h"
#include <stdexcept>
namespace ks {
namespace {void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
RecordedController::RecordedController(std::shared_ptr<const ControllerTape> data,uint64_t checkpoint,uint64_t cycle):tape(std::move(data)),clock(cycle){
    require(bool(tape),"Missing KS controller trace");tape->validate();
    require(tape->checkpoint==checkpoint && tape->beginCycle==cycle,"KS controller starting state differs from trace");
}
uint64_t RecordedController::advance(unsigned budget,const Read& read,const Write& write){
    require(!failed,"Recorded KS controller already failed");
    try {
    require(budget==128,"Recorded KS controller requires original 128-cycle slice");
    require(cursor<tape->slices.size(),"Recorded KS controller trace exhausted");const auto& slice=tape->slices[cursor];
    for(size_t n=slice.first;n<size_t(slice.first)+slice.count;++n){const auto& event=tape->events[n];
        if(event.flags&1)require(write(event.dsp,event.offset,event.value)==bool(event.flags&2),"Recorded KS controller write acceptance differs");
        else require(read(event.dsp,event.offset)==event.value,"Recorded KS controller DSP response differs");
        ++checked;
    }
    clock=slice.end;++cursor;return clock;
    }catch(...){failed=true;throw;}
}
}
