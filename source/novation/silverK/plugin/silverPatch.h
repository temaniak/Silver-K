#pragma once
#include "../../ks/ksJucePlugin/ksControls.h"
#include "../../kstation/kstationJucePlugin/kstationControls.h"
#include "../../ks/ksLib/command/ksUserDrumPattern.h"
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

inline constexpr auto NativePatchExtension = "silver-k";

class PatchBytes {
public:
 PatchBytes()=default;
 PatchBytes(const uint8_t* data,size_t size):bytes(data),length(size){}
 template<class Alloc> PatchBytes(const std::vector<uint8_t,Alloc>& data):PatchBytes(data.data(),data.size()){}
 const uint8_t* data()const noexcept{return bytes;}
 size_t size()const noexcept{return length;}
 bool empty()const noexcept{return length==0;}
 const uint8_t* begin()const noexcept{return bytes;}
 const uint8_t* end()const noexcept{return length?bytes+length:bytes;}
 uint8_t front()const noexcept{return *bytes;}
 uint8_t operator[](size_t index)const noexcept{return bytes[index];}
 PatchBytes slice(size_t offset,size_t count)const noexcept{return {bytes+offset,count};}
private:
 const uint8_t* bytes=nullptr;
 size_t length=0;
};

// Disks and the patch cache retain the entire versioned record. Browsing
// only needs its header, so the drum data is decoded on preset activation.
struct NativePatchView {
 PatchBytes dump,name,drumData;
 std::array<PatchBytes,4> drumMaps;
 uint8_t drumProgram=0;
 bool extended() const noexcept { return !drumData.empty(); }
 explicit operator bool() const noexcept { return !dump.empty(); }
};
NativePatchView nativePatch(PatchBytes);
bool validNativePatch(const NativePatchView&);
bool decodePatchDrums(const NativePatchView&,novation::controller::command::KSUserDrumPatterns&);
bool hasDrumEdits(const novation::controller::command::KSUserDrumBank&) noexcept;
PatchBytes patchDump(PatchBytes);
std::vector<uint8_t> encodeNativePatch(PatchBytes dump,const juce::String& name,
                                      const novation::controller::command::KSUserDrumPatterns* drums=nullptr,
                                      const std::array<std::vector<uint8_t>,4>& maps={},uint8_t drumProgram=0);
std::vector<uint8_t> initPatch(Model,bool performance=false,bool drums=false);
}
