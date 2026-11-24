#include "silverPatch.h"
#include "../../ks/ksLib/ksState.h"
#include <cstdlib>
#include <cstring>
#include <string_view>
namespace silverK {
juce::String semanticId(const juce::String& id){if(id=="amp_attack" || id=="amp_decay" || id=="amp_sustain" || id=="amp_release")return id.substring(4);if(id=="delay_level")return "delay";if(id=="reverb_level")return "reverb";if(id=="chorus_level")return "chorus";return id;}

Model modelOf(const uint8_t* p,size_t size){if(!p || size<14 || p[0]!=0xf0 || p[1]!=0 || p[2]!=0x20 || p[3]!=0x29 || p[4]!=1)return Model::Unknown;switch(p[5]){case 0x40:return Model::AStation;case 0x41:return Model::KStation;case 0x42:if(p[9]==0x10 && p[10]==0)return Model::XStation;return p[9]==0x11 && p[10]==2?Model::KS:Model::Unknown;default:return Model::Unknown;}}
juce::String modelName(Model m){switch(m){case Model::KStation:return "K-Station";case Model::KS:return "KS";case Model::AStation:return "A-Station";case Model::XStation:return "X-Station";case Model::VStation:return "V-Station";default:return "Unknown";}}
bool drumMap(const uint8_t* p,size_t size){
 if(!p || size!=DrumMapSize)return false;
 const auto bank=p[11];if(bank<5 || bank>8)return false;
 for(unsigned n=0;n<50;++n){const auto* frame=p+n*270;
  if(!validPatch(frame,270) || modelOf(frame,270)!=Model::KS || frame[7]!=1 || frame[11]!=bank || frame[12]!=n)return false;
 }return true;
}
bool validPatch(const uint8_t* p,size_t size){if(size==DrumMapSize)return drumMap(p,size);const auto m=modelOf(p,size);const bool program=(m==Model::KStation || m==Model::AStation)?size==142:(m==Model::KS || m==Model::XStation) && size==270;const bool performance=m==Model::KS && size==1038;return (program || performance) && p[size-1]==0xf7 && std::all_of(p+1,p+size-1,[](uint8_t b){return b<128;}) && (program?(p[7]==0 || p[7]==1):(p[7]==2 || p[7]==3));}
unsigned ksBank(const uint8_t* p,size_t size){
 if(modelOf(p,size)!=Model::KS)return 0;
 if(size==1038 && (p[7]==2 || p[7]==3))return 9;
 if((size==270 || size==DrumMapSize) && p[7]==1 && p[11]>=1 && p[11]<=8 && p[12]<(p[11]<=4?100:50))return p[11];
 return 0;
}
bool ksDrum(const uint8_t* p,size_t size){const auto bank=ksBank(p,size);return bank>=5 && bank<=8;}
namespace {
using namespace novation::controller::command;
constexpr std::array<uint8_t,4> NativeMagic{'S','L','V','K'};
constexpr size_t NativeHeaderSize=14;
constexpr size_t NativeDrumSize=std::tuple_size_v<KSUserDrumBytes>+KSDrumGridEdit::Slots;
constexpr size_t NativeNameLimit=512;
}

NativePatchView nativePatch(PatchBytes bytes) {
 if(bytes.size()<NativeHeaderSize || !std::equal(NativeMagic.begin(),NativeMagic.end(),bytes.begin()) ||
    bytes[4]!=1 || bytes[5]>=49 || bytes[6]>15 || bytes[7]>1)return {};
 const auto dumpSize=juce::ByteOrder::littleEndianInt(bytes.data()+8);
 const auto nameSize=juce::ByteOrder::littleEndianShort(bytes.data()+12);
 if((dumpSize!=142 && dumpSize!=270 && dumpSize!=1038 && dumpSize!=DrumMapSize) ||
    !nameSize || nameSize>NativeNameLimit)return {};
 const auto mapMask=bytes[6];
 const auto drumSize=bytes[7]?NativeDrumSize:0;
 const auto total=NativeHeaderSize+dumpSize+nameSize+drumSize+size_t(std::bitset<4>(mapMask).count())*DrumMapSize;
 if(bytes.size()!=total)return {};
 NativePatchView result;
 result.drumProgram=bytes[5];
 result.dump=bytes.slice(NativeHeaderSize,dumpSize);
 result.name=bytes.slice(NativeHeaderSize+dumpSize,nameSize);
 size_t offset=NativeHeaderSize+dumpSize+nameSize;
 result.drumData=bytes.slice(offset,drumSize);
 offset+=drumSize;
 for(size_t i=0;i<result.drumMaps.size();++i)if(mapMask&(1u<<i)){
  result.drumMaps[i]=bytes.slice(offset,DrumMapSize);
  offset+=DrumMapSize;
 }
 return result;
}

bool hasDrumEdits(const KSUserDrumBank& bank) noexcept {
 if(bank.selected || bank.assignedKeysOnly || bank.selectedPattern || !bank.retrigger ||
    bank.switchAfter!=KSDrumGridEdit::DefaultSwitchStep)return true;
 static constexpr KSUserDrumPattern defaults;
 for(size_t i=0;i<bank.patterns.size();++i){
  const auto& p=bank.patterns[i];
  if(bank.keyRanges[i].assigned() || p.steps!=defaults.steps || p.notes!=defaults.notes ||
     p.velocities!=defaults.velocities || p.probabilities!=defaults.probabilities || p.ratchets!=defaults.ratchets)
   return true;
 }
 return false;
}

bool decodePatchDrums(const NativePatchView& patch,KSUserDrumPatterns& grids) {
 grids={};
 if(!patch.extended())return true;
 const auto bytes=patch.drumData.slice(0,NativeDrumSize-KSDrumGridEdit::Slots);
 grids=decodeKSUserDrums(bytes.data(),bytes.size());
 // The state decoder returns a default bank on malformed data.
 const auto encoded=encodeKSUserDrums(grids);
 if(!std::equal(encoded.begin(),encoded.end(),bytes.begin()))return false;
 const auto selections=patch.drumData.slice(bytes.size(),KSDrumGridEdit::Slots);
 const bool performance=patch.dump.size()==1038;
 bool hasEdits=false;
 for(size_t i=0;i<grids.size();++i){
  if(selections[i]>=KSDrumGridEdit::Patterns)return false;
  grids[i].selectedPattern=selections[i];
  if(!hasDrumEdits(grids[i]))continue;
  if((i!=0)!=performance)return false;
  if(!performance && patch.dump.size()!=DrumMapSize)return false;
  if(performance){
   const auto bank=patch.dump[13+(i-1)*256+187];
   if(bank<5 || bank>8)return false;
  }
  hasEdits=true;
 }
 return hasEdits;
}

bool validNativePatch(const NativePatchView& patch) {
 if(!patch || !validPatch(patch.dump.data(),patch.dump.size()))return false;
 const auto model=modelOf(patch.dump.data(),patch.dump.size());
 if(model!=Model::KS && model!=Model::KStation)return false;
 const auto* name=reinterpret_cast<const char*>(patch.name.data());
 if(!juce::CharPointer_UTF8::isValidString(name,int(patch.name.size())) ||
    std::find(patch.name.begin(),patch.name.end(),0)!=patch.name.end() ||
    juce::String::fromUTF8(name,int(patch.name.size())).trim().isEmpty())return false;
 if(patch.drumProgram && patch.dump.size()!=DrumMapSize)return false;
 if(patch.extended()){
  if(model!=Model::KS)return false;
  KSUserDrumPatterns grids;
  if(!decodePatchDrums(patch,grids))return false;
 }
 for(size_t i=0;i<patch.drumMaps.size();++i){
  const auto map=patch.drumMaps[i];
  if(!map.empty() && (patch.dump.size()!=1038 || !drumMap(map.data(),map.size()) || map[11]!=i+5))
   return false;
 }
 if(patch.dump.size()==1038)for(size_t part=0;part<4;++part){
  const auto bank=patch.dump[13+part*256+187];
  if(bank>=5 && bank<=8 && patch.drumMaps[bank-5].empty())return false;
 }
 return true;
}

PatchBytes patchDump(PatchBytes bytes) {
 // Ordinary dumps are by far the most common records in imported banks.
 if(!bytes.empty() && bytes.front()==NativeMagic.front())
  return nativePatch(bytes).dump;
 return bytes;
}

std::vector<uint8_t> encodeNativePatch(PatchBytes dump,const juce::String& name,
                                      const KSUserDrumPatterns* drums,
                                      const std::array<std::vector<uint8_t>,4>& maps,uint8_t drumProgram) {
 const auto utf8=name.trim();
 const auto nameSize=utf8.getNumBytesAsUTF8();
 if(!nameSize || nameSize>NativeNameLimit || dump.size()>DrumMapSize)return {};
 uint8_t mapMask=0;
 for(size_t i=0;i<maps.size();++i)if(!maps[i].empty())mapMask|=uint8_t(1u<<i);
 juce::MemoryOutputStream out;
 out.write(NativeMagic.data(),NativeMagic.size());
 out.writeByte(1);
 out.writeByte(char(drumProgram));
 out.writeByte(char(mapMask));
 out.writeByte(char(drums!=nullptr));
 out.writeInt(int(dump.size()));
 out.writeShort(short(nameSize));
 out.write(dump.data(),dump.size());
 out.write(utf8.toRawUTF8(),nameSize);
 if(drums){
  const auto bytes=encodeKSUserDrums(*drums);
  out.write(bytes.data(),bytes.size());
  for(const auto& bank:*drums)out.writeByte(char(bank.selectedPattern));
 }
 for(const auto& map:maps)if(!map.empty())out.write(map.data(),map.size());
 const auto* data=static_cast<const uint8_t*>(out.getData());
 const PatchBytes bytes(data,out.getDataSize());
 if(!validNativePatch(nativePatch(bytes)))return {};
 return std::vector<uint8_t>(bytes.begin(),bytes.end());
}

juce::String patchType(const uint8_t* p,size_t size){
 if(size==DrumMapSize && drumMap(p,size))return "Drum Map";
 if(ksDrum(p,size))return p[12]==49?"Drum Map FX":"Drum sound";
 return size==1038?"Performance":"Program";
}
std::vector<std::vector<uint8_t>> groupDrumMaps(const std::vector<std::vector<uint8_t>>& input){
 std::array<std::array<int,50>,4> slots{};for(auto& bank:slots)bank.fill(-1);
 std::array<bool,4> duplicate{};
 for(size_t i=0;i<input.size();++i){const auto& frame=input[i];if(frame.size()!=270 || !validPatch(frame) || !ksDrum(frame.data(),frame.size()))continue;
  auto& slot=slots[frame[11]-5][frame[12]];if(slot>=0)duplicate[frame[11]-5]=true;else slot=int(i);
 }
 std::vector<std::vector<uint8_t>> output;std::array<bool,4> emitted{};
 for(const auto& frame:input){if(frame.size()==270 && validPatch(frame) && ksDrum(frame.data(),frame.size())){
   const unsigned bank=frame[11]-5;const auto& indices=slots[bank];
   if(!duplicate[bank] && std::all_of(indices.begin(),indices.end(),[](int i){return i>=0;})){
    if(!emitted[bank]){std::vector<uint8_t> map;map.reserve(DrumMapSize);for(int i:indices)map.insert(map.end(),input[i].begin(),input[i].end());output.push_back(std::move(map));emitted[bank]=true;}continue;
   }
  }output.push_back(frame);
 }return output;
}
std::vector<ksPlugin::Control> kControls(){std::vector<ksPlugin::Control> result;for(const auto& s:kstation::controls::Catalog)result.push_back({s.id,s.label,s.group,s.section,s.mapping==kstation::controls::Mapping::CC?ksPlugin::Mapping::CC:ksPlugin::Mapping::NRPN,s.number,s.minimum,s.maximum,s.defaultValue,s.patchOffset,s.patchMask,s.patchShift,s.wireMask,s.wireShift,s.enumLabels,s.enumCount,s.displayOffset});return result;}
namespace {
void encode(const ksPlugin::Control& c,uint8_t* p,int v){if(c.patchOffset>=0)p[c.patchOffset]=uint8_t((p[c.patchOffset]&~c.patchMask)|((v<<c.patchShift)&c.patchMask));}
int readValue(const ksPlugin::Control& c,const uint8_t* p){return c.patchOffset<0?c.defaultValue:(p[c.patchOffset]&c.patchMask)>>c.patchShift;}

void keepModulatedMixerAudible(const uint8_t* src,uint8_t* dst,juce::StringArray& changes){
 constexpr unsigned firstLevel=37;
 static constexpr unsigned levelCount=5;
 const auto hasLevel=[](const uint8_t* levels){
  return std::any_of(levels,levels+levelCount,[](uint8_t level){return level!=0;});
 };
 if(hasLevel(dst+firstLevel))return;

 // KS/X-Station can open a mixer channel from zero using LFOs or the A/D
 // envelope. Estimate the positive mean in mixer units for K-Station, which
 // has no corresponding routes. Leave an authored static mix intact.
 const auto lfoMeanTimesFour=[](uint8_t depth,bool unipolar){
  const int amount=int(depth)-64;
  return unipolar?2*std::max(amount,0):std::abs(amount);
 };
 const bool lfo1Unipolar=src[133]!=0,lfo2Unipolar=src[134]!=0;
 const int levels[]{
  lfoMeanTimesFour(src[141],lfo1Unipolar)+lfoMeanTimesFour(src[171],lfo2Unipolar),
  lfoMeanTimesFour(src[142],lfo2Unipolar),
  2*std::max(0,int(src[143])-64),
  lfoMeanTimesFour(src[144],lfo1Unipolar),
  lfoMeanTimesFour(src[145],lfo1Unipolar)
 };
 for(unsigned i=0;i<levelCount;++i)dst[firstLevel+i]=uint8_t((levels[i]+2)/4);
 if(hasLevel(dst+firstLevel))changes.add("KS mixer modulation -> fixed levels (K-Station)");
}
}

std::vector<uint8_t> initPatch(Model model,bool performance,bool drums){
 if(model==Model::KStation){
  std::array<uint8_t,128> patch{};
  for(const auto& control:kControls())encode(control,patch.data(),control.defaultValue);
  std::vector<uint8_t> result{0xf0,0,0x20,0x29,1,0x41,0x7f,0,0,0x11,1,0,0};
  result.insert(result.end(),patch.begin(),patch.end());
  result.push_back(0xf7);
  return result;
 }
 ks::State state;
 auto& program=state.parts[0];
 for(const auto& control:ksPlugin::controls)encode(control,program.data(),control.defaultValue);
 const auto setName=[](ks::Program& record,size_t offset,std::string_view name){
  std::fill_n(record.begin()+offset,16,' ');
  std::copy(name.begin(),name.end(),record.begin()+offset);
 };
 setName(program,151,drums?"INIT DRUM":"INIT PROG");
 program[183]=127;
 program[187]=drums?5:1;
 program[190]=1;
 if(drums){
  std::vector<uint8_t> result;
  result.reserve(DrumMapSize);
  for(uint8_t i=0;i<50;++i){
   if(i==49)setName(program,151,"INIT DRUM MAP");
   result.insert(result.end(),{0xf0,0,0x20,0x29,1,0x42,0x7f,1,1,0x11,2,5,i});
   result.insert(result.end(),program.begin(),program.end());
   result.push_back(0xf7);
  }
  return result;
 }
 if(performance){
  state.performance=true;
  state.parts.fill(program);
  state.parts[0][189]=1;
  for(size_t i=0;i<state.parts.size();++i){
   state.parts[i][181]=uint8_t(i);
   setName(state.parts[i],192,"INIT PERF");
  }
 }
 return ks::stateMessage(state);
}

Conversion convertPatch(const uint8_t* input,size_t size,Model target,unsigned part){
 Conversion out;if(!validPatch(input,size) || size==DrumMapSize || (target==Model::KStation && ksDrum(input,size)) || (target!=Model::KS && target!=Model::KStation))return out;const auto source=modelOf(input,size);
 // A/K share the complete 128-byte sound format. X/KS share the 256-byte
 // synth format. Normalize the transport header, retaining the original library dump.
 if(source==Model::AStation || source==Model::XStation){
  std::vector<uint8_t> normalized(input,input+size);
  normalized[5]=source==Model::AStation?0x41:0x42;
  // Older A-Station dumps predate the shared filter-drive field. The K OS
  // importer clears it through version 08.07; KS clears it for every A dump.
  const bool clearDrive=source==Model::AStation && (target==Model::KS || ((input[9]<<8)|input[10])<=0x0807);
  if(clearDrive)normalized[13+43]=0;
  normalized[9]=0x11;normalized[10]=source==Model::AStation?1:2;
  auto result=convertPatch(normalized,target,part);
  if(clearDrive && input[13+43])result.changes.add("A-Station filter drive -> off (original firmware import)");
  if(source==Model::AStation && target==Model::KS)result.data[13+151]='A';
  return result;
 }
 if(source!=Model::KS && source!=Model::KStation)return out;
 if(source==target){out.data.assign(input,input+size);out.data[6]=0x7f;out.data[7]=size==1038?2:0;out.data[8]=out.data[11]=out.data[12]=0;return out;}
 const auto k=kControls();const std::vector<ksPlugin::Control> ks(std::begin(ksPlugin::controls),std::end(ksPlugin::controls));
 const auto& from=source==Model::KS?ks:k;const auto& to=target==Model::KS?ks:k;
 const uint8_t* src=input+13+(size==1038?std::min(part,3u)*256:0);
 out.data.assign(target==Model::KS?270:142,0);const uint8_t header[]{0xf0,0,0x20,0x29,1,uint8_t(target==Model::KS?0x42:0x41),0x7f,0,0,0x11,uint8_t(target==Model::KS?2:1),0,0};std::copy_n(header,13,out.data.begin());out.data.back()=0xf7;auto* dst=out.data.data()+13;
 for(const auto& c:to)encode(c,dst,c.defaultValue);
 for(const auto& c:to){if(c.patchOffset<0)continue;auto s=std::find_if(from.begin(),from.end(),[&](const auto& x){return semanticId(c.id)==semanticId(x.id);});if(s==from.end())continue;int v=readValue(*s,src);bool replaced=false;
  if(c.enumCount && s->enumCount){int mapped=-1;if(v<s->enumCount)for(int n=0;n<c.enumCount;++n)if(juce::String(c.enumLabels[n]).equalsIgnoreCase(s->enumLabels[v]))mapped=n;if(mapped>=0)v=mapped;else{v=c.defaultValue;replaced=true;}
   if(juce::String(c.id).startsWith("lfo") && juce::String(c.id).endsWith("_wave")){
     const int raw=readValue(*s,src);if(source==Model::KStation){constexpr int map[]{1,2,3,4};if(raw<4){v=map[raw];replaced=false;}}
     else if(raw>=1 && raw<=4){v=raw-1;replaced=false;}
   }}
  if(v<c.minimum || v>c.maximum){v=c.defaultValue;replaced=true;}
  if(target==Model::KStation && juce::String(c.id).startsWith("osc") && juce::String(c.id).endsWith("_wave") && readValue(*s,src)>3){v=1;replaced=true;}
  if(juce::String(c.id)=="voice_level"){
   const int raw=readValue(*s,src);v=target==Model::KS?std::max(0,raw-64):raw+64;replaced=false;
   if(target==Model::KS && raw<64)out.changes.addIfNotAlreadyThere("Program attenuation -> 0 dB (KS supports boost only)");
   if(v<c.minimum || v>c.maximum){v=c.defaultValue;replaced=true;}
  }
  // User policy: every KS filter is imported as LP 12, including an original LP 24.
  if(target==Model::KStation && juce::String(c.id)=="filter_slope"){v=0;out.changes.addIfNotAlreadyThere("Filter -> LP 12 dB/oct");}
  if(target==Model::KStation && juce::String(c.id)=="osc_key_phase"){const int phase=readValue(*s,src);if(phase<=60 && phase%4==0){v=phase/4;replaced=false;}else{v=c.defaultValue;replaced=true;}}
  encode(c,dst,v);if(replaced)out.changes.add(juce::String(c.section)+" / "+c.label+" -> "+(c.enumCount?juce::String(c.enumLabels[v]):juce::String(v+c.displayOffset)));
 }
 for(const auto& s:from)if(s.patchOffset>=0 && std::none_of(to.begin(),to.end(),[&](const auto& c){return semanticId(c.id)==semanticId(s.id);}) && readValue(s,src)!=s.defaultValue)out.changes.add(juce::String(s.section)+" / "+s.label+" -> target default");
 if(target==Model::KStation)keepModulatedMixerAudible(src,dst,out.changes);
 else{
 // Original KS OS K-Station importer preserves the common 128-byte block,
 // extracts packed waves, clears their old field and rebiases voice level.
 std::copy_n(src,128,dst);dst[5]=(src[5]>>6)&1;dst[79]&=0xf6;
 dst[125]=uint8_t(std::max(0,int(src[125])-64));
 dst[128]=src[5]&3;dst[129]=(src[5]>>2)&3;dst[130]=(src[5]>>4)&3;
 dst[146]=64;dst[147]=(src[80]&0x78)>>1;
 const char name[]="K-STATION PROG  ";std::copy_n(name,16,dst+151);
 dst[179]=127;dst[182]=0;dst[183]=127;dst[184]=dst[185]=64;}
 if(size==1038)out.changes.add("Performance -> selected Part "+juce::String(std::min(part,3u)+1)+" only");
 return out;
}
}
