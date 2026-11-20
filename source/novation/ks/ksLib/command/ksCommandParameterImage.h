#pragma once

#include "ksCommandVoiceImage.h"
#include "ksRefreshPacket.h"

namespace novation::controller::command {
// Complete numerical observations for a voice image or legato update. The
// unison layer and selected spread belong to this actual allocation.
struct KSParameterVoiceContext {
    KSVoiceImageContext voice{};
    uint8_t unisonLayer=0,unisonSpread=0,glideDistance=0,allocation=0;
};
struct KSGlideWords {
    std::array<uint32_t,4> words{}; // relative addresses8c..8f
    uint8_t distance=0,previousNote=0;
    bool changedDistanceAndPrevious=false;
};
std::optional<KSGlideWords> encodeKsGlideWords(const KSVoiceImagePatch&,const KSVoiceImageTables&,
                                             const KSParameterVoiceContext&)noexcept;
struct KSParameterImageResult {
    KSVoiceImageResult image;
    KSGlideWords glide;
};
std::optional<KSParameterImageResult> encodeKsParameterImage(const KSVoiceImagePatch&,const KSVoiceImageTables&,
    const KSParameterVoiceContext&,const KSCutoffWord&)noexcept;
uint32_t encodeKsVoiceGate(const KSVoiceImagePatch&,bool fullGate)noexcept;
struct KSResumeImageResult { KSRefreshPacket packet; KSGlideWords glide; };
// Three velocity-dependent words, complete relative glide and the observed
// allocation's gate language. Does not generate a replacement100-word image.
std::optional<KSResumeImageResult> encodeKsResumePacket(VoiceSlot,const KSVoiceImagePatch&,
    const KSVoiceImageTables&,const KSParameterVoiceContext&,uint8_t velocity)noexcept;
}
