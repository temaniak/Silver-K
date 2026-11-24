#pragma once

#include "ksMusicalState.h"

namespace novation::controller::command {
// The eight latched GlobalSetup values, in serialized179..186 order. These
// are full byte observations; this owner does not invent MIDI/UI clamping.
struct KSPartConfigurationValues {
    uint8_t level=0,output=0,channel=0,lowKey=0,highKey=127,transpose=64,detune=64,velocityCurve=0;
    static constexpr size_t FirstField=179,FieldCount=8;
    static constexpr KSPartConfigurationValues fromBytes(const std::array<uint8_t,8>& bytes)noexcept {
        return {bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7]};
    }
    static constexpr KSPartConfigurationValues fromPatch(const std::array<uint8_t,256>& patch)noexcept {
        return {patch[179],patch[180],patch[181],patch[182],patch[183],patch[184],patch[185],patch[186]};
    }
    constexpr std::array<uint8_t,8> bytes()const noexcept {
        return {level,output,channel,lowKey,highKey,transpose,detune,velocityCurve};
    }
    friend constexpr bool operator==(KSPartConfigurationValues a,KSPartConfigurationValues b)noexcept {
        return a.level==b.level && a.output==b.output && a.channel==b.channel && a.lowKey==b.lowKey &&
            a.highKey==b.highKey && a.transpose==b.transpose && a.detune==b.detune && a.velocityCurve==b.velocityCurve;
    }
};
static_assert(sizeof(KSPartConfigurationValues)==8,"Eight byte configuration tuple");

// A native composition epoch, not an active-musical-part dependency. Passive
// Single storage1..4 and Performance storage0 also have latched controls.
struct KSPartConfigurationSource {
    Session session{};KSStorageSlot storage=KSStorageSlot::Single;uint64_t sourceRevision=0;
    friend constexpr bool operator==(KSPartConfigurationSource a,KSPartConfigurationSource b)noexcept {
        return a.session==b.session && a.storage==b.storage && a.sourceRevision==b.sourceRevision;
    }
};

// Owns only applied/latched values. Desired patch, metadata and FX remain in
// their existing owners. A caller captures the complete tuple at the native
// GlobalSetup semantic boundary, after voice images and before global words.
// No model keys, voice assignment, firmware address or execution phase lives
// here. In particular five storage tuples do not create five musical parts.
class KSPartConfigurationState final {
public:
    struct Projection {
        Session session{};KSStorageSlot storage=KSStorageSlot::Single;uint64_t version=0;
        KSPartConfigurationValues values{};uint8_t noteDispatchMode=0;
        std::optional<KSPartConfigurationSource> source;
    };
    struct Activation {
        Session session{};KSStorageSlot storage=KSStorageSlot::Single;uint64_t beforeVersion=0;
        KSPartConfigurationValues values{};uint8_t noteDispatchMode=0;KSPartConfigurationSource source{};
    };
    explicit KSPartConfigurationState(Session session)noexcept:session_(session){}
    KSPartConfigurationState(const KSPartConfigurationState&)=delete;
    KSPartConfigurationState& operator=(const KSPartConfigurationState&)=delete;
    Session session()const noexcept{return session_;}
    bool prepareCold(KSStorageSlot part,KSPartConfigurationValues values,uint8_t noteDispatchMode=0)noexcept {
        const auto index=size_t(part);
        if(!validSession() || index>=parts_.size() || parts_[index].version)return false;
        parts_[index].values=values;parts_[index].noteDispatchMode=noteDispatchMode;parts_[index].version=1;return true;
    }
    std::optional<Projection> read(KSStorageSlot part)const noexcept {
        const auto index=size_t(part);if(index>=parts_.size() || !parts_[index].version)return std::nullopt;
        const auto& value=parts_[index];return Projection{session_,part,value.version,value.values,value.noteDispatchMode,value.source};
    }
    bool current(const Projection& snapshot)const noexcept {
        const auto value=read(snapshot.storage);
        return value && snapshot.session==session_ && snapshot.version==value->version &&
            snapshot.values==value->values && snapshot.noteDispatchMode==value->noteDispatchMode && snapshot.source==value->source;
    }
    // GlobalSetup activates both the eight controls and the separately cached
    // note-dispatch mode. This full byte controls NoteOff replacement and the
    // future arp gate hook; allocation still reads its selected note program.
    std::optional<Activation> prepareActivation(KSStorageSlot part,KSPartConfigurationValues values,
        uint8_t noteDispatchMode,KSPartConfigurationSource source)const noexcept {
        const auto before=read(part);
        if(!validSource(part,source) || !before ||
           (before->source && source.sourceRevision<before->source->sourceRevision) ||
           (before->version==std::numeric_limits<uint64_t>::max() && !same(*before,values,noteDispatchMode,source)))return std::nullopt;
        return Activation{session_,part,before->version,values,noteDispatchMode,source};
    }
    // Independent bridges such as System242 change only control properties;
    // omitting the mode preserves the current applied dispatch policy.
    std::optional<Activation> prepareActivation(KSStorageSlot part,KSPartConfigurationValues values,
        KSPartConfigurationSource source)const noexcept {
        const auto before=read(part);return before?prepareActivation(part,values,before->noteDispatchMode,source):std::nullopt;
    }
    bool canActivate(KSStorageSlot part,KSPartConfigurationValues values,uint8_t noteDispatchMode,
        KSPartConfigurationSource source)const noexcept {
        return bool(prepareActivation(part,values,noteDispatchMode,source));
    }
    bool canActivate(KSStorageSlot part,KSPartConfigurationValues values,KSPartConfigurationSource source)const noexcept {
        return bool(prepareActivation(part,values,source));
    }
    bool commit(const Activation& activation)noexcept {
        const auto before=read(activation.storage);
        if(activation.session!=session_ || !validSource(activation.storage,activation.source) || !before ||
           before->version!=activation.beforeVersion ||
           (before->source && activation.source.sourceRevision<before->source->sourceRevision))return false;
        if(same(*before,activation.values,activation.noteDispatchMode,activation.source))return true;
        if(before->version==std::numeric_limits<uint64_t>::max())return false;
        auto& value=parts_[size_t(activation.storage)];value.values=activation.values;value.noteDispatchMode=activation.noteDispatchMode;
        value.source=activation.source;++value.version;return true;
    }
    bool activate(KSStorageSlot part,KSPartConfigurationValues values,uint8_t noteDispatchMode,
        KSPartConfigurationSource source)noexcept {
        const auto activation=prepareActivation(part,values,noteDispatchMode,source);return activation && commit(*activation);
    }
    bool activate(KSStorageSlot part,KSPartConfigurationValues values,KSPartConfigurationSource source)noexcept {
        const auto activation=prepareActivation(part,values,source);return activation && commit(*activation);
    }
    bool canActivate(KSStorageSlot part,KSPartConfigurationValues values,uint64_t sourceRevision)const noexcept {
        return canActivate(part,values,{session_,part,sourceRevision});
    }
    bool activate(KSStorageSlot part,KSPartConfigurationValues values,uint64_t sourceRevision)noexcept {
        return activate(part,values,{session_,part,sourceRevision});
    }
    std::optional<Activation> prepareActivation(KSStorageSlot part,KSPartConfigurationValues values,
        uint8_t noteDispatchMode,uint64_t sourceRevision)const noexcept {
        return prepareActivation(part,values,noteDispatchMode,{session_,part,sourceRevision});
    }
    bool canActivate(KSStorageSlot part,KSPartConfigurationValues values,uint8_t noteDispatchMode,uint64_t sourceRevision)const noexcept {
        return canActivate(part,values,noteDispatchMode,{session_,part,sourceRevision});
    }
    bool activate(KSStorageSlot part,KSPartConfigurationValues values,uint8_t noteDispatchMode,uint64_t sourceRevision)noexcept {
        return activate(part,values,noteDispatchMode,{session_,part,sourceRevision});
    }
private:
    struct Part {KSPartConfigurationValues values{};uint8_t noteDispatchMode=0;uint64_t version=0;std::optional<KSPartConfigurationSource> source;};
    bool validSession()const noexcept{return session_.owner && session_.generation;}
    bool validSource(KSStorageSlot part,KSPartConfigurationSource source)const noexcept {
        return validSession() && size_t(part)<parts_.size() && source.session==session_ && source.storage==part;
    }
    static bool same(const Projection& before,KSPartConfigurationValues values,uint8_t noteDispatchMode,KSPartConfigurationSource source)noexcept {
        return before.source && *before.source==source && before.values==values && before.noteDispatchMode==noteDispatchMode;
    }
    Session session_;std::array<Part,KSMusicalState::StorageSlotCount> parts_{};
};
}
