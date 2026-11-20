#pragma once
#include "ksVoiceSelection.h"
namespace novation::controller::command {
struct KSMonoReplacement {uint8_t note,velocity;};
inline std::optional<KSMonoReplacement> ksMonoReplacement(const KSKeyState::Projection& keys,
    uint8_t released,uint8_t lastPlayed,KSVoiceMode mode)noexcept {
    if(released>=128 || released!=lastPlayed || (mode!=KSVoiceMode::Mono && mode!=KSVoiceMode::MonoGlide))return std::nullopt;
    int upper=-1,lower=-1;
    for(unsigned i=released+1;i<128;++i)if(keys.physical.test(i)){upper=int(i);break;}
    for(int i=int(released)-1;i>=0;--i)if(keys.physical.test(size_t(i))){lower=i;break;}
    if(upper<0 && lower<0)return std::nullopt;
    const auto selected=upper<0?lower:lower<0?upper:upper-int(released)>int(released)-lower?lower:upper;
    // KS priority takes velocity from the released input key's memory.
    return KSMonoReplacement{uint8_t(selected),keys.velocities[released]};
}
inline constexpr KSVoiceMode ksVoiceMode(uint8_t packed)noexcept{return KSVoiceMode((packed>>3)&3);}
inline constexpr uint8_t ksUnisonExtraVoices(uint8_t packed)noexcept{return packed&7;}
}
