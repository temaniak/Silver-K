#pragma once
#include "kExpressionPacket.h"
#include "kCommandWriteState.h"
#include "kWriteProgress.h"
#include "../../../controller/command/commandSession.h"

namespace novation::controller::command {
// One complete CC1 event retains its copied partial packet across real queue
// and transfer continuations. Its desired source commits once on reservation.
class KCommandModWheelUpdate final {
public:
    KCommandModWheelUpdate()=default;
    ~KCommandModWheelUpdate();
    KCommandModWheelUpdate(const KCommandModWheelUpdate&)=delete;
    KCommandModWheelUpdate& operator=(const KCommandModWheelUpdate&)=delete;
    bool prepare(CommandSession&,KMusicalState&,const KExpressionPacket&,uint8_t newWheel)noexcept;
    DspAdmission admit(uint64_t operation,NativeFrame at)noexcept;
    std::optional<DspTicket> ticket()const noexcept{return ticket_;}
    KWriteProgress progress()noexcept;
    bool reset()noexcept;
    KWriteProgress abort()noexcept{return fail();}
    size_t size()const noexcept{return packet_.size();}
    size_t next()const noexcept{return next_;}
private:
    bool live()const noexcept;
    bool current()const noexcept;
    DspAdmission reject(DspAdmission)noexcept;
    KWriteProgress fail()noexcept;
    void finish()noexcept;
    CommandSession* session_=nullptr;KMusicalState* model_=nullptr;
    KExpressionPacket packet_{};kCommandDetail::WriteSource source_{};
    std::optional<DspTicket> ticket_;uint64_t operationRevision_=0;size_t next_=0;uint8_t wheel_=0;
    bool accepted_=false,complete_=false,fault_=false;
};
}
