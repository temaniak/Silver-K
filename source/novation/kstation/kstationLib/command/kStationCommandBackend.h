#pragma once

#include "kMusicalState.h"
#include "kProgramImage.h"
#include "../../../controller/command/commandSession.h"

namespace novation::controller::command {
// Owns ordered K program loads independently of the physical HDI adapter.
// The caller pumps one addressed transfer at a time and renders while waiting.
class KStationCommandBackend final {
public:
    static constexpr size_t ProgramBase=0x26400;
    static constexpr size_t ProgramBytes=KPackedProgramBytes;
    static constexpr size_t ProgramsPerBank=100;
    static constexpr size_t BankCount=4;
    static constexpr size_t AmpAttack=0x3e;
    static constexpr uint16_t ProgramMailbox=0x802b;
    enum class ProgramResult : uint8_t { Started, MissingCurrentBank, MissingDestination, Unsupported };
    enum class ReloadProgress : uint8_t { Pending, DspReady, Applied, Fault };
    struct PendingProgram {
        ProgramSelection selection;
        std::array<uint8_t, ProgramBytes> record;
        bool fromBank;
    };

    KStationCommandBackend(NativeFrame origin,const uint8_t* flash=nullptr,size_t bytes=0) noexcept
        : token_(session_.coldReset(Model::K,{0},origin,{48000},{112000000,2304},0)),
          model_(token_),flash_(flash),bytes_(bytes) {}
    const char* identity() const noexcept {return "command-native";}
    CommandSession& session() noexcept {return session_;}
    Session token() const noexcept {return token_;}
    KMusicalState& model() noexcept {return model_;}
    MidiAdmission pushMidiByte(NativeFrame due,uint8_t value) noexcept {return session_.pushMidiByte(token_,due,value);}

    bool selectBank(uint8_t value) noexcept {
        if(!validBank(value) || pending_)return false;
        bankLatch_=value;
        return true;
    }
    std::optional<uint8_t> bankLatch() const noexcept {return bankLatch_;}
    bool programPending() const noexcept {return pending_.has_value();}
    std::optional<PendingProgram> pendingProgram() const noexcept {
        if(!pending_)return {};
        PendingProgram result{pending_->target,{},pending_->fromBank};
        std::copy_n(pending_->expression.patch.begin(),ProgramBytes,result.record.begin());
        return result;
    }

    ProgramResult beginProgram(uint8_t number,const KExpressionSource& source,const KVoiceTunings& tunings,
                               const KProgramTables& tables,NativeFrame at) noexcept {
        const auto part=model_.part();
        if(!part.program)return ProgramResult::Unsupported;
        if(!programRecord({part.program->bank,number}))return ProgramResult::MissingCurrentBank;
        const ProgramSelection target{bankLatch_.value_or(part.program->bank),number};
        const auto* next=programRecord(target);
        if(!next)return ProgramResult::MissingDestination;
        return beginProgramRecord(target,next,source,tunings,tables,at);
    }

    ProgramResult beginProgramRecord(ProgramSelection target,const uint8_t* next,const KExpressionSource& source,
                                     const KVoiceTunings& tunings,const KProgramTables& tables,NativeFrame at) noexcept {
        if(!next || !validBank(target.bank) || target.program>=ProgramsPerBank ||
           !std::all_of(next,next+ProgramBytes,[](uint8_t value){return value<128;}))return ProgramResult::Unsupported;
        if(pending_ || session_.snapshot().lifecycle!=Lifecycle::Running || !model_.canReplaceProgram(at) ||
           !tables.clock || session_.transactions().poisoned(Endpoint::A))return ProgramResult::Unsupported;

        const auto decoded=decodeKProgram(next,source);
        if(decoded.patch[0x49]>tunings.lfoRates.size() || decoded.patch[0x4c]>tunings.lfoRates.size())
            return ProgramResult::Unsupported;
        if(!model_.claimVoiceOperation(this))return ProgramResult::Unsupported;
        auto& job=pending_.emplace();
        job.target=target;
        job.fromBank=next==programRecord(target);
        job.expression=decoded;
        job.panelMode=decoded.patch[0xab];
        job.globals=compileKProgramGlobals(decoded,tunings,tables);
        job.countChanged=source.patch[0x59]!=decoded.patch[0x59];
        job.started=at;
        for(size_t slot=0;slot<KMusicalState::VoiceCount;++slot) {
            const auto voice=model_.voice(VoiceSlot(slot));
            if(voice && voice->feedbackAge)job.retiring[job.count++]=uint8_t(slot);
        }
        return ProgramResult::Started;
    }

    const uint8_t* programRecord(ProgramSelection selection) const noexcept {
        const auto offset=programOffset(selection);
        if(!flash_ || !offset || *offset+ProgramBytes>bytes_)return nullptr;
        const auto* first=flash_+*offset;
        return std::all_of(first,first+ProgramBytes,[](uint8_t v){return v<128;})?first:nullptr;
    }

    static std::optional<size_t> programOffset(ProgramSelection selection) noexcept {
        if(!validBank(selection.bank) || selection.program>=ProgramsPerBank)return std::nullopt;
        return ProgramBase+((size_t(selection.bank)-1)*ProgramsPerBank+selection.program)*ProgramBytes;
    }

    bool programTimedOut(NativeFrame at) const noexcept {
        return pending_ && at.ticks>=pending_->started.ticks && at.ticks-pending_->started.ticks>=48000;
    }

    void abortProgram() noexcept {
        if(!pending_)return;
        session_.stop(token_);
        model_.releaseVoiceOperation(this);
        pending_.reset();
    }

    std::optional<DspTransactionDraft> nextProgramTransfer(NativeFrame at) const noexcept {
        if(!pending_ || pending_->ticket || at.ticks<pending_->nextQuery.ticks)return {};
        const auto& job=*pending_;
        switch(job.phase) {
        case Phase::Fade: return write(at,ProgramMailbox,&One);
        case Phase::PanelMode: return write(at,0x802c,&job.panelMode);
        case Phase::ReleaseLevel: return write(at,uint16_t(0x55+81*job.retiring[job.voice]),&ReleaseLevel);
        case Phase::ReleaseGate: return write(at,KFeedbackProtocol::gateAddress(VoiceSlot(job.retiring[job.voice])),&Zero);
        case Phase::WaitFade:
        case Phase::Fence: return KFeedbackProtocol::mailboxRead(at);
        case Phase::Acknowledge: return write(at,ProgramMailbox,&Zero);
        case Phase::Effects: return write(at,KProgramGlobals::FirstFxAddress,job.globals.fx.data(),job.globals.fx.size());
        case Phase::Route: return write(at,0x8039,&job.globals.routing[0]);
        case Phase::RouteDepth: return write(at,0x803b,&job.globals.routing[1]);
        case Phase::Pan: return write(at,0x0005,job.globals.pan.data(),job.globals.pan.size());
        case Phase::VoiceCount: return write(at,0x0002,&job.globals.voiceCount);
        case Phase::Unison: return write(at,0x8034,&job.globals.unison);
        case Phase::Split: return write(at,0x8030,&job.globals.split);
        case Phase::SplitAux: return write(at,0x8031,&job.globals.splitAux);
        case Phase::Level: return write(at,0x8033,&job.globals.level);
        }
        return {};
    }

    DspAdmission submitProgramTransfer(uint64_t operation,NativeFrame at,DspTicket& ticket) noexcept {
        const auto draft=nextProgramTransfer(at);
        if(!draft)return DspAdmission::InvalidDraft;
        const auto result=session_.transactions().submit(token_,operation,&*draft,1,&ticket,[]()noexcept{});
        if(result==DspAdmission::Accepted)pending_->ticket=ticket;
        return result;
    }

    ReloadProgress finishProgramTransfer(const DspTicket& ticket,NativeFrame at) noexcept {
        if(!pending_ || !verifiedTransfer(ticket,DspStatus::Transferred))return ReloadProgress::Fault;
        auto& job=*pending_;
        const auto phase=job.phase;
        if(phase==Phase::WaitFade || phase==Phase::Fence || !session_.transactions().retire(ticket))return ReloadProgress::Fault;
        job.ticket.reset();
        switch(phase) {
        case Phase::PanelMode: job.phase=job.count?Phase::ReleaseLevel:Phase::WaitFade; break;
        case Phase::ReleaseGate: job.phase=++job.voice<job.count?Phase::ReleaseLevel:Phase::WaitFade; break;
        case Phase::Pan: job.phase=job.countChanged?Phase::VoiceCount:Phase::Unison; break;
        default: job.phase=Phase(unsigned(phase)+1); break;
        }
        job.nextQuery=at;
        return ReloadProgress::Pending;
    }

    ReloadProgress acceptProgramReply(const DspTicket& ticket,const DspReply& reply,NativeFrame at,
                                      KExpressionSource& source) noexcept {
        if(!pending_ || !verifiedTransfer(ticket,DspStatus::Observed) || reply.endpoint!=Endpoint::A ||
           reply.transaction!=ticket.sequence)return ReloadProgress::Fault;
        auto& job=*pending_;
        if(job.phase!=Phase::WaitFade && job.phase!=Phase::Fence)return ReloadProgress::Fault;
        const auto value=KFeedbackProtocol::mailboxResult(reply.word);
        if(!value || !session_.transactions().retire(ticket))return ReloadProgress::Fault;
        job.ticket.reset();
        if(job.phase==Phase::WaitFade) {
            if(value->action==KMailboxAction::AcknowledgeAndRebuild) {
                job.phase=Phase::Acknowledge;
                return ReloadProgress::DspReady;
            }
            job.nextQuery={at.ticks+24};
            return ReloadProgress::Pending;
        }
        auto parameters=model_.part().parameters;
        parameters.cutoff=job.expression.patch[0x2e];
        parameters.modWheel=job.expression.controls[0];
        if(value->status!=0 || !model_.ownsVoiceOperation(this) ||
           !model_.replaceProgram(job.target,parameters,job.globals.voiceCount,at))
            return ReloadProgress::Fault;
        source=job.expression;
        bankLatch_.reset();
        pending_.reset();
        return model_.releaseVoiceOperation(this)?ReloadProgress::Applied:ReloadProgress::Fault;
    }

private:
    enum class Phase : uint8_t { Fade,PanelMode,ReleaseLevel,ReleaseGate,WaitFade,Acknowledge,Effects,
                                 Route,RouteDepth,Pan,VoiceCount,Unison,Split,SplitAux,Level,Fence };
    struct ProgramLoad {
        ProgramSelection target{};
        KExpressionSource expression{};
        KProgramGlobals globals{};
        std::array<uint8_t,KMusicalState::VoiceCount> retiring{};
        std::optional<DspTicket> ticket;
        size_t count=0,voice=0;
        NativeFrame started{},nextQuery{};
        Phase phase=Phase::Fade;
        uint32_t panelMode=0;
        bool countChanged=false,fromBank=false;
    };
    inline static constexpr uint32_t Zero=0,One=1,ReleaseLevel=0x100000;

    static DspTransactionDraft write(NativeFrame at,uint16_t address,const uint32_t* words,size_t count=1) noexcept {
        DspTransactionDraft draft;
        draft.earliest=at;
        draft.address={DspAddressMode::Write,address};
        draft.words=words;
        draft.wordCount=count;
        return draft;
    }
    bool verifiedTransfer(const DspTicket& ticket,DspStatus status) const noexcept {
        DspTransactionProgress progress;
        return pending_->ticket && ticket.sequence==pending_->ticket->sequence &&
            session_.transactions().progress(ticket,progress) && progress.status==status;
    }
    static constexpr bool validBank(uint8_t bank) noexcept {return bank>=1 && bank<=BankCount;}
    CommandSession session_;
    Session token_;
    KMusicalState model_;
    const uint8_t* flash_;
    size_t bytes_;
    std::optional<uint8_t> bankLatch_;
    std::optional<ProgramLoad> pending_;
};
}
