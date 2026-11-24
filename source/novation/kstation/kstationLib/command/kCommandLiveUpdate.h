#pragma once

#include "kCommandWriteState.h"
#include "kMidiParameters.h"
#include "kWriteProgress.h"
#include "../../../controller/command/commandSession.h"

namespace novation::controller::command {
enum class KLiveKind : uint8_t { Patch, Pressure, Breath, Pitch, Volume, Silence, Rebuild };
struct KLiveEdit {
    KLiveKind kind;
    uint16_t value=0;
    uint8_t offset=0; // used by Patch
};

// A fixed-capacity operation from desired controller state to addressed
// DSP changes. A correlated read publishes all changes behind their writes.
class KCommandLiveUpdate final {
public:
    ~KCommandLiveUpdate();
    KCommandLiveUpdate()=default;
    KCommandLiveUpdate(const KCommandLiveUpdate&)=delete;
    KCommandLiveUpdate& operator=(const KCommandLiveUpdate&)=delete;
    bool prepare(CommandSession&,KMusicalState&,const KExpressionSource&,const KVoiceTunings&,
                 const KProgramTables&,KLiveEdit,NativeFrame) noexcept;
    DspAdmission submit(uint64_t operation,NativeFrame at) noexcept;
    KWriteProgress transferred() noexcept;
    bool finish(std::optional<DspReply>,NativeFrame,KExpressionSource&,KVoiceTunings&) noexcept;
    std::optional<DspTicket> ticket() const noexcept {return ticket_;}
    size_t size() const noexcept {return count_;}
    bool resetsKeys() const noexcept {return resetKeys_;}
    bool reset() noexcept;

private:
    struct Write {uint16_t address;uint32_t word;};
    static constexpr size_t Capacity=8*(80+3)+64;
    bool add(uint16_t,uint32_t) noexcept;
    bool compile(const KExpressionSource&,const KVoiceTunings&,const KProgramTables&,KLiveEdit) noexcept;
    bool compileCutoffDeltas(const KExpressionSource&,const KVoiceTunings&) noexcept;
    bool compileVoiceDeltas(const KExpressionSource&,const KVoiceTunings&) noexcept;
    bool compileGlobals(const KExpressionSource&,const KVoiceTunings&,const KProgramTables&,bool all) noexcept;
    bool silence(uint8_t mask) noexcept;
    bool current() const noexcept;
    bool fail() noexcept;

    CommandSession* session_=nullptr;
    KMusicalState* model_=nullptr;
    kCommandDetail::WriteSource captured_{};
    KExpressionSource after_{};
    KVoiceTunings tuning_{};
    KParameters parameters_{};
    std::array<Write,Capacity> writes_{};
    std::optional<DspTicket> ticket_;
    uint64_t operationRevision_=0;
    size_t next_=0,count_=0,pendingWords_=0,limit_=0;
    uint8_t retired_=0;
    bool owned_=false,completed_=false,resetKeys_=false;
};
}
