#pragma once
#include "ksResetImageTask.h"
#include <cstdint>
#include <optional>
namespace ks {class ControllerCpu;class ResetImageCalls final {public:
 static bool candidate(unsigned p){return p==0x180||(p>=0x2ec&&p<0x456)||p==0xffffec00||p==0xffffec04||p==0xffffec06||p==0xffffec0a;}
 bool step(ControllerCpu&);void discardCalls(){task_.reset();}void resetMetrics(){initialActive=bool(task_);started=completed=partialEntries=0;}unsigned activeCalls()const{return bool(task_);}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:std::optional<ResetImageTask> task_;
};using ResetImagePartialTest=bool(*)(ControllerCpu&);void setResetImagePartialForTests(ResetImagePartialTest);}
