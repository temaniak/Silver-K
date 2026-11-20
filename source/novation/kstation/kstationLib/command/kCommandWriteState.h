#pragma once
#include "kMusicalState.h"

namespace novation::controller::command::kCommandDetail {
// A value capture of every physical slot, including empty-slot generations.
// This guards a complete write event against allocation/retirement and
// observation ABA without retaining a legacy cursor or live patch binding.
struct WriteSource {
    ParameterDependency<KPart> dependency{};
    KMusicalState::Voices::Projection voices{};
    std::array<uint64_t,KMusicalState::VoiceCount> generations{};
    std::array<uint16_t,KMusicalState::VoiceCount> countdown{};
    bool capture(const KMusicalState& model,uint64_t rate)noexcept {
        const auto source=model.dependency(rate);if(!source)return false;dependency=*source;
        for(size_t i=0;i<voices.size();++i) {
            const auto generation=model.voiceGeneration(VoiceSlot(i));if(!generation)return false;
            generations[i]=*generation;voices[i]=model.voice(VoiceSlot(i));countdown[i]=0;
            if(voices[i]) {const auto feedback=model.feedback(voices[i]->allocation);if(!feedback)return false;countdown[i]=feedback->countdown;}
        }
        return true;
    }
    bool current(const KMusicalState& model,uint64_t rate)const noexcept {
        const auto source=model.dependency(rate);if(!source || !(*source==dependency))return false;
        for(size_t i=0;i<voices.size();++i) {
            const auto generation=model.voiceGeneration(VoiceSlot(i));const auto now=model.voice(VoiceSlot(i));const auto& before=voices[i];
            if(!generation || *generation!=generations[i] || bool(now)!=bool(before))return false;
            if(now) {
                const auto feedback=model.feedback(now->allocation);
                if(!(now->allocation==before->allocation) || now->part!=before->part || now->note!=before->note ||
                   now->velocity!=before->velocity || now->feedbackAge!=before->feedbackAge ||
                   now->observationRevision!=before->observationRevision || !feedback || feedback->countdown!=countdown[i])return false;
            }
        }
        return true;
    }
};
}
