#pragma once

#include "ksCommandParameterImage.h"
#include <bitset>

namespace novation::controller::command {
enum class KSVoiceParameterGroup:uint8_t { Header,Oscillator1,Oscillator2,Oscillator3,Mixer,Filter,
    Envelopes,Clock,FilterEnvelope,Lfo1,Lfo2,Glide,Gate,Count };
using KSVoiceParameterDirty=std::bitset<size_t(KSVoiceParameterGroup::Count)>;

// The unpacked, model-owned byte index is a serialized parameter identity,
// never a firmware address or an execution boundary. Globals/FX and the
// musical model's cutoff/depth have separate authoritative owners.
struct KSVoiceParameterEdit { uint16_t field=0;uint8_t value=0; };
inline KSVoiceParameterDirty ksVoiceParameterGroups(uint16_t field)noexcept {
    KSVoiceParameterDirty groups;
    const auto add=[&](KSVoiceParameterGroup group){groups.set(size_t(group));};
    if(field==0) {add(KSVoiceParameterGroup::Filter);add(KSVoiceParameterGroup::Clock);add(KSVoiceParameterGroup::Glide);
        for(unsigned i=1;i<=3;++i)groups.set(i);}
    if(field==6){add(KSVoiceParameterGroup::Header);for(unsigned i=1;i<=3;++i)groups.set(i);}
    if(field==1 || field==2 || (field>=31&&field<=36))for(unsigned i=1;i<=3;++i)groups.set(i);
    for(unsigned lane=0;lane<3;++lane)if((field>=7+8*lane && field<=14+8*lane) || field==128+lane)groups.set(1+lane);
    for(auto f:{37,38,39,40,41,42,43,44,45,141,142,143,144,145,146,171,222})
        if(field==f)add(KSVoiceParameterGroup::Mixer);
    if((field>=46&&field<=55) || field==140 || field==150)add(KSVoiceParameterGroup::Filter);
    if((field>=56&&field<=65) || field==172 || field==173)add(KSVoiceParameterGroup::Envelopes);
    if(field==125 || field==148)add(KSVoiceParameterGroup::Clock);
    if(field>=66&&field<=70)add(KSVoiceParameterGroup::FilterEnvelope);
    for(unsigned lane=0;lane<2;++lane)if((field>=72+3*lane&&field<=74+3*lane) || field==131+lane || field==133+lane ||
        field==135+lane || field==137+lane || field==169+lane)groups.set(size_t(KSVoiceParameterGroup::Lfo1)+lane);
    if((field>=3&&field<=5) || field==168)add(KSVoiceParameterGroup::Glide);
    if(field==4 || field==78 || field==79 || field==80 || field==147)add(KSVoiceParameterGroup::Gate);
    return groups;
}

class KSVoiceParameterBank final {
public:
    struct Observation {
        ParameterDependency<KSStorageSlot> dependency;
        uint64_t revision=0;
        KSVoiceImagePatch patch{};
        KSVoiceParameterDirty dirty;
    };
    struct Edit {
        ParameterDependency<KSStorageSlot> dependency;
        uint64_t revision=0;
        KSVoiceImagePatch patch{};
        KSVoiceParameterDirty changed;
    };
    explicit KSVoiceParameterBank(Session session)noexcept:session_(session){}
    KSVoiceParameterBank(const KSVoiceParameterBank&)=delete;
    KSVoiceParameterBank& operator=(const KSVoiceParameterBank&)=delete;

    static bool owns(uint16_t field)noexcept {
        return field<256 && field!=46 && field!=150 && ksVoiceParameterGroups(field).any();
    }
    bool canLoad(KSStorageSlot slot,const KSVoiceImagePatch& patch)const noexcept {
        const auto index=size_t(slot);
        return index<parts_.size() && parts_[index].revision!=std::numeric_limits<uint64_t>::max() && validPatch(patch);
    }
    bool load(ParameterDependency<KSStorageSlot> source,const KSVoiceImagePatch& patch)noexcept {
        if(source.session!=session_ || !canLoad(source.part,patch))return false;
        auto& part=parts_[size_t(source.part)];
        if(part.source && source.revisions.program<=part.source->revisions.program)return false;
        part.source=source;part.patch=patch;++part.revision;part.dirty.set();return true;
    }
    std::optional<Edit> prepare(ParameterDependency<KSStorageSlot> current,const KSVoiceParameterEdit* edits,size_t count)const noexcept {
        if(!matches(current) || !edits || !count || count>256)return std::nullopt;
        const auto& part=parts_[size_t(current.part)];if(part.revision==std::numeric_limits<uint64_t>::max())return std::nullopt;
        Edit result{current,part.revision,part.patch,{}};std::bitset<256> seen;
        for(size_t i=0;i<count;++i) {
            const auto edit=edits[i];if(!owns(edit.field) || seen.test(edit.field))return std::nullopt;
            seen.set(edit.field);
            if(result.patch[edit.field]!=edit.value){result.patch[edit.field]=edit.value;result.changed|=ksVoiceParameterGroups(edit.field);}
        }
        if(!validPatch(result.patch))return std::nullopt;return result;
    }
    // The worker prevalidates its corresponding musical state change, commits
    // both owners without callbacks, then publishes the combined snapshot.
    bool commit(const Edit& edit,ParameterDependency<KSStorageSlot> before,ParameterDependency<KSStorageSlot> after)noexcept {
        if(!matches(before) || !(before==edit.dependency) || before.session!=after.session || before.part!=after.part ||
           before.routingRevision!=after.routingRevision || before.rateGeneration!=after.rateGeneration ||
           before.revisions.program!=after.revisions.program ||
           before.revisions.parameters==std::numeric_limits<uint64_t>::max() ||
           after.revisions.parameters!=before.revisions.parameters+1 || !validPatch(edit.patch))return false;
        auto& part=parts_[size_t(before.part)];
        if(part.revision!=edit.revision || part.revision==std::numeric_limits<uint64_t>::max())return false;
        // Public plans cannot write another owner's cold storage fields.
        KSVoiceParameterDirty expected;
        for(size_t field=0;field<256;++field)if(part.patch[field]!=edit.patch[field]) {
            if(!owns(uint16_t(field)))return false;expected|=ksVoiceParameterGroups(uint16_t(field));
        }
        if(expected!=edit.changed)return false;
        part.patch=edit.patch;part.source=after;part.dirty|=edit.changed;++part.revision;return true;
    }
    // Cutoff/depth are overlaid from the current musical model. Globals in
    // the returned serialized container are cold-only and unused by the
    // voice compiler; this bank never edits or publishes them as live FX.
    std::optional<Observation> observe(ParameterDependency<KSStorageSlot> current,KSFilterParameters filter)const noexcept {
        if(!matches(current) || filter.cutoff>=128 || filter.velocityDepth>=128)return std::nullopt;
        const auto& part=parts_[size_t(current.part)];auto patch=part.patch;patch[46]=filter.cutoff;patch[150]=filter.velocityDepth;
        return Observation{current,part.revision,patch,part.dirty};
    }
    bool acknowledge(const Observation& accepted)noexcept {
        if(!matches(accepted.dependency))return false;
        auto& part=parts_[size_t(accepted.dependency.part)];
        if(part.revision!=accepted.revision)return false;
        part.dirty&=~accepted.dirty;return true;
    }
    bool rebase(ParameterDependency<KSStorageSlot> before,ParameterDependency<KSStorageSlot> after)noexcept {
        if(!matches(before) || before.session!=after.session || before.part!=after.part ||
           before.routingRevision!=after.routingRevision || before.revisions.program!=after.revisions.program ||
           after.revisions.parameters<before.revisions.parameters || after.rateGeneration<before.rateGeneration)return false;
        parts_[size_t(before.part)].source=after;return true;
    }
    bool invalidate(ParameterDependency<KSStorageSlot> current,KSVoiceParameterDirty groups)noexcept {
        if(!matches(current))return false;auto& part=parts_[size_t(current.part)];
        if(part.revision==std::numeric_limits<uint64_t>::max())return false;
        part.dirty|=groups;++part.revision;return true;
    }
private:
    static bool validPatch(const KSVoiceImagePatch& patch)noexcept {
        for(auto field:{11,19,27,52})if(patch[field]>=128)return false;
        for(auto field:{73,76,137,138})if(patch[field]>34)return false;return true;
    }
    bool matches(ParameterDependency<KSStorageSlot> source)const noexcept {
        return source.session==session_ && size_t(source.part)<parts_.size() &&
            parts_[size_t(source.part)].source && *parts_[size_t(source.part)].source==source;
    }
    struct Part {
        std::optional<ParameterDependency<KSStorageSlot>> source;
        uint64_t revision=0;KSVoiceImagePatch patch{};KSVoiceParameterDirty dirty;
    };
    Session session_;std::array<Part,KSMusicalState::StorageSlotCount> parts_{};
};
}
