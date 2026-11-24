#pragma once

#include "ksAllocatedVoiceSource.h"
#include <exception>

namespace novation::controller::command {
// One immutable part voice-update intent. Payload construction samples every
// numerical input once. Only real addressed transfers continue the owner.
class KSCommandVoiceUpdate final {
public:
    using Contexts=std::array<KSParameterVoiceContext,KSMusicalState::VoiceCount>;
    KSCommandVoiceUpdate()=default;
    ~KSCommandVoiceUpdate(){if(accepted_ && !complete_ && !fault_)fail();}
    KSCommandVoiceUpdate(const KSCommandVoiceUpdate&)=delete;
    KSCommandVoiceUpdate& operator=(const KSCommandVoiceUpdate&)=delete;

    bool prepare(CommandSession& session,KSMusicalState& state,KSVoiceParameterBank& bank,KSStorageSlot part,
        const KSVoiceImageTables& tables,const Contexts& contexts,uint64_t contextRevision,
        std::optional<VoiceSlot> tracked=std::nullopt)noexcept {
        return prepareImpl(session,state,bank,part,tables,contexts,contextRevision,nullptr,nullptr,tracked);
    }
    // Allocation bindings select a live editable source or the immutable
    // record retained by that allocation, independently for every voice.
    bool prepare(CommandSession& session,KSMusicalState& state,KSVoiceParameterBank& bank,KSStorageSlot part,
        const KSVoiceImageTables& tables,const Contexts& contexts,uint64_t contextRevision,
        const KSDrumVoiceBindings& bindings,const KSDrumProgramStore& store,
        std::optional<VoiceSlot> tracked=std::nullopt)noexcept {
        return prepareImpl(session,state,bank,part,tables,contexts,contextRevision,&bindings,&store,tracked);
    }
private:
    bool prepareImpl(CommandSession& session,KSMusicalState& state,KSVoiceParameterBank& bank,KSStorageSlot part,
        const KSVoiceImageTables& tables,const Contexts& contexts,uint64_t contextRevision,
        const KSDrumVoiceBindings* bindings,const KSDrumProgramStore* store,std::optional<VoiceSlot> tracked)noexcept {
        if(state_ || session.snapshot().session!=state.session() || session.snapshot().lifecycle!=Lifecycle::Running ||
           !state.voiceOperationAvailable() || (tracked && size_t(*tracked)>=KSMusicalState::VoiceCount))return false;
        const auto dependency=state.dependency(part,session.rateGeneration());const auto model=state.part(part);
        const auto observed=dependency && model?bank.observe(*dependency,model->parameters):std::nullopt;
        if(!observed)return false;
        KSRefreshPacket packet;
        std::array<std::optional<KSGlideWords>,KSMusicalState::VoiceCount> glides{};
        KSAllocatedVoiceSources sources{};
        // Glide's previous-note register is shared by the complete ascending
        // voice batch. A record may update it even when the next record has
        // neutral detune and only reads that new value.
        std::optional<uint8_t> previousNote;
        for(size_t i=0;i<KSMusicalState::VoiceCount;++i) {
            const auto voice=state.voice(VoiceSlot(i));
            if(!voice || !voice->feedbackAge || voice->part!=part)continue;
            auto context=contexts[i];
            if(previousNote)context.voice.previousNote=*previousNote;
            std::optional<KSParameterImageResult> image;
            if(bindings) {
                sources[i]=observeKsAllocatedVoiceSource(state,bank,*bindings,*store,voice->allocation,tables.filter,session.rateGeneration());
                if(!sources[i])return false;
                image=encodeKsAllocatedInitialImage(*sources[i],tables,context);
            } else {
                const auto source=state.observeCutoff(part,voice->allocation,tables.filter,session.rateGeneration());
                image=source?encodeKsParameterImage(observed->patch,tables,context,{*source,encodeKsCutoff(source->inputs)}):std::nullopt;
            }
            if(!image || !packet.append(Endpoint::B,uint16_t(0x2c+101*i),image->image.image.data(),100))return false;
            glides[i]=image->glide;
            previousNote=image->glide.previousNote;
        }
        if(tracked) {
            if(bindings && !sources[size_t(*tracked)]) {
                const auto voice=state.voice(*tracked);
                if(!voice || voice->part!=part)return false;
                sources[size_t(*tracked)]=observeKsAllocatedVoiceSource(state,bank,*bindings,*store,
                    voice->allocation,tables.filter,session.rateGeneration());
                if(!sources[size_t(*tracked)])return false;
            }
            const auto& patch=bindings?sources[size_t(*tracked)]->patch:observed->patch;
            const auto words=encodeKsVoiceTracking(patch,tables);if(!words)return false;
            for(size_t i=0;i<words->size();++i)if(!packet.appendWord(Endpoint::B,
                uint16_t(KSVoiceImageResult::TrackingAddresses[i]+101*size_t(*tracked)),(*words)[i]))return false;
        }
        state_=&state;session_=&session;bank_=&bank;source_=*observed;packet_=packet;glides_=glides;
        bindings_=bindings;store_=store;allocatedSources_=sources;
        contextRevision_=contextRevision;ownerRevision_=state.voiceOperationRevision();
        for(size_t i=0;i<KSMusicalState::VoiceCount;++i) {
            before_[i]=state.voice(VoiceSlot(i));generations_[i]=*state.voiceGeneration(VoiceSlot(i));
            pending_[i]=state.deferredVoice(VoiceSlot(i));
        }
        return true;
    }
public:
    DspAdmission admit(uint64_t operation,NativeFrame at,uint64_t contextRevision)noexcept {
        if(!live())return reject(DspAdmission::StaleSession);
        if(complete_ || ticket_)return DspAdmission::StaleOperation;
        if(!current(contextRevision))return reject(DspAdmission::InvalidDraft);
        if(session_->transactions().poisoned(Endpoint::B))return reject(DspAdmission::Poisoned);
        if(packet_.size()==0) {
            if(!state_->claimVoiceOperation(this))std::terminate();accepted_=true;
            return finish(contextRevision)==KSRefreshProgress::Complete?DspAdmission::Accepted:DspAdmission::InvalidDraft;
        }
        const auto write=packet_.at(next_);if(!write)return reject(DspAdmission::InvalidDraft);
        DspTransactionDraft draft;draft.model=Model::KS;draft.endpoint=write->endpoint;draft.earliest=at;
        draft.address={DspAddressMode::Write,write->address};draft.words=write->words;draft.wordCount=write->count;
        DspTicket ticket;
        const auto result=session_->transactions().submit(state_->session(),operation,&draft,1,&ticket,[&]()noexcept{
            if(!accepted_){if(!state_->claimVoiceOperation(this))std::terminate();accepted_=true;}
            ticket_=ticket;
        });
        return result==DspAdmission::Accepted || result==DspAdmission::Full?result:reject(result);
    }
    KSRefreshProgress progress(uint64_t contextRevision)noexcept {
        if(complete_ && live())return KSRefreshProgress::Complete;
        if(!live() || !accepted_ || !current(contextRevision) || session_->transactions().poisoned(Endpoint::B))return fail();
        if(!ticket_)return KSRefreshProgress::Ready;
        DspTransactionProgress progress;
        if(!session_->transactions().progress(*ticket_,progress))return fail();
        if(progress.status==DspStatus::Queued)return KSRefreshProgress::WaitingForTransfer;
        if(progress.status!=DspStatus::Transferred || !session_->transactions().retire(*ticket_))return fail();
        ticket_.reset();if(++next_!=packet_.size())return KSRefreshProgress::Ready;return finish(contextRevision);
    }
    bool reset()noexcept {
        if(accepted_ && !complete_ && !fault_)return false;
        state_=nullptr;session_=nullptr;bank_=nullptr;ticket_.reset();packet_={};next_=0;
        bindings_=nullptr;store_=nullptr;allocatedSources_={};
        accepted_=complete_=fault_=false;return true;
    }
    KSRefreshProgress abort()noexcept{return fail();}
    std::optional<DspTicket> ticket()const noexcept{return ticket_;}
    const KSRefreshPacket& packet()const noexcept{return packet_;}
    std::optional<KSGlideWords> glide(VoiceSlot slot)const noexcept
    {return size_t(slot)<glides_.size()?glides_[size_t(slot)]:std::nullopt;}
    std::optional<VoiceAllocation> allocation(VoiceSlot slot)const noexcept
    {return glide(slot) && before_[size_t(slot)]?std::optional{before_[size_t(slot)]->allocation}:std::nullopt;}
    const KSVoiceParameterBank::Observation* observation()const noexcept{return state_?&source_:nullptr;}
    const KSAllocatedVoiceSource* allocatedSource(VoiceSlot slot)const noexcept {
        const auto index=size_t(slot);return state_ && bindings_ && index<allocatedSources_.size() && allocatedSources_[index]?
            &*allocatedSources_[index]:nullptr;
    }

private:
    bool live()const noexcept {
        return state_ && !fault_ && session_->snapshot().session==state_->session() &&
            session_->snapshot().lifecycle==Lifecycle::Running && session_->rateGeneration()==source_.dependency.rateGeneration;
    }
    bool current(uint64_t contextRevision)const noexcept {
        const auto dependency=state_->dependency(source_.dependency.part,source_.dependency.rateGeneration);
        const auto model=state_->part(source_.dependency.part);
        if(!dependency || !model || dependency->session!=source_.dependency.session ||
           dependency->revisions.program!=source_.dependency.revisions.program ||
           dependency->routingRevision!=source_.dependency.routingRevision || contextRevision<contextRevision_ ||
           state_->voiceOperationRevision()!=ownerRevision_+(accepted_?1:0) ||
           (accepted_?!state_->ownsVoiceOperation(this):!state_->voiceOperationAvailable()))return false;
        const auto bank=bank_->observe(*dependency,model->parameters);if(!bank)return false;
        if(!accepted_ && (!(*dependency==source_.dependency) || bank->revision!=source_.revision || contextRevision!=contextRevision_))return false;
        for(size_t i=0;i<before_.size();++i) {
            const auto voice=state_->voice(VoiceSlot(i));const auto& before=before_[i];
            if(*state_->voiceGeneration(VoiceSlot(i))!=generations_[i] || bool(voice)!=bool(before) ||
               !(state_->deferredVoice(VoiceSlot(i))==pending_[i]) || (voice &&
               (!(voice->allocation==before->allocation) || voice->observationRevision!=before->observationRevision)))return false;
            if(bindings_ && allocatedSources_[i] &&
               !currentKsAllocatedVoiceSource(*allocatedSources_[i],*state_,*bank_,*bindings_,*store_,source_.dependency.rateGeneration,accepted_))return false;
        }
        return true;
    }
    KSRefreshProgress finish(uint64_t contextRevision)noexcept {
        const auto dependency=state_->dependency(source_.dependency.part,source_.dependency.rateGeneration);
        if(!dependency)return fail();
        if(contextRevision!=contextRevision_) {
            KSVoiceParameterDirty all;all.set();if(!bank_->invalidate(*dependency,all))return fail();
        } else {
            // A newer accepted desired edit owns its dirty revision. Failure
            // to acknowledge this older observation deliberately retains it.
            bank_->acknowledge(source_);
        }
        if(!state_->releaseVoiceOperation(this))return fail();complete_=true;return KSRefreshProgress::Complete;
    }
    DspAdmission reject(DspAdmission result)noexcept{if(accepted_)fail();return result;}
    KSRefreshProgress fail()noexcept {
        if(state_ && accepted_ && session_->snapshot().session==state_->session())session_->stop(state_->session());
        if(state_ && state_->ownsVoiceOperation(this))state_->releaseVoiceOperation(this);
        fault_=true;return KSRefreshProgress::Fault;
    }
    CommandSession* session_=nullptr;KSMusicalState* state_=nullptr;KSVoiceParameterBank* bank_=nullptr;
    const KSDrumVoiceBindings* bindings_=nullptr;const KSDrumProgramStore* store_=nullptr;
    KSAllocatedVoiceSources allocatedSources_{};
    KSVoiceParameterBank::Observation source_{};
    std::array<std::optional<KSGlideWords>,KSMusicalState::VoiceCount> glides_{};
    KSRefreshPacket packet_{};std::optional<DspTicket> ticket_;
    KSMusicalState::Voices::Projection before_{};
    KSMusicalState::DeferredVoices pending_{};
    std::array<uint64_t,KSMusicalState::VoiceCount> generations_{};
    uint64_t contextRevision_=0,ownerRevision_=0;size_t next_=0;
    bool accepted_=false,complete_=false,fault_=false;
};
}
