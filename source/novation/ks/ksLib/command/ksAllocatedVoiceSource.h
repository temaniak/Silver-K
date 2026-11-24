#pragma once

#include "ksVoiceParameterBank.h"
#include "ksNoteAssignment.h"
#include "ksExpressionPacket.h"

namespace novation::controller::command {

// A value snapshot of one allocation's parameter source. Immutable drum
// records are copied while their cold store handle is valid; no borrowed
// record payload survives into a DSP transaction or expression packet.
struct KSAllocatedVoiceSource {
    VoiceAllocation allocation{};
    ParameterDependency<KSStorageSlot> dependency{};
    uint64_t bankRevision=0,observationRevision=0;
    uint16_t age=0;
    uint8_t note=0,velocity=0,noteOffExtra=0;
    KSNoteAssignment assignment{};
    KSVoiceImagePatch patch{};
    KSCutoffWord cutoff{};

    // VoicePopulation captures this value on allocation. Later patch edits
    // must not replace the allocation's ordinary NoteOff suppression flag.
    uint8_t noteOffSuppression()const noexcept{return noteOffExtra;}
    bool immutableRecord()const noexcept{return assignment.action==KSNoteAssignmentAction::PlayDrumRecord;}
};
using KSAllocatedVoiceSources=std::array<std::optional<KSAllocatedVoiceSource>,KSMusicalState::VoiceCount>;

inline bool sameKsNoteAssignment(const KSNoteAssignment& a,const KSNoteAssignment& b)noexcept {
    return a.action==b.action && a.kind==b.kind && a.dependency==b.dependency &&
        a.program.bank==b.program.bank && a.program.program==b.program.program && a.note==b.note &&
        a.bankIndex==b.bankIndex && a.sampleIndex==b.sampleIndex && a.storeGeneration==b.storeGeneration &&
        a.bankRevision==b.bankRevision && a.record==b.record;
}

inline std::optional<KSAllocatedVoiceSource> observeKsAllocatedVoiceSource(const KSMusicalState& state,
    const KSVoiceParameterBank& bank,const KSDrumVoiceBindings& bindings,const KSDrumProgramStore& store,
    VoiceAllocation allocation,uint16_t filterBase,uint64_t rate)noexcept {
    const auto voice=state.voice(allocation);const auto binding=bindings.binding(allocation);
    // Allocation ownership survives a zero-age feedback observation. A last
    // tracked slot still supplies tracking parameters after its sound ends;
    // live image/expression cohorts separately filter feedbackAge below.
    if(!voice || !binding)return std::nullopt;
    const auto dependency=state.dependency(voice->part,rate);const auto part=state.part(voice->part);
    const auto live=dependency && part?bank.observe(*dependency,part->parameters):std::nullopt;
    if(!live)return std::nullopt;
    const auto* resolved=bindings.resolve(state,allocation,*dependency,live->patch,store);
    if(!resolved || (*resolved)[46]>=128 || (*resolved)[150]>=128)return std::nullopt;
    KSAllocatedVoiceSource result;
    result.allocation=allocation;result.dependency=*dependency;result.bankRevision=live->revision;
    result.observationRevision=voice->observationRevision;result.age=voice->feedbackAge;
    const auto extra=state.noteOffExtra(allocation);if(!extra)return std::nullopt;
    result.note=voice->note;result.velocity=voice->velocity;result.noteOffExtra=*extra;
    result.assignment=binding->source;result.patch=*resolved;
    KSCutoffObservation observed{*dependency,allocation,voice->observationRevision,
        {{result.patch[46],result.patch[150]},voice->velocity,filterBase}};
    result.cutoff={observed,encodeKsCutoff(observed.inputs)};return result;
}

// Checks only handles/tokens after packet construction. In particular it
// never dereferences the captured record pointer after a store replacement.
// The store owner must advance its generation before releasing old records.
inline bool currentKsAllocatedVoiceSource(const KSAllocatedVoiceSource& source,const KSMusicalState& state,
    const KSVoiceParameterBank& bank,const KSDrumVoiceBindings& bindings,const KSDrumProgramStore& store,
    uint64_t rate,bool desiredMayAdvance=false)noexcept {
    if(source.immutableRecord()) {
        if(store.session!=source.dependency.session || store.generation!=source.assignment.storeGeneration ||
           source.assignment.bankIndex>=store.banks.size())return false;
        const auto& records=store.banks[source.assignment.bankIndex];
        if(!records.records || records.revision!=source.assignment.bankRevision ||
           records.count<49 || records.count>50 || source.assignment.sampleIndex>=records.count)return false;
    }
    const auto voice=state.voice(source.allocation);const auto binding=bindings.binding(source.allocation);
    if(!voice || !binding || voice->part!=source.dependency.part || voice->note!=source.note ||
       voice->velocity!=source.velocity || voice->feedbackAge!=source.age || voice->observationRevision!=source.observationRevision ||
       state.noteOffExtra(source.allocation)!=source.noteOffExtra ||
       !sameKsNoteAssignment(binding->source,source.assignment))return false;
    const auto dependency=state.dependency(voice->part,rate);const auto model=state.part(voice->part);
    if(!dependency || dependency->session!=source.dependency.session || dependency->part!=source.dependency.part ||
       dependency->revisions.program!=source.dependency.revisions.program ||
       dependency->routingRevision!=source.dependency.routingRevision || dependency->rateGeneration!=source.dependency.rateGeneration)
        return false;
    const auto observed=model?bank.observe(*dependency,model->parameters):std::nullopt;
    if(!observed || (!desiredMayAdvance && (!(*dependency==source.dependency) || observed->revision!=source.bankRevision)))return false;
    return bindings.resolve(state,source.allocation,*dependency,source.patch,store)!=nullptr;
}

inline std::optional<KSAllocatedVoiceSources> observeKsAllocatedVoiceSources(const KSMusicalState& state,
    const KSVoiceParameterBank& bank,const KSDrumVoiceBindings& bindings,const KSDrumProgramStore& store,
    uint16_t filterBase,uint64_t rate,std::optional<KSStorageSlot> part=std::nullopt)noexcept {
    if(part && size_t(*part)>=KSMusicalState::StorageSlotCount)return std::nullopt;
    KSAllocatedVoiceSources result{};
    for(size_t i=0;i<result.size();++i) {
        const auto voice=state.voice(VoiceSlot(i));
        if(!voice || !voice->feedbackAge || (part && voice->part!=*part))continue;
        result[i]=observeKsAllocatedVoiceSource(state,bank,bindings,store,voice->allocation,filterBase,rate);
        if(!result[i])return std::nullopt;
    }
    return result;
}

inline KSParameterVoiceContext ksAllocatedVoiceContext(const KSAllocatedVoiceSource& source,
    KSParameterVoiceContext context)noexcept {
    context.unisonSpread=source.patch[1];return context;
}

inline std::optional<KSParameterImageResult> encodeKsAllocatedInitialImage(const KSAllocatedVoiceSource& source,
    const KSVoiceImageTables& tables,const KSParameterVoiceContext& context)noexcept {
    if(!(source.cutoff.observation.allocation==source.allocation) ||
       source.cutoff.observation.inputs.staticBase!=tables.filter)return std::nullopt;
    return encodeKsParameterImage(source.patch,tables,ksAllocatedVoiceContext(source,context),source.cutoff);
}

// Every source is an owned value and every output payload is copied into the
// returned packet. The temporary patch pointers used by the pure encoder
// cannot escape this call or outlive these source snapshots.
inline std::optional<KSRefreshPacket> encodeKsAllocatedExpressionPacket(Session session,KSLayout layout,
    KSStorageSlot part,KSExpressionField field,const KSExpressionValues& values,bool suppressed,
    const KSVoiceImageTables& tables,const KSAllocatedVoiceSources& sources,
    const std::array<KSParameterVoiceContext,KSMusicalState::VoiceCount>& contexts)noexcept {
    std::array<KSExpressionVoice,KSMusicalState::VoiceCount> voices{};size_t count=0;
    for(size_t i=0;i<sources.size();++i)if(sources[i]) {
        const auto& source=*sources[i];
        if(size_t(source.allocation.slot)!=i || source.allocation.session!=session || !source.allocation.generation ||
           !(source.cutoff.observation.allocation==source.allocation) || !(source.cutoff.observation.part==source.dependency))
            return std::nullopt;
        voices[count++]={source.allocation,source.dependency.part,source.age,&source.patch,ksAllocatedVoiceContext(source,contexts[i])};
    }
    return encodeKsExpressionPacket(session,layout,part,field,values,suppressed,tables,voices.data(),count);
}
}
