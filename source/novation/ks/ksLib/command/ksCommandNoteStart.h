#pragma once

#include "ksCommandVoiceImage.h"
#include "ksAllocatedVoiceSource.h"
#include "ksVoiceSelection.h"
#include <exception>

namespace novation::controller::command
{
    enum class KSNoteStartProgress { WaitingForTransfer, Complete, Fault };

    // One owned Note On intent. Continuations wait only for actual stop,
    // pitch reply and initial DSP transfers. No legacy execution stages.
    class KSCommandNoteStart final
    {
    public:
        KSCommandNoteStart() = default;
        // Session and musical state outlive this owner. Abandoning an accepted
        // unfinished intent is terminal; it must never leave queued old work.
        ~KSCommandNoteStart() { if (reserved_ && !complete_ && !fault_) fail(*session_); }
        KSCommandNoteStart(const KSCommandNoteStart&) = delete;
        KSCommandNoteStart& operator=(const KSCommandNoteStart&) = delete;

        bool prepare(CommandSession& session, KSMusicalState& state, KSStorageSlot part, VoiceSlot slot,
                     uint8_t note, uint8_t rawVelocity, uint8_t shapedVelocity, bool stopBeforeReuse,
                     KSNoteAllocation kind=KSNoteAllocation::PolyStart, bool publishKey=true,uint8_t noteOffExtra=0,bool suppressed=false) noexcept
        {
            if (state_ || session.snapshot().session != state.session() ||
                session.snapshot().lifecycle != Lifecycle::Running || note >= 128 ||
                !rawVelocity || rawVelocity >= 128 || (suppressed?shapedVelocity!=128:shapedVelocity>=128) ||
                !state.canAssignVoice(slot, part, note) || !state.voiceOperationAvailable())
                return false;
            const auto dependency = state.dependency(part, session.rateGeneration());
            const auto* keys = state.keys(part);
            const auto previous = state.voice(slot);
            const bool full=kind==KSNoteAllocation::PolyStart || kind==KSNoteAllocation::MonoStart;
            if (!dependency || !keys || !keys->canChange() || size_t(kind)>size_t(KSNoteAllocation::Reuse) ||
                (full ? bool(previous && previous->feedbackAge)!=stopBeforeReuse :
                        stopBeforeReuse || !previous || !previous->feedbackAge || previous->part!=part))
                return false;
            state_ = &state; session_ = &session; dependency_ = *dependency; slot_ = slot; note_ = note;
            raw_ = rawVelocity; shaped_ = shapedVelocity; keyRevision_ = keys->revision();
            kind_=kind;publishKey_=publishKey;noteOffExtra_=noteOffExtra;suppressed_=suppressed;
            startRevision_ = state.voiceOperationRevision();
            sustainRevision_ = state.sustain(part)->revision;
            stops_ = stopBeforeReuse ? 3 : 0;
            for (size_t i = 0; i < before_.size(); ++i)
            {
                before_[i] = state.voice(VoiceSlot(i));
                generations_[i] = *state.voiceGeneration(VoiceSlot(i));
            }
            return true;
        }

        DspAdmission admit(CommandSession& session, uint64_t operation, NativeFrame at) noexcept
        {
            if (!live(session)) return DspAdmission::StaleSession;
            if (reserved_) return DspAdmission::StaleOperation;
            if (!sourceCurrent(session) || !state_->canPublish(at)) return DspAdmission::InvalidDraft;
            std::array<DspTransactionDraft, 4> drafts{};
            const uint32_t cut = 0x200000, gateOff = 0;
            const uint16_t addresses[]{uint16_t(0x72 + 101*size_t(slot_)),
                                       uint16_t(0x7a + 101*size_t(slot_)),
                                       uint16_t(0x8085 + 22*size_t(slot_))};
            for (size_t i = 0; i < stops_; ++i)
            {
                drafts[i].model = Model::KS; drafts[i].endpoint = Endpoint::B; drafts[i].earliest = at;
                drafts[i].address = {DspAddressMode::Write, addresses[i]};
                drafts[i].words = i == 2 ? &gateOff : &cut; drafts[i].wordCount = 1;
            }
            auto& pitch = drafts[stops_];
            pitch.model = Model::KS; pitch.endpoint = Endpoint::B; pitch.earliest = at;
            pitch.address = {DspAddressMode::Read, uint16_t(0x808c + 22*size_t(slot_))};
            pitch.releaseEndpoint = DspMilestone::Observation;
            if (stops_) pitch.dependency.earlierDraft = uint8_t(stops_ - 1);
            return session.transactions().submit(state_->session(), operation, drafts.data(), stops_ + 1,
                startup_.data(), [&]() noexcept {
                    if (!state_->claimVoiceOperation(this)) std::terminate();
                    reserved_ = true;
                    if (!stops_) commit(at);
                });
        }

        KSNoteStartProgress finishStop(CommandSession& session, NativeFrame at) noexcept
        {
            if (committed_ && stops_ && live(session)) return KSNoteStartProgress::Complete;
            if (!live(session) || !reserved_ || !stops_ || !sourceCurrent(session) || !state_->canPublish(at) ||
                session.transactions().poisoned(Endpoint::B)) return fail(session);
            for (size_t i = 0; i < stops_; ++i)
            {
                DspTransactionProgress progress;
                if (!session.transactions().progress(startup_[i], progress)) return fail(session);
                if (progress.status == DspStatus::Queued) return KSNoteStartProgress::WaitingForTransfer;
                if (progress.status != DspStatus::Transferred) return fail(session);
            }
            if (!sourceCurrent(session) || !state_->canPublish(at)) return fail(session);
            commit(at);
            for (size_t i = 0; i < stops_; ++i)
                if (!session.transactions().retire(startup_[i])) return fail(session);
            return KSNoteStartProgress::Complete;
        }

        bool acceptPitch(CommandSession& session, DspReply reply) noexcept
        {
            if (!committed_ || pitchObserved_ || !current(session) ||
                reply.endpoint != Endpoint::B || reply.transaction != startup_[stops_].sequence || reply.word > 0xffffff)
                return fail(session), false;
            DspTransactionProgress progress;
            if (!session.transactions().progress(startup_[stops_], progress) || progress.status != DspStatus::Observed ||
                progress.address.address != 0x808c + 22*size_t(slot_) ||
                !session.transactions().retire(startup_[stops_]))
                return fail(session), false;
            pitchObserved_ = true;
            return true;
        }

        bool prepareInitial(CommandSession& session, const KSVoiceImagePatch& patch,
            const KSVoiceImageTables& tables, const KSVoiceImageContext& context, const KSCutoffWord& filter,
            const KSParameterVoiceContext* extended=nullptr,bool selectTracking=true) noexcept
        {
            return prepareInitialPayload(session,patch,tables,context,filter,extended,selectTracking,true);
        }
        bool prepareInitial(CommandSession& session,const KSAllocatedVoiceSource& source,
            const KSVoiceImageTables& tables,const KSVoiceImageContext& context,const KSVoiceParameterBank& bank,
            const KSDrumVoiceBindings& bindings,const KSDrumProgramStore& store,
            const KSParameterVoiceContext* extended=nullptr,bool selectTracking=true)noexcept
        {
            if(!state_ || !allocation_ || !(source.allocation==*allocation_) ||
               !currentKsAllocatedVoiceSource(source,*state_,bank,bindings,store,session.rateGeneration()) ||
               source.noteOffSuppression()!=source.patch[173] ||
               source.cutoff.observation.inputs.staticBase!=tables.filter)return false;
            auto input=ksAllocatedVoiceContext(source,extended?*extended:KSParameterVoiceContext{});
            if(!prepareInitialPayload(session,source.patch,tables,context,source.cutoff,&input,selectTracking,false))return false;
            allocatedSource_=source;sourceBank_=&bank;sourceBindings_=&bindings;sourceStore_=&store;return true;
        }
    private:
        bool prepareInitialPayload(CommandSession& session,const KSVoiceImagePatch& patch,
            const KSVoiceImageTables& tables,const KSVoiceImageContext& context,const KSCutoffWord& filter,
            const KSParameterVoiceContext* extended,bool selectTracking,bool livePartFilter)noexcept
        {
            if (suppressed_ || !pitchObserved_ || initialPrepared_ || !current(session) ||
                !(filter.observation.allocation == *allocation_))
                return false;
            if(livePartFilter) {
                const auto observed=state_->observeCutoff(dependency_.part,*allocation_,
                    filter.observation.inputs.staticBase,session.rateGeneration());
                if(!observed || !(*observed==filter.observation))return false;
            }
            auto input=extended?*extended:KSParameterVoiceContext{};input.voice=context;input.allocation=uint8_t(kind_);
            if(kind_==KSNoteAllocation::PolyStart || kind_==KSNoteAllocation::MonoStart) {
                const auto image=encodeKsParameterImage(patch,tables,input,filter);if(!image)return false;
                initial_=image->image;glide_=image->glide;initialCount_=selectTracking?3:2;
                gate_=encodeKsVoiceGate(patch,false);
            } else {
                const auto resume=encodeKsResumePacket(slot_,patch,tables,input,shaped_);if(!resume || resume->packet.size()!=8)return false;
                const size_t first=selectTracking?1:0;
                if(selectTracking){resumeAddresses_[0]=0x8046;resumeWords_[0]=uint32_t(size_t(slot_));}
                for(size_t i=0;i<8;++i) {
                    const auto write=resume->packet.at(i);if(!write || write->count!=1)return false;
                    resumeAddresses_[i+first]=write->address;resumeWords_[i+first]=*write->words;
                }
                glide_=resume->glide;initialCount_=8+first;gate_=resumeWords_[7+first];
            }
            selectTracking_=selectTracking;
            initialPrepared_=true;initialObservation_=filter.observation;
            return true;
        }
    public:
        DspAdmission admitInitial(CommandSession& session, uint64_t operation, NativeFrame at) noexcept
        {
            if (!live(session)) return DspAdmission::StaleSession;
            if (initialAdmitted_) return DspAdmission::StaleOperation;
            if (!initialPrepared_ || !current(session)) return DspAdmission::InvalidDraft;
            if(!allocatedSource_) {
                const auto observed=state_->observeCutoff(dependency_.part,*allocation_,
                    initialObservation_->inputs.staticBase,session.rateGeneration());
                if(!observed || !(*observed==*initialObservation_))return DspAdmission::InvalidDraft;
            }
            const uint32_t slot = uint32_t(slot_);
            std::array<DspTransactionDraft, 9> drafts{};
            for (auto& draft : drafts) { draft.model=Model::KS; draft.endpoint=Endpoint::B; draft.earliest=at; }
            if(initial_) {
                const size_t first=selectTracking_?1:0;
                if(selectTracking_){drafts[0].address={DspAddressMode::Write,0x8046};drafts[0].words=&slot;drafts[0].wordCount=1;}
                drafts[first].address={DspAddressMode::Write,uint16_t(0x2c+101*size_t(slot_))};
                drafts[first].words=initial_->image.data();drafts[first].wordCount=initial_->image.size();
                drafts[first+1].address={DspAddressMode::Write,uint16_t(0x2b+101*size_t(slot_))};
                drafts[first+1].words=&gate_;drafts[first+1].wordCount=1;
            } else for(size_t i=0;i<initialCount_;++i) {
                drafts[i].address={DspAddressMode::Write,resumeAddresses_[i]};drafts[i].words=&resumeWords_[i];drafts[i].wordCount=1;
            }
            return session.transactions().submit(state_->session(),operation,drafts.data(),initialCount_,
                initialTickets_.data(),[&]() noexcept { initialAdmitted_=true; });
        }

        KSNoteStartProgress finishInitial(CommandSession& session) noexcept
        {
            if (complete_ && live(session)) return KSNoteStartProgress::Complete;
            if (!initialAdmitted_ || !current(session) ||
                session.transactions().poisoned(Endpoint::B)) return fail(session);
            for (size_t i=0;i<initialCount_;++i)
            {
                const auto ticket=initialTickets_[i];
                DspTransactionProgress progress;
                if (!session.transactions().progress(ticket,progress)) return fail(session);
                if (progress.status==DspStatus::Queued) return KSNoteStartProgress::WaitingForTransfer;
                if (progress.status!=DspStatus::Transferred) return fail(session);
            }
            for (size_t i=0;i<initialCount_;++i)
                if (!session.transactions().retire(initialTickets_[i])) return fail(session);
            if (!state_->releaseVoiceOperation(this)) return fail(session);
            complete_=true;
            return KSNoteStartProgress::Complete;
        }

        KSNoteStartProgress finishSuppressed(CommandSession& session) noexcept
        {
            if(complete_ && suppressed_ && live(session))return KSNoteStartProgress::Complete;
            if(!suppressed_ || !committed_ || !current(session) || initialPrepared_ || !pitchObserved_ ||
                session.transactions().poisoned(Endpoint::B))
                return fail(session);
            if(!state_->releaseVoiceOperation(this))return fail(session);
            complete_=true;return KSNoteStartProgress::Complete;
        }
        bool suppressed()const noexcept{return suppressed_;}

        bool reset() noexcept
        {
            if (reserved_ && !complete_) return false;
            state_=nullptr;session_=nullptr;reserved_=committed_=pitchObserved_=initialAdmitted_=complete_=fault_=false;
            allocation_.reset();initial_.reset();initialObservation_.reset();
            initialPrepared_=false;initialCount_=0;glide_.reset();suppressed_=false;
            allocatedSource_.reset();sourceBank_=nullptr;sourceBindings_=nullptr;sourceStore_=nullptr;
            return true;
        }
        bool reserved() const noexcept { return reserved_; }
        KSNoteStartProgress abort(CommandSession& session) noexcept { return fail(session); }
        bool committed() const noexcept { return committed_; }
        VoiceSlot slot() const noexcept { return slot_; }
        size_t stopCount() const noexcept { return stops_; }
        const KSVoiceImageResult* initialImage() const noexcept { return initial_ ? &*initial_ : nullptr; }
        uint32_t gateWord() const noexcept { return gate_; }
        size_t initialCount()const noexcept{return initialCount_;}
        const KSGlideWords* glide()const noexcept{return glide_?&*glide_:nullptr;}
        bool appendInitial(KSRefreshPacket& packet)const noexcept {
            if(!initialPrepared_)return false;
            if(initial_)return (!selectTracking_ || packet.appendWord(Endpoint::B,0x8046,uint32_t(size_t(slot_)))) &&
                packet.append(Endpoint::B,uint16_t(0x2c+101*size_t(slot_)),initial_->image.data(),100) &&
                packet.appendWord(Endpoint::B,uint16_t(0x2b+101*size_t(slot_)),gate_);
            for(size_t i=0;i<initialCount_;++i)if(!packet.appendWord(Endpoint::B,resumeAddresses_[i],resumeWords_[i]))return false;
            return true;
        }
        std::optional<VoiceAllocation> allocation() const noexcept { return allocation_; }
        std::optional<DspTicket> stopTicket(size_t i) const noexcept
        { return reserved_ && !committed_ && i<stops_ ? std::optional{startup_[i]} : std::nullopt; }
        std::optional<DspTicket> pitchTicket() const noexcept
        { return reserved_ && !pitchObserved_ ? std::optional{startup_[stops_]} : std::nullopt; }
        std::optional<DspTicket> initialTicket(size_t i) const noexcept
        { return initialAdmitted_ && !complete_ && i<initialCount_ ? std::optional{initialTickets_[i]} : std::nullopt; }

    private:
        bool live(const CommandSession& session) const noexcept
        {
            const auto view=session.snapshot();
            return state_ && !fault_ && view.session==state_->session() && view.lifecycle==Lifecycle::Running &&
                   session.rateGeneration()==dependency_.rateGeneration;
        }
        bool dependencyCurrent(const CommandSession& session) const noexcept
        {
            const auto dependency=state_->dependency(dependency_.part,session.rateGeneration());
            const auto* sustain=state_->sustain(dependency_.part);
            return dependency && *dependency==dependency_ && sustain && sustain->revision==sustainRevision_;
        }
        bool sourceCurrent(const CommandSession& session) const noexcept
        {
            const auto* keys=state_->keys(dependency_.part);
            if (!dependencyCurrent(session) || !keys || keys->revision()!=keyRevision_ || !keys->canChange() ||
                state_->voiceOperationRevision()!=startRevision_+(reserved_?1:0) ||
                (reserved_ ? !state_->ownsVoiceOperation(this) : !state_->voiceOperationAvailable()) ||
                !state_->canAssignVoice(slot_,dependency_.part,note_)) return false;
            for (size_t i=0;i<before_.size();++i)
            {
                const auto now=state_->voice(VoiceSlot(i));
                if (*state_->voiceGeneration(VoiceSlot(i))!=generations_[i] || bool(now)!=bool(before_[i]) ||
                    (now && (!(now->allocation==before_[i]->allocation) ||
                             now->observationRevision!=before_[i]->observationRevision))) return false;
            }
            return true;
        }
        bool current(const CommandSession& session) const noexcept
        {
            if (!live(session) || !committed_ || !dependencyCurrent(session) ||
                !state_->ownsVoiceOperation(this) || state_->voiceOperationRevision()!=startRevision_+1) return false;
            const auto voice=state_->voice(*allocation_);
            const auto* keys=state_->keys(dependency_.part);
            return (!allocatedSource_ || currentKsAllocatedVoiceSource(*allocatedSource_,*state_,*sourceBank_,
                       *sourceBindings_,*sourceStore_,session.rateGeneration())) &&
                   voice && voice->observationRevision==0 && voice->feedbackAge==0x8000 &&
                   keys && keys->revision()==keyRevision_+(publishKey_?1:0);
        }
        void commit(NativeFrame at) noexcept
        {
            allocation_=state_->assignVoice(slot_,dependency_.part,note_,shaped_,0x8000,noteOffExtra_);
            if (!allocation_ || (publishKey_ && !state_->pressKey(dependency_.part,note_,raw_)) || !state_->publish(at)) std::terminate();
            committed_=true;
        }
        KSNoteStartProgress fail(CommandSession& session) noexcept
        {
            if (state_ && reserved_ && session.snapshot().session==state_->session()) session.stop(state_->session());
            if (state_ && state_->ownsVoiceOperation(this)) state_->releaseVoiceOperation(this);
            fault_=true;
            return KSNoteStartProgress::Fault;
        }
        KSMusicalState* state_=nullptr;
        CommandSession* session_=nullptr;
        ParameterDependency<KSStorageSlot> dependency_{};
        VoiceSlot slot_{};
        uint8_t note_=0,raw_=0,shaped_=0,noteOffExtra_=0;
        uint64_t keyRevision_=0,startRevision_=0,sustainRevision_=0;
        KSMusicalState::Voices::Projection before_{};
        std::array<uint64_t,KSMusicalState::VoiceCount> generations_{};
        std::array<DspTicket,4> startup_{};
        std::array<DspTicket,9> initialTickets_{};
        std::array<uint16_t,9> resumeAddresses_{};
        std::array<uint32_t,9> resumeWords_{};
        std::optional<VoiceAllocation> allocation_;
        std::optional<KSVoiceImageResult> initial_;
        std::optional<KSCutoffObservation> initialObservation_;
        std::optional<KSAllocatedVoiceSource> allocatedSource_;
        const KSVoiceParameterBank* sourceBank_=nullptr;
        const KSDrumVoiceBindings* sourceBindings_=nullptr;
        const KSDrumProgramStore* sourceStore_=nullptr;
        std::optional<KSGlideWords> glide_;
        uint32_t gate_=0;
        size_t stops_=0;
        size_t initialCount_=0;
        KSNoteAllocation kind_=KSNoteAllocation::PolyStart;
        bool publishKey_=true,initialPrepared_=false,selectTracking_=true,suppressed_=false;
        bool reserved_=false,committed_=false,pitchObserved_=false,initialAdmitted_=false,complete_=false,fault_=false;
    };
}
