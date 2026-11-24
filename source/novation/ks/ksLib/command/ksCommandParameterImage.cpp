#include "ksCommandParameterImage.h"
#include "../ksParameterDependencies.h"
#include "../ksParameterScaleMath.h"

namespace novation::controller::command {
namespace {
using ks::parameter::calculate;
using ks::parameter::Formula;
using ks::parameter::dependencySign8;
using ks::parameter::dependencySign16;
uint32_t coefficient(uint16_t input)noexcept {
    return uint32_t(ks::decodeParameterCoefficient(int16_t(dependencySign16(input))));
}
uint32_t velocityWord(uint8_t velocity,uint8_t depth)noexcept {
    depth=uint8_t(depth-64);
    if(depth&128){velocity^=127;depth=uint8_t(0-depth);}
    const auto product=uint16_t(dependencySign8(depth)*dependencySign8(uint8_t(velocity-127)));
    const auto scaled=uint16_t(uint32_t(product)<<2);
    return scaled?uint32_t(uint16_t(0x7f00+scaled))<<8:0x7fffff;
}
uint32_t pitch(const KSVoiceImagePatch& patch,const KSVoiceImageTables& tables,
               const KSParameterVoiceContext& source,unsigned lane)noexcept {
    const auto field=7+8*lane;const auto& voice=source.voice;
    const auto octave=calculate(Formula::PitchOctave,{tables.pitch,patch[6],lane}).first;
    const auto coarse=calculate(Formula::PitchCoarse,{octave,patch[field]}).first;
    auto fine=uint16_t(uint8_t(voice.globalFine+patch[field+1])+voice.wheelFine-192);
    if(patch[0]&7)fine=uint16_t(calculate(Formula::PitchSpread,{fine,patch[0],source.unisonLayer,source.unisonSpread}).first);
    const auto tuned=calculate(Formula::PitchFine,{coarse,fine}).first;
    return calculate(Formula::PitchWheel,{tuned,patch[field+2],voice.wheelPitch}).first;
}
}

std::optional<KSGlideWords> encodeKsGlideWords(const KSVoiceImagePatch& patch,const KSVoiceImageTables& tables,
                                             const KSParameterVoiceContext& source)noexcept {
    if(source.unisonLayer>7 || source.allocation>4)return std::nullopt;
    KSGlideWords result;result.distance=source.glideDistance;result.previousNote=source.voice.previousNote;
    result.words[0]=uint32_t(uint8_t((patch[168]?patch[168]:source.voice.transposedNote)-60))<<16;
    const auto delta=uint8_t(patch[4]-64);
    if(delta) {
        result.changedDistanceAndPrevious=true;
        result.distance=delta&128?uint8_t(0-delta):delta;
        result.previousNote=uint8_t(source.voice.transposedNote+delta);
    }
    const auto mode=(patch[0]>>3)&3;
    const auto time=mode==1 && (source.allocation==1 || source.allocation==2)?0:patch[3];
    if(!time){result.words[1]=0;result.words[2]=0x7fffff;}
    else {
        const auto inverted=uint8_t(time^127);
        const auto input=uint16_t(unsigned(inverted)*93+(inverted>118?unsigned(inverted-118)<<8:0)+tables.glide);
        const auto value=coefficient(input);
        if(patch[5] && result.distance) {
            const auto distance=uint32_t(result.distance)<<8;
            const auto scaled=uint32_t(uint16_t(value>>16))*distance+(uint32_t(uint16_t(value))*distance>>16);
            result.words[1]=(scaled<512?512:scaled)>>8;result.words[2]=0;
        } else {result.words[1]=value>>13;result.words[2]=value>>8;}
    }
    result.words[3]=uint32_t(uint8_t(result.previousNote-60))<<16;
    return result;
}

std::optional<KSParameterImageResult> encodeKsParameterImage(const KSVoiceImagePatch& patch,const KSVoiceImageTables& tables,
    const KSParameterVoiceContext& voice,const KSCutoffWord& cutoff)noexcept {
    const auto glide=encodeKsGlideWords(patch,tables,voice);if(!glide)return std::nullopt;
    // The verified common image compiler has a deliberately bounded P05
    // precondition. Reuse its common words, then supply the newly verified
    // unison and glide groups from the original complete tuple.
    auto neutral=patch;neutral[0]&=0xf8;neutral[3]=0;neutral[4]=64;
    auto common=encodeKsVoiceImage(neutral,tables,voice.voice,cutoff);if(!common)return std::nullopt;
    for(unsigned lane=0;lane<3;++lane)common->image[0x32+11*lane-0x2c]=pitch(patch,tables,voice,lane);
    constexpr unsigned velocityAddresses[]{0x6b,0x6f,0x77,0x83,0x8b};
    constexpr unsigned velocityFields[]{58,61,66,169,170};
    for(unsigned i=0;i<5;++i)common->image[velocityAddresses[i]-0x2c]=velocityWord(cutoff.observation.inputs.velocity,patch[velocityFields[i]]);
    common->image[0x74-0x2c]=coefficient(uint16_t(calculate(Formula::ClockInput,{patch[125],patch[0]}).first))>>8;
    for(unsigned i=0;i<4;++i)common->image[0x8c+i-0x2c]=glide->words[i];
    return KSParameterImageResult{*common,*glide};
}

uint32_t encodeKsVoiceGate(const KSVoiceImagePatch& patch,bool fullGate)noexcept {
    const auto flags=uint8_t(fullGate?(patch[79]&0x3f)|((patch[78]&3)<<6):patch[79]|0xc0);
    const auto middle=uint8_t(fullGate?0:patch[147]<<1);
    const auto low=uint8_t((fullGate?(patch[80]&7)|0xb8:0xbf)|(patch[4]!=64?0x40:0));
    return (uint32_t(flags)<<16)|(uint32_t(middle)<<8)|low;
}

std::optional<KSResumeImageResult> encodeKsResumePacket(VoiceSlot slot,const KSVoiceImagePatch& patch,
    const KSVoiceImageTables& tables,const KSParameterVoiceContext& voice,uint8_t velocity)noexcept {
    if(size_t(slot)>=KSMusicalState::VoiceCount || velocity>=128 || voice.allocation<2 || voice.allocation>4)return std::nullopt;
    const auto glide=encodeKsGlideWords(patch,tables,voice);if(!glide)return std::nullopt;
    KSResumeImageResult result{{},*glide};
    constexpr uint16_t addresses[]{0x6b,0x6f,0x77};constexpr unsigned fields[]{58,61,66};
    for(unsigned i=0;i<3;++i)if(!result.packet.appendWord(Endpoint::B,uint16_t(addresses[i]+101*size_t(slot)),velocityWord(velocity,patch[fields[i]])))return std::nullopt;
    for(unsigned i=0;i<4;++i)if(!result.packet.appendWord(Endpoint::B,uint16_t(0x8c+i+101*size_t(slot)),glide->words[i]))return std::nullopt;
    if(!result.packet.appendWord(Endpoint::B,uint16_t(0x2b+101*size_t(slot)),encodeKsVoiceGate(patch,voice.allocation>2)))return std::nullopt;
    return result;
}
}
