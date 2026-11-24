#pragma once
#include "kCommandWriteState.h"
#include "kWriteProgress.h"
#include "../../../controller/command/commandSession.h"

namespace novation::controller::command {
// NoteOff and sustain pedal for single-part K. A release batch is reserved
// before publication. A deferred gate belongs to the current voice allocation.
// Runtime owns physical/held key changes separately from this voice command.
class KCommandNoteOff final {
public:
    KCommandNoteOff()=default;
    ~KCommandNoteOff();
    KCommandNoteOff(const KCommandNoteOff&)=delete;
    KCommandNoteOff& operator=(const KCommandNoteOff&)=delete;
    bool prepare(CommandSession&,KMusicalState&,uint8_t note,bool sustainBypass=false,uint8_t excludedSlots=0)noexcept;
    bool preparePedal(CommandSession&,KMusicalState&,uint8_t value,bool sustainBypass=false)noexcept;
    bool preparePending(CommandSession&,KMusicalState&)noexcept;
    bool prepareAll(CommandSession&,KMusicalState&)noexcept;
    bool prepareOwned(CommandSession&,KMusicalState&,const VoiceAllocation*,size_t)noexcept;
    DspAdmission admit(uint64_t operation,NativeFrame at)noexcept;
    std::optional<DspTicket> ticket()const noexcept {
        return accepted_ && !complete_ && next_<gateCount_?std::optional{tickets_[next_]}:std::nullopt;
    }
    KWriteProgress progress()noexcept;
    bool reset()noexcept;
    KWriteProgress abort()noexcept{return fail();}
    size_t size()const noexcept{return gateCount_;}
    size_t next()const noexcept{return next_;}
    std::optional<VoiceAllocation> allocation(size_t index)const noexcept {
        return index<gateCount_?std::optional{gates_[index]}:std::nullopt;
    }
private:
    bool prepareFor(CommandSession&,KMusicalState&,std::optional<uint8_t> note,
                    std::optional<bool> pedal,bool sustainBypass,bool all=false,
                    const VoiceAllocation* owned=nullptr,size_t ownedCount=0,uint8_t excludedSlots=0)noexcept;
    void selectGates()noexcept;
    bool live()const noexcept;
    bool current()const noexcept;
    DspAdmission reject(DspAdmission)noexcept;
    KWriteProgress fail()noexcept;
    void finish()noexcept;
    CommandSession* session_=nullptr;KMusicalState* model_=nullptr;kCommandDetail::WriteSource source_{};
    std::array<VoiceAllocation,KMusicalState::VoiceCount> allocations_{};
    std::array<VoiceAllocation,KMusicalState::VoiceCount> gates_{};
    std::array<DspTicket,KMusicalState::VoiceCount> tickets_{};
    uint64_t operationRevision_=0;size_t count_=0,gateCount_=0,next_=0;
    std::optional<bool> pedal_;
    bool defer_=false;
    bool accepted_=false,complete_=false,fault_=false;
};
}
