#include "silverProcessor.h"
namespace silverK {
using namespace ksPlugin;
namespace {
constexpr uint8_t prefix[]{0xf0,0,0x20,0x29,1,0x42};
juce::String nameAt(const uint8_t* patch,unsigned offset){juce::String name;for(unsigned n=offset;n<offset+16;++n){const auto c=patch[n];name+=juce::String::charToString(c>=32 && c<127?c:' ');}return name.trim();}
juce::String patchName(const uint8_t* patch){return nameAt(patch,151);}
bool validDump(const uint8_t* p,size_t n){return (n==270 || n==1038) && std::equal(std::begin(prefix),std::end(prefix),p) && (n==270?(p[7]==0 || p[7]==1):(p[7]==2 || p[7]==3)) && p[n-1]==0xf7 && std::all_of(p+1,p+n-1,[](uint8_t v){return v<128;});}
}
void Processor::buildLibrary(){
 library.clear();performanceLibrary.clear();libraryBaseCount=performanceBaseCount=0;if(flash.size()!=0x80000)return;
 for(unsigned n=0;n<600;++n){const auto* p=flash.data()+0x20000+n*256;if(std::any_of(p,p+256,[](uint8_t v){return v>=128;}))continue;
  Preset preset;preset.bank=uint8_t(n<400?1+n/100:5+(n-400)/50);preset.number=uint8_t(n<400?n%100:(n-400)%50);
  preset.name=juce::String(preset.bank)+juce::String(preset.number).paddedLeft('0',2)+"  "+patchName(p);
  preset.data={0xf0,0,0x20,0x29,1,0x42,0x7f,0,0,0x11,2,0,0};std::copy_n(p,256,preset.data.begin()+13);preset.data.back()=0xf7;library.push_back(std::move(preset));
 }
 for(unsigned n=0;n<100;++n){const auto* p=flash.data()+0x45800+n*1024;if(std::any_of(p,p+1024,[](uint8_t v){return v>=128;}))continue;
  Performance preset;preset.name=juce::String(n).paddedLeft('0',2)+"  "+nameAt(p,192);
  preset.data={0xf0,0,0x20,0x29,1,0x42,0x7f,2,0,0x11,2,1,uint8_t(n)};std::copy_n(p,1024,preset.data.begin()+13);preset.data.back()=0xf7;performanceLibrary.push_back(std::move(preset));
 }
 libraryBaseCount=library.size();performanceBaseCount=performanceLibrary.size();
}
std::vector<Processor::Preset> Processor::presets()const{std::lock_guard guard(controlMutex);return library;}
std::vector<Processor::Performance> Processor::performances()const{std::lock_guard guard(controlMutex);return performanceLibrary;}
void Processor::publishFactoryPresets(){
 if(!firmwareReady() || isKStation())return;
 std::vector<std::vector<uint8_t>> factory;
 for(const auto& p:presets()){factory.emplace_back(p.data.begin(),p.data.end());auto& bytes=factory.back();bytes[7]=1;bytes[11]=p.bank;bytes[12]=p.number;}
 for(const auto& p:performances())factory.emplace_back(p.data.begin(),p.data.end());
 if(!factory.empty())patchLibrary.addFactory(factory,"KS Factory");
}
bool Processor::queueUiMidi(const uint8_t* data,size_t n){std::lock_guard guard(uiMidiMutex);return uiMidi.push(data,n);}
bool Processor::sendShortMidi(uint8_t status,uint8_t a,uint8_t b){const uint8_t bytes[]{status,a,b};return queueUiMidi(bytes,(status&0xf0)==0xc0?2:3);}
void Processor::resetPending(){for(auto& v:pending)v.store(-1);for(auto& v:awaiting)v.store(-1);resetWire.store(true);}
juce::String Processor::currentName()const{if(isFirmwareLoading())return "Loading firmware...";std::lock_guard guard(controlMutex);if(ownedK)return currentPatch.isEmpty()?"K-Station program":currentPatch;if(!ownedDevice)return {};const auto state=ownedDevice->fullSnapshot();return state.performance?nameAt(state.parts[0].data(),192)+" / Part "+juce::String(currentPart()+1)+": "+patchName(state.parts[currentPart()].data()):patchName(state.parts[0].data());}
bool Processor::queueState(const ks::State& state,bool selectMode,juce::String& error){
 std::vector<uint8_t> bytes;
 if(selectMode){for(unsigned ch=0;ch<16;++ch)for(uint8_t cc:{uint8_t(64),uint8_t(123),uint8_t(120)})bytes.insert(bytes.end(),{uint8_t(0xb0|ch),cc,0});
  auto select=ks::singleSelection(state.parts[0].data(),controlChannel.load());if(state.performance){select[2]=65;select[4]=0;}bytes.insert(bytes.end(),select.begin(),select.end());}
 const auto dump=ks::stateMessage(state);bytes.insert(bytes.end(),dump.begin(),dump.end());
 if(!state.performance){const auto metadata=ks::singleMetadata(state.parts[0].data(),controlChannel.load());bytes.insert(bytes.end(),metadata.begin(),metadata.end());}
 if(!queueUiMidi(bytes.data(),bytes.size())){error="Preset queue is full";return false;}resetPending();return true;
}
std::array<juce::String,4> Processor::partNames()const{
 std::array<juce::String,4> names{};std::lock_guard guard(controlMutex);
 if(isFirmwareLoading())return names;
 if(ownedDevice){const auto state=ownedDevice->fullSnapshot();for(unsigned n=0;n<4;++n)names[n]=patchName(state.parts[n].data());}
 else if(ownedK)names[0]=currentPatch;
 return names;
}
bool Processor::setPerformanceMode(bool enabled,juce::String& error){
 std::lock_guard guard(controlMutex);if(!ownedDevice){error="Load firmware, wave bank and factory data first";return false;}
 auto state=ownedDevice->fullSnapshot();if(state.performance==enabled)return true;
 if(enabled){for(unsigned part=1;part<ks::PartCount;++part)state.parts[part]=state.parts[0];
  for(unsigned part=0;part<ks::PartCount;++part){auto& p=state.parts[part];p[179]=127;p[180]=0;p[181]=uint8_t(part);p[182]=0;p[183]=127;p[184]=p[185]=64;p[186]=0;}
  auto& first=state.parts[0];first[189]=1;first[190]=15;std::fill(first.begin()+192,first.begin()+208,' ');constexpr char name[]="New Performance";std::copy_n(name,15,first.begin()+192);state.activeMask=15;
 }else{state.parts[0]=state.parts[currentPart()];selectedPart.store(0);state.activeMask=1;}
 state.performance=enabled;return queueState(state,true,error);
}
bool Processor::setPartActive(unsigned part,bool active,juce::String& error){
 std::lock_guard guard(controlMutex);if(!ownedDevice || part>=ks::PartCount)return false;auto state=ownedDevice->fullSnapshot();if(!state.performance)return false;
 state.activeMask=uint8_t(active?(state.activeMask|(1u<<part)):(state.activeMask&~(1u<<part)));state.parts[0][190]=state.activeMask;return queueState(state,false,error);
}
bool Processor::selectPerformance(int index,juce::String& error){
 std::lock_guard guard(controlMutex);if(!ownedDevice){error="Load firmware first";return false;}if(index<0 || index>=int(performanceLibrary.size()))return false;
 ks::State state;state.performance=true;for(unsigned part=0;part<ks::PartCount;++part)std::copy_n(performanceLibrary[index].data.data()+13+part*256,256,state.parts[part].begin());
 state.activeMask=state.parts[0][190]&15;return queueState(state,true,error);
}
bool Processor::selectPreset(int index,juce::String& error){
 std::lock_guard guard(controlMutex);if(!ownedDevice){error="Load firmware, wave bank and factory data first";return false;}if(index<0 || index>=int(library.size())){error="Preset index out of range";return false;}
 auto state=ownedDevice->fullSnapshot();const auto& preset=library[index];
 if(state.performance){const auto part=currentPart();const unsigned channel=state.parts[part][181]&15;unsigned matches=0;for(unsigned n=0;n<ks::PartCount;++n)if((state.activeMask&(1u<<n)) && (state.parts[n][181]&15)==channel)++matches;
  // Original addressed Program Change changes one part without reloading its
  // neighbours. For imported sounds/layered channels use a complete native dump.
  if(preset.bank && matches==1 && (state.activeMask&(1u<<part))){const uint8_t bytes[]{uint8_t(0xb0|channel),32,preset.bank,uint8_t(0xc0|channel),preset.number};if(!queueUiMidi(bytes,sizeof(bytes))){error="Preset queue is full";return false;}resetPending();return true;}
  std::copy_n(preset.data.data()+13,179,state.parts[part].begin());return queueState(state,false,error);
 }
 std::copy_n(preset.data.data()+13,179,state.parts[0].begin());return queueState(state,false,error);
}
bool Processor::importPresets(const juce::File& file,juce::String& error){
 if(file.getSize()>4*1024*1024){error="Preset file is too large (maximum 4 MiB)";return false;}
 std::vector<Preset> imported;std::vector<Performance> multiImported;
 const auto accept=[&](const uint8_t* p,size_t n){if(!validDump(p,n))return false;
  if(n==1038){Performance preset;preset.name=nameAt(p+13,192);if(preset.name.isEmpty())preset.name=file.getFileNameWithoutExtension();std::copy_n(p,n,preset.data.begin());preset.data[6]=0x7f;preset.data[7]=2;preset.data[8]=preset.data[11]=preset.data[12]=0;multiImported.push_back(std::move(preset));}
  else{Preset preset;preset.name=patchName(p+13);if(preset.name.isEmpty())preset.name=file.getFileNameWithoutExtension();std::copy_n(p,n,preset.data.begin());preset.data[6]=0x7f;preset.data[7]=0;preset.data[8]=preset.data[11]=preset.data[12]=0;imported.push_back(std::move(preset));}return true;};
 if(file.hasFileExtension("mid;midi")){juce::MidiFile midi;auto in=file.createInputStream();if(!in || !midi.readFrom(*in)){error="Invalid MIDI file";return false;}
  for(int t=0;t<midi.getNumTracks();++t)for(int n=0;n<midi.getTrack(t)->getNumEvents();++n){const auto& m=midi.getTrack(t)->getEventPointer(n)->message;if(m.isSysEx() && !accept(m.getRawData(),m.getRawDataSize())){error="MIDI contains an unsupported or malformed KS dump";return false;}}
 }else{juce::MemoryBlock block;if(!file.loadFileAsData(block)){error="Cannot read presets";return false;}const auto* p=static_cast<const uint8_t*>(block.getData());size_t begin=0;while(begin<block.getSize()){if(p[begin]!=0xf0){error="Expected a KS SysEx";return false;}size_t end=begin+1;while(end<block.getSize() && p[end]!=0xf7)++end;if(end==block.getSize() || !accept(p+begin,end-begin+1)){error="Expected a 270-byte program or 1038-byte performance";return false;}begin=end+1;}}
 if(imported.empty() && multiImported.empty()){error="No KS programs or performances found";return false;}
 size_t first,firstMulti;{std::lock_guard guard(controlMutex);first=library.size();firstMulti=performanceLibrary.size();library.insert(library.end(),imported.begin(),imported.end());performanceLibrary.insert(performanceLibrary.end(),multiImported.begin(),multiImported.end());}
 if(firmwareReady())return imported.empty()?selectPerformance(int(firstMulti),error):selectPreset(int(first),error);return true;
}
}
