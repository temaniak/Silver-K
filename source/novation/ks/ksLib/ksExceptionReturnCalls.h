#pragma once
#include "ksExceptionReturnTask.h"
#include <cstdint>
namespace ks{class ControllerCpu;class ExceptionReturnCalls final{public:static bool candidate(unsigned p){return p==0x170;}bool step(ControllerCpu&);unsigned activeCalls()const{return 0;}void resetMetrics(){initialActive=0;started=completed=partialEntries=0;}void discardCalls(){}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;};}
