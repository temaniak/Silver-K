#pragma once

#include "ksCommandVoiceImage.h"

namespace novation::controller::command {
// Backing records have stable addresses. Writable catalogs give each drum
// allocation its own image so a bank import cannot change a sounding voice.
struct KSDrumProgramBank {
    const KSVoiceImagePatch* records=nullptr;
    size_t count=0;
    uint64_t revision=0;
};
struct KSDrumProgramStore {
    Session session{};
    uint64_t generation=0;
    std::array<KSDrumProgramBank,4> banks{};
    bool snapshotRecords=false;
};
enum class KSNoteAssignmentKind { Select,PlayCurrent };
enum class KSNoteAssignmentAction { PlayCurrent,PlayDrumRecord,IgnoreNote,InvalidProgram,MissingRecord,StaleStore };
struct KSNoteAssignment {
    KSNoteAssignmentAction action=KSNoteAssignmentAction::InvalidProgram;
    KSNoteAssignmentKind kind=KSNoteAssignmentKind::Select;
    ParameterDependency<KSStorageSlot> dependency{};
    ProgramSelection program{};
    uint8_t note=0,bankIndex=0,sampleIndex=0;
    uint64_t storeGeneration=0,bankRevision=0;
    const KSVoiceImagePatch* record=nullptr;
    bool playable()const noexcept {
        return action==KSNoteAssignmentAction::PlayCurrent || action==KSNoteAssignmentAction::PlayDrumRecord;
    }
};
inline KSNoteAssignment selectKsNoteAssignment(KSNoteAssignmentKind kind,ParameterDependency<KSStorageSlot> dependency,
    ProgramSelection program,uint8_t note,const KSDrumProgramStore& store)noexcept {
    KSNoteAssignment result;result.kind=kind;result.dependency=dependency;result.program=program;result.note=note;
    if(size_t(dependency.part)>=KSMusicalState::StorageSlotCount || !dependency.session.owner ||
       !dependency.session.generation || !dependency.revisions.program || note>=128)return result;
    if(kind==KSNoteAssignmentKind::PlayCurrent || program.bank<=4) {
        result.action=KSNoteAssignmentAction::PlayCurrent;return result;
    }
    if(program.bank>8 || program.program>=50)return result;
    if(note<36 || note>84){result.action=KSNoteAssignmentAction::IgnoreNote;return result;}
    result.bankIndex=uint8_t(program.bank-5);result.sampleIndex=uint8_t(note-36);
    if(result.sampleIndex==program.program){result.action=KSNoteAssignmentAction::PlayCurrent;return result;}
    if(store.session!=dependency.session || !store.generation){result.action=KSNoteAssignmentAction::StaleStore;return result;}
    const auto& bank=store.banks[result.bankIndex];
    if(!bank.records || bank.count<49 || bank.count>50 || !bank.revision || result.sampleIndex>=bank.count) {
        result.action=KSNoteAssignmentAction::MissingRecord;return result;
    }
    result.action=KSNoteAssignmentAction::PlayDrumRecord;result.storeGeneration=store.generation;
    result.bankRevision=bank.revision;result.record=bank.records+result.sampleIndex;return result;
}

class KSDrumVoiceBindings final {
public:
    struct Binding {
        VoiceAllocation allocation;
        KSNoteAssignment source;
        std::optional<KSVoiceImagePatch> snapshot;
    };
    explicit KSDrumVoiceBindings(Session session)noexcept:session_(session){}
    KSDrumVoiceBindings(const KSDrumVoiceBindings&)=delete;
    KSDrumVoiceBindings& operator=(const KSDrumVoiceBindings&)=delete;
    bool bind(const KSMusicalState& state,VoiceAllocation allocation,const KSNoteAssignment& source,
              const KSDrumProgramStore& store)noexcept {
        if(state.session()!=session_ || allocation.session!=session_ || !source.playable())return false;
        const auto voice=state.voice(allocation);const auto current=state.dependency(source.dependency.part,source.dependency.rateGeneration);
        const auto part=state.part(source.dependency.part);
        if(!voice || voice->part!=source.dependency.part || voice->note!=source.note || !current || !(*current==source.dependency) ||
           !part || !part->program || part->program->bank!=source.program.bank || part->program->program!=source.program.program)return false;
        const auto selected=selectKsNoteAssignment(source.kind,*current,source.program,source.note,store);
        if(selected.action!=source.action || selected.record!=source.record || selected.storeGeneration!=source.storeGeneration ||
           selected.bankRevision!=source.bankRevision || selected.bankIndex!=source.bankIndex || selected.sampleIndex!=source.sampleIndex)return false;
        auto& binding=bindings_[size_t(allocation.slot)].emplace(Binding{allocation,source,{}});
        if(store.snapshotRecords && source.record)binding.snapshot.emplace(*source.record);
        return true;
    }
    std::optional<Binding> binding(VoiceAllocation allocation)const noexcept {
        if(allocation.session!=session_ || size_t(allocation.slot)>=bindings_.size())return std::nullopt;
        const auto& result=bindings_[size_t(allocation.slot)];return result && result->allocation==allocation?result:std::nullopt;
    }
    const KSVoiceImagePatch* resolve(const KSMusicalState& state,VoiceAllocation allocation,
        ParameterDependency<KSStorageSlot> liveDependency,const KSVoiceImagePatch& livePatch,const KSDrumProgramStore& store)const noexcept {
        if(allocation.session!=session_ || size_t(allocation.slot)>=bindings_.size())return nullptr;
        const auto& owned=bindings_[size_t(allocation.slot)];
        const auto voice=state.voice(allocation);
        if(!owned || !(owned->allocation==allocation) || state.session()!=session_ || !voice ||
           voice->part!=owned->source.dependency.part || voice->note!=owned->source.note)return nullptr;
        const auto& source=owned->source;
        if(source.action==KSNoteAssignmentAction::PlayCurrent) {
            const auto current=state.dependency(source.dependency.part,liveDependency.rateGeneration);
            if(!current || !(*current==liveDependency) || liveDependency.session!=source.dependency.session ||
               liveDependency.part!=source.dependency.part || liveDependency.revisions.program!=source.dependency.revisions.program ||
               liveDependency.routingRevision!=source.dependency.routingRevision)return nullptr;
            return &livePatch;
        }
        if(store.session!=session_ || store.generation!=source.storeGeneration || source.bankIndex>=store.banks.size())return nullptr;
        if(owned->snapshot)return &*owned->snapshot;
        const auto& bank=store.banks[source.bankIndex];
        return bank.records && bank.count>=49 && bank.count<=50 && source.sampleIndex<bank.count &&
            bank.revision==source.bankRevision && bank.records+source.sampleIndex==source.record?source.record:nullptr;
    }
    // A program load replaces the mutable part patch. Existing editable
    // allocations keep that live source; immutable drum tails retain their
    // original record. This is an explicit program-transition commit, never
    // an automatic stale-source fallback during packet preparation.
    bool rebindEditablePart(const KSMusicalState& state,KSStorageSlot part,uint64_t rate)noexcept {
        const auto dependency=state.dependency(part,rate);const auto model=state.part(part);
        if(state.session()!=session_ || !dependency || !model || !model->program)return false;
        auto next=bindings_;
        for(auto& binding:next)if(binding && binding->source.dependency.part==part &&
            binding->source.action==KSNoteAssignmentAction::PlayCurrent) {
            const auto voice=state.voice(binding->allocation);
            if(!voice || voice->part!=part || voice->note!=binding->source.note)return false;
            KSNoteAssignment source;source.action=KSNoteAssignmentAction::PlayCurrent;
            source.kind=KSNoteAssignmentKind::PlayCurrent;source.dependency=*dependency;source.program=*model->program;
            source.note=voice->note;binding->source=source;
        }
        bindings_=next;return true;
    }
    bool retire(VoiceAllocation allocation)noexcept {
        if(!binding(allocation))return false;bindings_[size_t(allocation.slot)].reset();return true;
    }
private:
    Session session_;std::array<std::optional<Binding>,KSMusicalState::VoiceCount> bindings_{};
};
}
