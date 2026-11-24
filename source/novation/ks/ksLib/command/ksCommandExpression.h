#pragma once

#include "ksMusicalState.h"
#include "ksRefreshPacket.h"
#include "../../../controller/command/controllerEvent.h"
#include <exception>
#include <utility>

namespace novation::controller::command {

enum class KSExpressionField { Pitch, Wheel, Pressure, Breath };
enum class KSExpressionDisposition { Apply, ForwardControl, Ignore, Unhandled, Invalid };
struct KSExpressionValues {
    uint16_t wheelPitch=256;
    // The firmware parameter compiler consumes wheel, pressure, breath in
    // this order, not MIDI controller-number order.
    std::array<uint8_t,3> modulation{};
};
struct KSExpressionUpdate {
    KSExpressionDisposition disposition=KSExpressionDisposition::Unhandled;
    KSExpressionField field=KSExpressionField::Pitch;
    uint16_t value=0;
    bool voiceRefresh=false,globalRefresh=false,remapped=false;
    ControlChange forwarded{};
};

// Dynamic source levels are six direct writes, not part of the static FX
// image. Preserve the verified B-then-A order and independent address slots.
inline bool appendKsExpressionGlobals(KSRefreshPacket& packet,KSLayout layout,KSStorageSlot part,
                                      const KSExpressionValues& values)noexcept {
    const auto index=size_t(part);
    if(index>=KSMusicalState::StorageSlotCount ||
       (layout!=KSLayout::Single && layout!=KSLayout::Performance) ||
       (layout==KSLayout::Single && part!=KSStorageSlot::Single) || !packet.canAppend(6,6))return false;
    for(const auto byte:values.modulation)if(byte>=128)return false;
    const auto base=layout==KSLayout::Single?size_t(0):size_t(1);
    const auto offset=3*(index<base?0:index-base);
    for(const auto endpoint:{Endpoint::B,Endpoint::A})for(size_t i=0;i<3;++i) {
        const auto level=values.modulation[i];
        const auto word=(uint32_t(level)<<16)|(uint32_t(uint8_t(level*2))<<8);
        if(!packet.appendWord(endpoint,uint16_t(0x19+offset+i),word))std::terminate();
    }
    return true;
}

inline KSExpressionUpdate decodeKsExpression(const EventPayload& event,uint8_t expressionController,
                                               bool refreshSuppressed=false)noexcept {
    KSExpressionUpdate result;
    // Authenticated KS2.1.02 channel descriptor A0 is the no-op capability.
    // Poly pressure is consumed without changing any modulation source.
    if(const auto* pressure=std::get_if<PolyPressure>(&event)) {
        result.disposition=pressure->channel<16 && pressure->note<128 && pressure->pressure<128?
            KSExpressionDisposition::Ignore:KSExpressionDisposition::Invalid;return result;
    }
    if(const auto* pitch=std::get_if<PitchBend>(&event)) {
        if(pitch->channel>=16 || pitch->value>=16384){result.disposition=KSExpressionDisposition::Invalid;return result;}
        const auto coarse=pitch->value>>6;
        result.disposition=KSExpressionDisposition::Apply;result.field=KSExpressionField::Pitch;
        result.value=uint16_t(coarse==255?512:coarse*2);result.voiceRefresh=!refreshSuppressed;return result;
    }
    if(const auto* pressure=std::get_if<ChannelPressure>(&event)) {
        if(pressure->channel>=16 || pressure->pressure>=128){result.disposition=KSExpressionDisposition::Invalid;return result;}
        result.disposition=KSExpressionDisposition::Apply;result.field=KSExpressionField::Pressure;
        result.value=uint16_t((2u*pressure->pressure*pressure->pressure)>>8);
        result.voiceRefresh=result.globalRefresh=!refreshSuppressed;return result;
    }
    if(const auto* control=std::get_if<ControlChange>(&event)) {
        if(control->channel>=16 || control->controller>=128 || control->value>=128){result.disposition=KSExpressionDisposition::Invalid;return result;}
        result.forwarded=*control;
        if(result.forwarded.controller==11){result.forwarded.controller=expressionController;result.remapped=true;}
        if(result.forwarded.controller==1 || result.forwarded.controller==2) {
            result.disposition=KSExpressionDisposition::Apply;
            result.field=result.forwarded.controller==1?KSExpressionField::Wheel:KSExpressionField::Breath;
            result.value=control->value;result.voiceRefresh=result.globalRefresh=!refreshSuppressed;
        } else result.disposition=KSExpressionDisposition::ForwardControl;
    }
    return result;
}

enum class KSExpressionApply { Applied, ForwardControl, Ignored, Unhandled, Invalid, Busy, RevisionExhausted };
struct KSExpressionProjection {
    KSExpressionValues values{};
    uint8_t expressionController=1;
    bool refreshSuppressed=false;
    uint64_t revision=0;
    bool voiceDirty=false,globalDirty=false;
};

// An owned submodel. Embedding this alongside keys/voices in the main musical
// model provides one authoritative set of source values and dirty ownership.
// No DSP packet or physical observation is inferred by this component.
class KSCommandExpression final {
public:
    explicit KSCommandExpression(Session session)noexcept:session_(session){}
    Session session()const noexcept{return session_;}
    bool prepare(KSStorageSlot part,KSExpressionValues values,uint8_t expressionController,bool suppressed)noexcept {
        const auto index=size_t(part);
        if(index>=parts_.size() || initialized_.test(index) || values.wheelPitch>512)return false;
        for(const auto byte:values.modulation)if(byte>=128)return false;
        parts_[index]={values,expressionController,suppressed,0,false,false};initialized_.set(index);return true;
    }
    const KSExpressionProjection* part(KSStorageSlot part)const noexcept {
        const auto index=size_t(part);return index<parts_.size() && initialized_.test(index)?&parts_[index]:nullptr;
    }
    KSExpressionApply apply(KSMusicalState& model,KSStorageSlot part,const EventPayload& event,
                            KSExpressionUpdate& decoded)noexcept {
        const auto index=size_t(part);
        if(model.session()!=session_ || !model.keys(part) || !this->part(part))return KSExpressionApply::Invalid;
        const auto& before=parts_[index];
        decoded=decodeKsExpression(event,before.expressionController,before.refreshSuppressed);
        if(decoded.disposition==KSExpressionDisposition::Invalid)return KSExpressionApply::Invalid;
        if(decoded.disposition==KSExpressionDisposition::ForwardControl)return KSExpressionApply::ForwardControl;
        if(decoded.disposition==KSExpressionDisposition::Ignore)return KSExpressionApply::Ignored;
        if(decoded.disposition==KSExpressionDisposition::Unhandled)return KSExpressionApply::Unhandled;
        // One expression event must retire before the next may replace its
        // pitch/modulation obligations, including before first DSP admission.
        if(model.voiceOperationBusy() || before.voiceDirty || before.globalDirty)return KSExpressionApply::Busy;
        if(before.revision==std::numeric_limits<uint64_t>::max() || !model.canTouchParameters(part))
            return KSExpressionApply::RevisionExhausted;
        auto& target=parts_[index];
        if(decoded.field==KSExpressionField::Pitch)target.values.wheelPitch=decoded.value;
        else target.values.modulation[decoded.field==KSExpressionField::Wheel?0:decoded.field==KSExpressionField::Pressure?1:2]=uint8_t(decoded.value);
        target.voiceDirty|=decoded.voiceRefresh;target.globalDirty|=decoded.globalRefresh;++target.revision;
        // Partial expression work owns its own dirty obligations. A general
        // deferred full-image bit would add unverified DSP side effects.
        if(!model.touchParameters(part,false))std::terminate();
        return KSExpressionApply::Applied;
    }
    bool setPolicy(KSStorageSlot part,uint8_t expressionController,bool refreshSuppressed)noexcept {
        const auto index=size_t(part);if(!this->part(part) || parts_[index].revision==std::numeric_limits<uint64_t>::max())return false;
        auto& target=parts_[index];target.expressionController=expressionController;target.refreshSuppressed=refreshSuppressed;++target.revision;return true;
    }
    // Taking work transfers the complete serialized expression obligation.
    std::optional<std::pair<bool,bool>> takeDirty(KSStorageSlot part,uint64_t revision)noexcept {
        const auto index=size_t(part);if(!this->part(part) || parts_[index].revision!=revision)return std::nullopt;
        auto& target=parts_[index];const auto result=std::pair{target.voiceDirty,target.globalDirty};
        target.voiceDirty=target.globalDirty=false;return result;
    }
private:
    Session session_;
    std::array<KSExpressionProjection,KSMusicalState::StorageSlotCount> parts_{};
    std::bitset<KSMusicalState::StorageSlotCount> initialized_{};
};
}
