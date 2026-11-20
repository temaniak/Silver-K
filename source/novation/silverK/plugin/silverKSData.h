#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace silverK {
bool readKSDataFiles(const std::vector<juce::File>&, std::vector<uint8_t>&, juce::String& error);
}
