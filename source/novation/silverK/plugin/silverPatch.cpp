#include "silverPatch.h"
#include <cstring>
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
  if(target==Model::KStation && juce::String(c.id).startsWith("osc") && juce::String(c.id).endsWith("_wave") && readValue(*s,src)>3){v=0;replaced=true;}
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
 if(target==Model::KS){
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
