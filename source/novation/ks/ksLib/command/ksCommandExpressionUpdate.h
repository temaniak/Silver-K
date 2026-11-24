#pragma once

#include "ksExpressionPacket.h"

namespace novation::controller::command {
enum class KSExpressionUpdateProgress { Ready, WaitingForTransfer, Complete, Fault };

// One complete expression event owns its immutable partial packet across real
// endpoint readiness and transfer continuations. Expression source edits are
// serialized by the common voice-operation owner until this packet completes.
class KSCommandExpressionUpdate final {
public:
    KSCommandExpressionUpdate()=default;
    ~KSCommandExpressionUpdate(){if(accepted_ && !complete_ && !fault_)fail();}
    KSCommandExpressionUpdate(const KSCommandExpressionUpdate&)=delete;
    KSCommandExpressionUpdate& operator=(const KSCommandExpressionUpdate&)=delete;

    bool prepare(CommandSession& session,KSMusicalState& model,KSCommandExpression& expression,KSStorageSlot part,
                 uint64_t expressionRevision,KSRefreshPacket packet)noexcept {
        if(model_ || session.snapshot().session!=model.session() || session.snapshot().lifecycle!=Lifecycle::Running ||
           !model.voiceOperationAvailable())return false;
        const auto dependency=model.dependency(part,session.rateGeneration());const auto* source=expression.part(part);
        if(!dependency || !source || source->revision!=expressionRevision || expression.session()!=model.session())return false;
        session_=&session;model_=&model;expression_=&expression;part_=part;dependency_=*dependency;
        expressionRevision_=expressionRevision;operationRevision_=model.voiceOperationRevision();packet_=std::move(packet);
        for(size_t i=0;i<generations_.size();++i){generations_[i]=*model.voiceGeneration(VoiceSlot(i));before_[i]=model.voice(VoiceSlot(i));}
        for(size_t i=0;i<packet_.size();++i)endpoints_|=uint8_t(1u<<unsigned(packet_.at(i)->endpoint));
        return true;
    }
    DspAdmission admit(uint64_t operation,NativeFrame at)noexcept {
        if(!live())return rejected(DspAdmission::StaleSession);
        if(complete_ || ticket_)return DspAdmission::StaleOperation;
        if(!current())return rejected(DspAdmission::InvalidDraft);
        if(poisoned())return rejected(DspAdmission::Poisoned);
        if(!packet_.size()) {
            if(!model_->claimVoiceOperation(this))std::terminate();accepted_=true;finish();return DspAdmission::Accepted;
        }
        const auto write=packet_.at(next_);if(!write)return rejected(DspAdmission::InvalidDraft);
        DspTransactionDraft draft;draft.model=Model::KS;draft.endpoint=write->endpoint;draft.earliest=at;
        draft.address={DspAddressMode::Write,write->address};draft.words=write->words;draft.wordCount=write->count;
        DspTicket ticket;
        const auto result=session_->transactions().submit(model_->session(),operation,&draft,1,&ticket,[&]()noexcept{
            if(!accepted_){if(!model_->claimVoiceOperation(this))std::terminate();accepted_=true;}
        });
        if(result==DspAdmission::Accepted)ticket_=ticket;
        else if(result!=DspAdmission::Full)return rejected(result);
        return result;
    }
    KSExpressionUpdateProgress progress()noexcept {
        if(complete_ && live())return KSExpressionUpdateProgress::Complete;
        if(!live() || !current() || poisoned())return fail();
        if(!ticket_)return KSExpressionUpdateProgress::Ready;
        DspTransactionProgress progress;
        if(!session_->transactions().progress(*ticket_,progress))return fail();
        if(progress.status==DspStatus::Queued)return KSExpressionUpdateProgress::WaitingForTransfer;
        if(progress.status!=DspStatus::Transferred || !session_->transactions().retire(*ticket_))return fail();
        ticket_.reset();
        if(++next_==packet_.size()){finish();return KSExpressionUpdateProgress::Complete;}
        return KSExpressionUpdateProgress::Ready;
    }
    bool reset()noexcept {
        if(accepted_ && !complete_ && !fault_)return false;
        session_=nullptr;model_=nullptr;expression_=nullptr;ticket_.reset();next_=0;endpoints_=0;
        accepted_=complete_=fault_=acknowledged_=false;return true;
    }
    KSExpressionUpdateProgress abort()noexcept{return fail();}
    std::optional<DspTicket> ticket()const noexcept{return ticket_;}
    size_t size()const noexcept{return packet_.size();}
    size_t next()const noexcept{return next_;}
    bool acknowledged()const noexcept{return acknowledged_;}
private:
    bool live()const noexcept {
        return model_ && !fault_ && session_->snapshot().session==model_->session() &&
            session_->snapshot().lifecycle==Lifecycle::Running && session_->rateGeneration()==dependency_.rateGeneration;
    }
    bool poisoned()const noexcept {
        for(const auto endpoint:{Endpoint::A,Endpoint::B})if((endpoints_&(1u<<unsigned(endpoint))) && session_->transactions().poisoned(endpoint))return true;
        return false;
    }
    bool current()const noexcept {
        const auto dependency=model_->dependency(part_,session_->rateGeneration());
        if(!dependency || dependency->session!=dependency_.session || dependency->part!=dependency_.part ||
           dependency->revisions.program!=dependency_.revisions.program || dependency->routingRevision!=dependency_.routingRevision ||
           model_->voiceOperationRevision()!=operationRevision_+unsigned(accepted_) ||
           (accepted_?!model_->ownsVoiceOperation(this):!model_->voiceOperationAvailable()))return false;
        const auto* source=expression_->part(part_);
        if(!source || source->revision!=expressionRevision_ || !(*dependency==dependency_))return false;
        for(size_t i=0;i<before_.size();++i) {
            const auto now=model_->voice(VoiceSlot(i));const auto& before=before_[i];
            if(bool(now)!=bool(before) || (now && (!(now->allocation==before->allocation) ||
               (!accepted_ && now->observationRevision!=before->observationRevision))))return false;
        }
        for(size_t i=0;i<generations_.size();++i)if(*model_->voiceGeneration(VoiceSlot(i))!=generations_[i])return false;
        return true;
    }
    void finish()noexcept {
        acknowledged_=bool(expression_->takeDirty(part_,expressionRevision_));
        if(!model_->releaseVoiceOperation(this))std::terminate();complete_=true;
    }
    DspAdmission rejected(DspAdmission result)noexcept{if(accepted_)fail();return result;}
    KSExpressionUpdateProgress fail()noexcept {
        if(model_ && accepted_ && session_->snapshot().session==model_->session())session_->stop(model_->session());
        if(model_ && model_->ownsVoiceOperation(this))model_->releaseVoiceOperation(this);
        fault_=true;return KSExpressionUpdateProgress::Fault;
    }
    CommandSession* session_=nullptr;KSMusicalState* model_=nullptr;KSCommandExpression* expression_=nullptr;
    KSStorageSlot part_=KSStorageSlot::Single;ParameterDependency<KSStorageSlot> dependency_{};
    KSRefreshPacket packet_{};std::optional<DspTicket> ticket_;
    std::array<uint64_t,KSMusicalState::VoiceCount> generations_{};
    KSMusicalState::Voices::Projection before_{};
    uint64_t expressionRevision_=0,operationRevision_=0;
    size_t next_=0;uint8_t endpoints_=0;
    bool accepted_=false,complete_=false,fault_=false,acknowledged_=false;
};
}
