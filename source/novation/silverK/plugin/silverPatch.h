#pragma once
#include "../../ks/ksJucePlugin/ksControls.h"
#include "../../kstation/kstationJucePlugin/kstationControls.h"
#include <juce_core/juce_core.h>
namespace silverK {
enum class Model {Unknown,KStation,KS,AStation,XStation,VStation};
juce::String semanticId(const juce::String&);
Model modelOf(const uint8_t*,size_t);
template<class Alloc> Model modelOf(const std::vector<uint8_t,Alloc>& p){return modelOf(p.data(),p.size());}
juce::String modelName(Model);
bool validPatch(const uint8_t*,size_t);
template<class Alloc> bool validPatch(const std::vector<uint8_t,Alloc>& p){return validPatch(p.data(),p.size());}
// A complete KS map retains all 50 original Program dumps, including shared FX.
inline constexpr size_t DrumMapSize=50*270;
bool drumMap(const uint8_t*,size_t);
unsigned ksBank(const uint8_t*,size_t); // 1..8 programs/maps, 9 performances, 0 edit buffer/other model
bool ksDrum(const uint8_t*,size_t);
juce::String patchType(const uint8_t*,size_t);
std::vector<std::vector<uint8_t>> groupDrumMaps(const std::vector<std::vector<uint8_t>>&);
struct Conversion {std::vector<uint8_t> data;juce::StringArray changes;};
Conversion convertPatch(const uint8_t*,size_t,Model target,unsigned part=0);
template<class Alloc> Conversion convertPatch(const std::vector<uint8_t,Alloc>& p,Model target,unsigned part=0){return convertPatch(p.data(),p.size(),target,part);}
std::vector<ksPlugin::Control> kControls();
}
