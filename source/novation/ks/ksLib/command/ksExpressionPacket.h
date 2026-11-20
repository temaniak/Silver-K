#pragma once

#include "ksCommandExpression.h"
#include "ksCommandParameterImage.h"
#include "../ksParameterDependencies.h"

namespace novation::controller::command {
struct KSExpressionVoice {
    VoiceAllocation allocation{};
    KSStorageSlot part=KSStorageSlot::Single;
    uint16_t age=0;
    const KSVoiceImagePatch* patch=nullptr;
    KSParameterVoiceContext context{};
};

// A complete immutable expression operation. Only the verified partial voice
// groups are emitted: three pitch words OR four modulation words, followed by
// the separate six-source globals tail for wheel/pressure/breath. A full100-word
// image is deliberately not substituted for these side-effect-sensitive writes.
inline std::optional<KSRefreshPacket> encodeKsExpressionPacket(Session session,KSLayout layout,KSStorageSlot part,
    KSExpressionField field,const KSExpressionValues& sources,bool refreshSuppressed,const KSVoiceImageTables& tables,
    const KSExpressionVoice* voices,size_t count)noexcept {
    if(!session.owner || !session.generation || size_t(part)>=KSMusicalState::StorageSlotCount ||
       (layout!=KSLayout::Single && layout!=KSLayout::Performance) ||
       (layout==KSLayout::Single?part!=KSStorageSlot::Single:part==KSStorageSlot::Single) ||
       size_t(field)>size_t(KSExpressionField::Breath) || sources.wheelPitch>512 ||
       count>KSMusicalState::VoiceCount || (count && !voices))return std::nullopt;
    for(const auto value:sources.modulation)if(value>=128)return std::nullopt;
    KSRefreshPacket packet;
    if(refreshSuppressed)return packet;
    size_t previous=0;
    for(size_t i=0;i<count;++i) {
        const auto& voice=voices[i];const auto index=size_t(voice.allocation.slot);
        if(voice.allocation.session!=session || index>=KSMusicalState::VoiceCount || (i && index<=previous) ||
           size_t(voice.part)>=KSMusicalState::StorageSlotCount)return std::nullopt;
        previous=index;
        if(!voice.age || voice.part!=part)continue;
        if(!voice.patch || voice.context.unisonLayer>7)return std::nullopt;
        const auto& patch=*voice.patch;const auto& context=voice.context.voice;
        using ks::parameter::calculate;using ks::parameter::Formula;
        if(field==KSExpressionField::Pitch)for(unsigned lane=0;lane<3;++lane) {
            const auto source=7+8*lane;
            const auto octave=calculate(Formula::PitchOctave,{tables.pitch,patch[6],lane}).first;
            const auto coarse=calculate(Formula::PitchCoarse,{octave,patch[source]}).first;
            auto fine=uint16_t(uint8_t(context.globalFine+patch[source+1])+context.wheelFine-192);
            if(patch[0]&7)fine=uint16_t(calculate(Formula::PitchSpread,
                {fine,patch[0],voice.context.unisonLayer,voice.context.unisonSpread}).first);
            const auto tuned=calculate(Formula::PitchFine,{coarse,fine}).first;
            const auto word=calculate(Formula::PitchWheel,{tuned,patch[source+2],sources.wheelPitch}).first;
            if(!packet.appendWord(Endpoint::B,uint16_t(0x32+11*lane+101*index),word))return std::nullopt;
        } else for(unsigned lane=0;lane<4;++lane) {
            const auto bias=lane==3?51:10+8*lane;const auto depth=lane==3?53:34;
            const auto curve=calculate(Formula::ModulationIndex,{patch[bias],patch[depth],sources.modulation[0],
                patch[depth+1],sources.modulation[1],patch[depth+2],sources.modulation[2]});
            const auto word=calculate(Formula::CurveProduct,{curve.flags&1u,tables.modulationCurve[curve.first/2]}).first;
            constexpr uint16_t offsets[]{0x34,0x3f,0x4a,0x61};
            if(!packet.appendWord(Endpoint::B,uint16_t(offsets[lane]+101*index),word))return std::nullopt;
        }
    }
    if(field!=KSExpressionField::Pitch && !appendKsExpressionGlobals(packet,layout,part,sources))return std::nullopt;
    return packet;
}
}
