#include "ksParameterIngress.h"
#include <algorithm>

namespace novation::controller::command {
std::optional<KSParameterMapTables> decodeKsParameterMapTables(const uint8_t* image,size_t size)noexcept {
    constexpr size_t typeOffset=71586,fieldOffset=71842,packedOffset=72134;
    if(!image || size<packedOffset+16)return std::nullopt;
    KSParameterMapTables result;
    std::copy_n(image+typeOffset,256,result.type.begin());std::copy_n(image+fieldOffset,256,result.field.begin());
    for(size_t i=0;i<4;++i) {
        uint32_t pointer=0;for(unsigned n=0;n<4;++n)pointer=(pointer<<8)|image[packedOffset+4*i+n];
        if(pointer>=0xa00000 && pointer-0xa00000<size)pointer-=0xa00000;
        if(pointer>size || size-pointer<4)return std::nullopt;
        auto& descriptor=result.packed[i];descriptor.rightShift=image[pointer];descriptor.limit=image[pointer+1];descriptor.mask=image[pointer+2];
        const unsigned maximum=descriptor.rightShift<8?127u>>descriptor.rightShift:0;
        descriptor.pairCount=uint8_t((std::min(unsigned(descriptor.limit),maximum+1)+1)/2);
        if(size-pointer<4+2*descriptor.pairCount)return std::nullopt;
        for(size_t pair=0;pair<descriptor.pairCount;++pair)
            descriptor.pairs[pair]={image[pointer+4+2*pair],image[pointer+5+2*pair]};
    }
    return result;
}
void expandKsParameterFields(KSVoiceImagePatch& patch)noexcept {
    patch[209]=(patch[0]>>3)&3;patch[210]=patch[0]&7;patch[211]=(patch[0]>>5)&3;
    patch[212]=(patch[88]>>3)&1;patch[213]=(patch[88]>>4)&1;patch[214]=patch[88]&3;
    patch[215]=(patch[88]>>2)&1;patch[216]=(patch[88]>>5)&3;
    patch[217]=patch[121]&7;patch[218]=(patch[121]>>3)&3;
    patch[219]=patch[122]&3;patch[220]=(patch[122]>>2)&3;patch[221]=(patch[122]>>4)&3;
    patch[222]=patch[127]&3;patch[223]=(patch[127]>>2)&3;patch[224]=(patch[127]>>4)&3;patch[225]=(patch[127]>>6)&1;
    patch[226]=patch[126]&7;const auto nibble=(patch[126]>>3)&15;
    patch[227]=uint8_t(nibble<8?nibble:int(nibble)-16);
}
std::optional<KSParameterIngress::Plan> KSParameterIngress::prepare(ParameterDependency<KSStorageSlot> dependency,
    const KSVoiceImagePatch& patch,ControlChange input,bool inhibited,bool normalizedAlias)const noexcept {
    return prepareImpl(dependency,patch,input,inhibited,normalizedAlias,std::nullopt);
}
std::optional<KSParameterIngress::Plan> KSParameterIngress::prepareWithSelector(ParameterDependency<KSStorageSlot> dependency,
    const KSVoiceImagePatch& patch,ControlChange input,uint8_t externalSelector)const noexcept {
    if(input.controller!=6 || externalSelector<104 || externalSelector>107)return std::nullopt;
    return prepareImpl(dependency,patch,input,true,false,externalSelector);
}
std::optional<KSParameterIngress::Plan> KSParameterIngress::prepareImpl(ParameterDependency<KSStorageSlot> dependency,
    const KSVoiceImagePatch& patch,ControlChange input,bool inhibited,bool normalizedAlias,std::optional<uint8_t> externalSelector)const noexcept {
    if(dependency.session!=session_ || size_t(dependency.part)>=states_.size() || input.channel>=16 ||
       (!normalizedAlias && input.controller>=128) || input.value>=128 || states_[size_t(dependency.part)].revision==std::numeric_limits<uint64_t>::max())return std::nullopt;
    const auto& state=states_[size_t(dependency.part)];
    Plan result;result.dependency=dependency;result.revision=state.revision;result.before=result.after=patch;
    result.controller=input.controller;result.inputValue=result.value=input.value;result.selector=state.selector;
    if(!normalizedAlias && (input.controller==1 || input.controller==2 || input.controller==11 || input.controller==32 || input.controller==64 ||
       input.controller==120 || input.controller==121 || input.controller>=123)) {
        result.action=KSParameterIngressAction::ReservedControl;return result;
    }
    if(input.controller==98){result.selector=input.value;result.action=KSParameterIngressAction::Selector;return result;}
    const bool publicParameter=input.controller==6;
    const auto selector=externalSelector?*externalSelector:state.selector;
    if(publicParameter && selector>=128)return result;
    result.parameter=publicParameter?selector:input.controller;
    if(input.controller==122 && input.value!=63)result.value=input.value<63?0:127;
    bool packed=false;
    if(publicParameter && result.parameter>=104 && result.parameter<=107) {
        const auto& descriptor=tables_.packed[result.parameter-104];
        const unsigned selected=descriptor.rightShift<8?input.value>>descriptor.rightShift:0;
        if(selected>=descriptor.limit) {
            if(selected==descriptor.limit)result.action=KSParameterIngressAction::ReservedPackedBoundary;
            return result;
        }
        const unsigned pair=selected/2;if(pair>=descriptor.pairCount)return std::nullopt;
        const auto mapping=descriptor.pairs[pair];result.field=mapping.field;
        const auto mask=uint8_t(~descriptor.mask);const unsigned rotate=mapping.shift&7;
        const auto preserve=rotate?uint8_t((unsigned(mask)<<rotate)|(unsigned(mask)>>(8-rotate))):mask;
        const auto value=mapping.shift<8?uint8_t((input.value&descriptor.mask)<<mapping.shift):uint8_t(0);
        result.value=uint8_t(value|(patch[result.field]&preserve));packed=true;
    } else {
        result.field=tables_.field[uint8_t(result.parameter+(publicParameter?128:0))];
        const auto type=tables_.type[result.field];
        if(type>=126){result.action=KSParameterIngressAction::GlobalSetting;return result;}
        if(inhibited)return result;
    }
    (void)packed;result.action=KSParameterIngressAction::Patch;
    result.after[result.field]=result.value;expandKsParameterFields(result.after);
    result.edits[result.editCount++]={result.field,result.after[result.field]};
    for(unsigned field=209;field<=227;++field)if(field!=result.field && result.after[field]!=patch[field])
        result.edits[result.editCount++]={uint16_t(field),result.after[field]};
    return result;
}
}
