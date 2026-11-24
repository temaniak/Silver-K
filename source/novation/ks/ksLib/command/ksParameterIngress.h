#pragma once

#include "ksVoiceParameterBank.h"

namespace novation::controller::command {
struct KSParameterPackedDescriptor {
    struct Pair {uint8_t shift=0,field=0;};
    uint8_t rightShift=0,limit=0,mask=0,pairCount=0;
    std::array<Pair,64> pairs{};
};
struct KSParameterMapTables {
    std::array<uint8_t,256> field{},type{};
    std::array<KSParameterPackedDescriptor,4> packed{};
};
// Cold parsing of authenticated decoded KS2.1.02 ROM tables; runtime does not
// retain their addresses, descriptor pointers or a legacy data-map owner.
std::optional<KSParameterMapTables> decodeKsParameterMapTables(const uint8_t*,size_t)noexcept;
void expandKsParameterFields(KSVoiceImagePatch&)noexcept;

enum class KSParameterIngressAction { Patch,GlobalSetting,Selector,Ignore,ReservedControl,ReservedPackedBoundary };
class KSParameterIngress final {
public:
    struct Plan {
        ParameterDependency<KSStorageSlot> dependency{};
        uint64_t revision=0;
        KSParameterIngressAction action=KSParameterIngressAction::Ignore;
        uint8_t controller=0,inputValue=0,parameter=0,field=0,value=0,selector=255;
        KSVoiceImagePatch before{},after{};
        std::array<KSVoiceParameterEdit,20> edits{};
        size_t editCount=0;
    };
    explicit KSParameterIngress(Session session,const KSParameterMapTables& tables)noexcept:session_(session),tables_(tables){}
    KSParameterIngress(const KSParameterIngress&)=delete;
    KSParameterIngress& operator=(const KSParameterIngress&)=delete;
    bool prepareSelector(KSStorageSlot part,uint8_t value)noexcept {
        if(size_t(part)>=states_.size() || states_[size_t(part)].revision)return false;
        states_[size_t(part)].selector=value;return true;
    }
    // A CC11 alias is an internal, already decoded control byte. It is not
    // re-entered through the external MIDI validator or remapped a second time.
    std::optional<Plan> prepare(ParameterDependency<KSStorageSlot>,const KSVoiceImagePatch&,ControlChange,bool editingInhibited=false,bool normalizedAlias=false)const noexcept;
    // A global-channel packed control uses that channel's selector while the
    // explicit editor target retains its own selector and source revision.
    // Only public packed104..107 through CC6 are admitted by this entry point.
    std::optional<Plan> prepareWithSelector(ParameterDependency<KSStorageSlot>,const KSVoiceImagePatch&,
        ControlChange,uint8_t externalSelector)const noexcept;
    bool current(const Plan& plan,ParameterDependency<KSStorageSlot> dependency,const KSVoiceImagePatch& patch)const noexcept {
        return dependency.session==session_ && size_t(dependency.part)<states_.size() && plan.dependency==dependency &&
            plan.before==patch && states_[size_t(dependency.part)].revision==plan.revision &&
            plan.action!=KSParameterIngressAction::ReservedControl && plan.action!=KSParameterIngressAction::ReservedPackedBoundary &&
            plan.revision!=std::numeric_limits<uint64_t>::max();
    }
    bool commit(const Plan& plan,ParameterDependency<KSStorageSlot> dependency,const KSVoiceImagePatch& patch)noexcept {
        if(!current(plan,dependency,patch))return false;
        auto& state=states_[size_t(dependency.part)];state.selector=plan.selector;++state.revision;return true;
    }
    std::optional<uint8_t> selector(KSStorageSlot part)const noexcept {
        return size_t(part)<states_.size()?std::optional{states_[size_t(part)].selector}:std::nullopt;
    }
private:
    std::optional<Plan> prepareImpl(ParameterDependency<KSStorageSlot>,const KSVoiceImagePatch&,ControlChange,
        bool editingInhibited,bool normalizedAlias,std::optional<uint8_t> externalSelector)const noexcept;
    struct State {uint64_t revision=0;uint8_t selector=255;};
    Session session_;const KSParameterMapTables& tables_;
    std::array<State,KSMusicalState::StorageSlotCount> states_{};
};
}
