#include "silverProcessor.h"
#include "../../controller/command/controllerArchive.h"
#include "silverEditor.h"
#include "silverKEngine.h"
#include "kstationLib/command/kCommandStartup.h"
#include "kstationLib/command/kProgramFlash.h"
#include "kstationLib/kstationRomLoader.h"
#include "ksLib/command/ksControllerCheckpoint.h"
#include "ksLib/ksRom.h"
#include "baseLib/sha256.h"
#include <chrono>
#include <cstdlib>
#include <thread>
namespace silverK {
using namespace ksPlugin;
namespace {
namespace command = novation::controller::command;
thread_local const Processor* syncingProcessor=nullptr;

// The optional drum target travels with a pending APVTS value. A subsequent
// MIDI key can change the editor while the previous knob edit is still queued.
struct DrumParameterAddress {
 static constexpr unsigned ValueBits=7, ProgramBits=6, BankShift=ProgramBits, SlotShift=BankShift+2,
                           RevisionShift=SlotShift+3;
 static constexpr int64_t ValueMask=(1<<ValueBits)-1;
 static uint64_t encode(uint64_t revision,uint8_t slot,uint8_t bank,uint8_t program)noexcept {
  if(!revision || revision>=(uint64_t(1)<<(63-ValueBits-RevisionShift)) || bank<5 || bank>8)return 0;
  return (revision<<RevisionShift)|(uint64_t(slot)<<SlotShift)|(uint64_t(bank-5)<<BankShift)|program;
 }
 static unsigned slot(uint64_t address)noexcept{return unsigned((address>>SlotShift)&7);}
 static command::KSDrumVoiceEdit edit(uint64_t address,const Control& c,int value)noexcept {
  return {address>>RevisionShift,uint8_t(slot(address)),uint8_t(5+((address>>BankShift)&3)),
          uint8_t(address&((1<<ProgramBits)-1)),uint8_t(c.patchOffset),c.patchMask,
          uint8_t((value<<c.patchShift)&c.patchMask)};
 }
};
constexpr auto arpAutomationParameters=[]{
 using kstation::controls::Catalog;
 using kstation::controls::Mapping;
 constexpr auto missing=Catalog.size();
 std::array<size_t,4> indices{missing,missing,missing,missing};
 for(size_t i=0;i<Catalog.size();++i){
  const auto& control=Catalog[i];
  if(control.mapping==Mapping::CC){
   if(control.number==105)indices[0]=i;
   if(control.number==106)indices[1]=i;
   if(control.number==80)indices[2]=i;
  }else if(control.mapping==Mapping::NRPN && control.number==11)indices[3]=i;
 }
 return indices;
}();
static_assert(arpAutomationParameters[0]<kstation::controls::Catalog.size()
              && arpAutomationParameters[1]<kstation::controls::Catalog.size()
              && arpAutomationParameters[2]<kstation::controls::Catalog.size()
              && arpAutomationParameters[3]<kstation::controls::Catalog.size());
std::vector<uint8_t> readArchiveBytes(const juce::String& encoded){
 juce::MemoryBlock raw;
 constexpr size_t Limit=2*1024*1024;
 if(encoded.isEmpty() || size_t(encoded.length())>2*Limit ||
    !raw.fromBase64Encoding(encoded) || !raw.getSize() || raw.getSize()>Limit)return {};
 const auto* first=static_cast<const uint8_t*>(raw.getData());
 return {first,first+raw.getSize()};
}
std::vector<uint8_t> readNativeState(const juce::ValueTree& state,const char* key,command::Model model,const char* hashKey){
 auto bytes=readArchiveBytes(state.getProperty(key).toString());
 const auto saved=command::decodeControllerArchive(bytes);
 if(!saved || saved->model!=model || saved->resourceHash!=state.getProperty(hashKey).toString().toStdString())return {};
 return bytes;
}
juce::String ksPartName(const std::vector<uint8_t>& dump,unsigned part){
 const auto* text=dump.data()+13+(dump.size()==1038?part*256:0)+151;
 juce::String name;
 for(unsigned i=0;i<16;++i)name+=juce::String::charToString(text[i]>=32 && text[i]<127?text[i]:' ');
 return name.trim();
}
juce::String kProgramKey(const command::ControllerArchive& archive){
 // The edit buffer contains sound bytes, while the selected program address
 // lives in the global page. Both identify the temporary K-Station conversion.
 constexpr auto address=command::KProgramFlash::Globals-command::KProgramFlash::UserOffset;
 return juce::String(baseLib::SHA256(archive.activeProgram).toString())+":"
     +juce::String(archive.userFlash[address])+":"+juce::String(archive.userFlash[address+1]);
}
std::vector<uint8_t> bytes(const juce::File& f){juce::MemoryBlock b;if(!f.loadFileAsData(b))return {};auto* p=static_cast<const uint8_t*>(b.getData());return {p,p+b.getSize()};}
bool firmwareFileSizeValid(const juce::File& file){
 const auto size=file.getSize();return size>0 && size<=(file.hasFileExtension("zip")?8:2)*1024*1024;
}
kstation::Rom readK(const juce::File& file){if(!file.hasFileExtension("zip"))return kstation::Rom(file.getFullPathName().toStdString());juce::ZipFile zip(file);for(int i=0;i<zip.getNumEntries();++i){const auto* e=zip.getEntry(i);if(e->uncompressedSize<=0 || e->uncompressedSize>2*1024*1024 || !juce::File(e->filename).hasFileExtension("syx;os;bin"))continue;std::unique_ptr<juce::InputStream> in(zip.createStreamForEntry(i));juce::MemoryBlock b;if(!in || in->readIntoMemoryBlock(b,2*1024*1024)<=0)continue;auto* p=static_cast<const uint8_t*>(b.getData());kstation::Rom rom(std::vector<uint8_t>(p,p+b.getSize()),e->filename.toStdString());if(rom.isValid())return rom;}return {};}
ks::Rom firmware(const juce::File& file) {
 if (!file.hasFileExtension("zip")) return ks::Rom(file.getFullPathName().toStdString());
 juce::ZipFile archive(file);
 for (int i=0;i<archive.getNumEntries();++i) {
  const auto* entry=archive.getEntry(i);
  if (!entry || entry->uncompressedSize<=0 || entry->uncompressedSize>131072
      || !juce::File(entry->filename).hasFileExtension("bin;os;syx")) continue;
  auto input=std::unique_ptr<juce::InputStream>(archive.createStreamForEntry(i));
  juce::MemoryBlock data;
  if (!input || input->readIntoMemoryBlock(data,131073)<=0 || data.getSize()>131072) continue;
  auto* b=static_cast<const uint8_t*>(data.getData());
  ks::Rom candidate(std::vector<uint8_t>(b,b+data.getSize()),file.getFullPathName().toStdString());
  if (candidate.isValid()) return candidate;
 }
 return {};
}
}
juce::AudioProcessorValueTreeState::ParameterLayout Processor::layout(){
 juce::AudioProcessorValueTreeState::ParameterLayout p;
 for(unsigned part=0;part<ks::PartCount;++part)for(const auto& c:controls){
  const auto id=parameterId(c,part);
  const juce::String name=(part?"Part "+juce::String(part+1)+" / ":juce::String{})+juce::String(c.group)+" / "+c.section+" / "+c.label;
  if(c.enumCount){juce::StringArray labels;for(unsigned n=0;n<c.enumCount;++n)labels.add(c.enumLabels[n]);p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(id,1),name,labels,c.defaultValue));}
  else p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(id,1),name,c.minimum,c.maximum,c.defaultValue));
 }
 for(const auto& c:kControls()){const auto id="k_"+juce::String(c.id);const auto name="K-Station / "+juce::String(c.group)+" / "+c.section+" / "+c.label;if(c.enumCount){juce::StringArray labels;for(unsigned n=0;n<c.enumCount;++n)labels.add(c.enumLabels[n]);p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(id,1),name,labels,c.defaultValue));}else p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(id,1),name,c.minimum,c.maximum,c.defaultValue));}
 return p;
}
Processor::Processor():AudioProcessor(BusesProperties().withInput("Vocoder / external",juce::AudioChannelSet::mono(),juce::PluginHostType::getPluginLoadedAs()==wrapperType_Standalone).withOutput(juce::PluginHostType::getPluginLoadedAs()==wrapperType_Standalone?"Outputs 1-4":"Main 1+2",juce::PluginHostType::getPluginLoadedAs()==wrapperType_Standalone?juce::AudioChannelSet::discreteChannels(4):juce::AudioChannelSet::stereo(),true).withOutput("Aux 3+4",juce::AudioChannelSet::stereo(),false)),parameters(*this,nullptr,"SilverParameters",layout()),libraryOwner(sharedLibrary(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",{}).isEmpty()?juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("The Usual Suspects/Silver-K/patches"):juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",{})))),patchLibrary(*libraryOwner){
 for(size_t n=0;n<TotalCount;++n){parameterIds[n]=n<ParameterCount?parameterId(controls[n%controls.size()],unsigned(n/controls.size())):"k_"+juce::String(kstation::controls::Catalog[n-ParameterCount].id);pending[n].store(-1);awaiting[n].store(-1);editTime[n].store(0);parameters.addParameterListener(parameterIds[n],this);}
 const auto* threads=std::getenv("SILVER_KS_DSP_THREADS");
 twoCoreProcessing.store(threads && std::strcmp(threads,"2")==0);
 initializeDataFolder();startTimerHz(10);
}
Processor::~Processor(){stopTimer();if(firmwareLoader.joinable())firmwareLoader.join();firmwareLoading.store(false);savePreferences(true);for(const auto& id:parameterIds)parameters.removeParameterListener(id,this);stopEngines();}
bool Processor::isBusesLayoutSupported(const BusesLayout& l)const{
 if(l.inputBuses.size()>1 || (l.inputBuses.size()==1 && !l.inputBuses[0].isDisabled() && l.inputBuses[0]!=juce::AudioChannelSet::mono() && l.inputBuses[0]!=juce::AudioChannelSet::stereo()) || l.outputBuses.isEmpty())return false;
 // JUCE's standalone player describes physical device channels as one bus.
 // Plugins retain two independently enabled stereo buses for the DAW.
 if(wrapperType==wrapperType_Standalone)return l.outputBuses.size()<=2 && (l.outputBuses[0].size()==2 || l.outputBuses[0].size()==4) && (l.outputBuses.size()==1 || l.outputBuses[1].isDisabled());
 return l.outputBuses.size()==2 && l.outputBuses[0]==juce::AudioChannelSet::stereo() && (l.outputBuses[1].isDisabled() || l.outputBuses[1]==juce::AudioChannelSet::stereo());
}
void Processor::parameterChanged(const juce::String& id,float v){
 if(syncingProcessor==this)return;
 for(size_t n=0;n<TotalCount;++n)if(id==parameterIds[n]){
  int64_t value=int(std::round(v));
  if(n<ParameterCount && command::KSDrumVoiceEdit::voiceField(controls[n%controls.size()].patchOffset)){
   const auto address=drumParameterAddress.load();
   if(address && n/controls.size()+(performanceMode()?1:0)==DrumParameterAddress::slot(address))
    value|=int64_t(address<<DrumParameterAddress::ValueBits);
  }
  awaiting[n].store(value);editTime[n].store(juce::Time::getMillisecondCounter());pending[n].store(value);
  return;
 }
}
void Processor::refreshControls(){
 if(isFirmwareLoading())return;
 std::lock_guard guard(controlMutex);
 if(ownedK){std::array<uint8_t,128> patch;if(!ownedK->patch(patch))return;syncingProcessor=this;
 for(size_t i=0;i<KCount;++i){const auto& c=kstation::controls::Catalog[i];if(c.patchOffset<0)continue;const size_t n=ParameterCount+i;int v=(patch[c.patchOffset]&c.patchMask)>>c.patchShift;v=juce::jlimit(int(c.minimum),int(c.maximum),v);const int requested=awaiting[n].load();if(requested>=0){if(v==requested && pending[n].load()<0)awaiting[n].store(-1);else if(uint32_t(juce::Time::getMillisecondCounter()-editTime[n].load())<2000 || pending[n].load()>=0)continue;else awaiting[n].store(-1);}auto* p=parameters.getParameter(parameterIds[n]);if(p->convertFrom0to1(p->getValue())!=v)p->setValueNotifyingHost(p->convertTo0to1(v));}syncingProcessor=nullptr;return;}
 if(!ownedDevice)return;
 const auto state=ownedDevice->editorState(false);
 if(!state)return;
 multi.store(state->performance);activeMask.store(state->activeMask);
 for(unsigned part=0;part<ks::PartCount;++part)partChannels[part].store(state->parts[part].parameters[181]&15);
 if(!state->performance)selectedPart.store(0);
 const auto slot=drumEditorSlot();
 const auto& drumEditor=state->drumEditor;
 const auto& selected=state->parts[state->performance?slot-1:0];
 const auto address=drumEditEnabled() && drumEditor.slot==slot?
  DrumParameterAddress::encode(selected.revisions.program,slot,selected.program.bank,drumEditor.program):0;
 drumParameterAddress.store(address);
 syncingProcessor=this;
 for(size_t n=0;n<(state->performance?ParameterCount:controls.size());++n){
  const auto part=n/controls.size();
  const auto& c=controls[n%controls.size()];
  if(c.patchOffset<0)continue;
  const bool drum=address && part+(state->performance?1:0)==slot && command::KSDrumVoiceEdit::voiceField(c.patchOffset);
  const auto* patch=drum?drumEditor.voice.data():state->parts[part].parameters.data();
  const auto value=juce::jlimit(int(c.minimum),int(c.maximum),int(decode(c,patch)));
  auto wanted=awaiting[n].load();
  if(wanted>=0){
   const auto wantedAddress=uint64_t(wanted)>>DrumParameterAddress::ValueBits;
   const auto wantedValue=wanted&DrumParameterAddress::ValueMask;
   if(wantedAddress==(drum?address:0) && (value!=wantedValue || pending[n].load()>=0) &&
      (uint32_t(juce::Time::getMillisecondCounter()-editTime[n].load())<2000 || pending[n].load()>=0))
     continue;
   awaiting[n].compare_exchange_strong(wanted,-1);
  }
  if(parameters.getRawParameterValue(parameterIds[n])->load()!=value){auto* parameter=parameters.getParameter(parameterIds[n]);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
 }syncingProcessor=nullptr;
}
void Processor::rebuild(){
 stopEngines();
 kMode.store(false);
 if(os.empty() || bank.size()!=ks::Board::WaveSize || flash.size()!=0x80000)return;
 try {
  auto candidate=std::make_unique<KsEngine>(os,bank,flash,programToRestore,ksCommandState,
      hostRate,hostBlock,CommandEngine::BufferFloors[bufferProfile()],ksTwoCoreProcessing(),userDrums);
  if(ksCommandState.empty())ksCommandState=candidate->saveArchive();
  publishKS(std::move(candidate));
 } catch(const std::exception& error) { message=error.what(); }
}
void Processor::publishKS(std::unique_ptr<KsEngine> ready){
 kMode.store(false);
 ownedDevice=std::move(ready);
 applyBufferProfile();
 auto* dReady=ownedDevice.get();
 const auto state=dReady->fullSnapshot();controlChannel.store(state.globalChannel);wireShadow=state.parts;for(auto& mask:wirePendingMask)mask.fill(0);multi.store(state.performance);activeMask.store(state.activeMask);
 for(unsigned part=0;part<ks::PartCount;++part)partChannels[part].store(state.parts[part][181]&15);
 if(!state.performance)selectedPart.store(0);syncingProcessor=this;
 for(size_t n=0;n<ParameterCount;++n){const auto& c=controls[n%controls.size()];const auto& patch=state.parts[n/controls.size()];if(c.patchOffset>=0 && (state.performance || n<controls.size())){auto* p=parameters.getParameter(parameterIds[n]);p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(int(c.minimum),int(c.maximum),int(decode(c,patch.data())))));}pending[n].store(-1);awaiting[n].store(-1);}
 syncingProcessor=nullptr;{std::lock_guard lock(uiMidiMutex);uiMidi.reset();}
 for(unsigned part=0;part<(state.performance?ks::PartCount:1);++part)for(const auto& c:controls)if(c.patchOffset<0){const uint8_t midi[]{uint8_t(0xb0|(state.performance?partChannels[part].load():controlChannel.load())),uint8_t(c.number),uint8_t(parameters.getRawParameterValue(parameterId(c,part))->load())};dReady->midi(midi,3);}resetClock.store(true);device.store(dReady);
 message=placeholder?"KS OS 2.1.02 - temporary zero wave ROM; sampled waves are silent":"KS OS 2.1.02 - external wave bank loaded";
}
bool Processor::load(const juce::File& o,const juce::File& b,const juce::File& f,juce::String& e){
 const juce::ScopeGuard notify{[this]{notifyHostLatency();}};
 std::lock_guard guard(controlMutex);
 auto rom=firmware(o);if(!rom.isValid()){e=rom.error();return false;}
 return loadKS(o,b,f,rom.data(),e);
}
bool Processor::loadKS(const juce::File& o,const juce::File& b,const juce::File& f,std::vector<uint8_t> firmwareImage,juce::String& e){
 auto waves=bytes(b);std::vector<uint8_t> storage;
 const auto dataFiles=f==flashFile?selectedKSDataFiles():std::vector<juce::File>{f};
 if(!readKSData(dataFiles,storage,e))return false;
 if(waves.size()!=ks::Board::WaveSize){e="Put the KS Wave ROM (129024 bytes) in ROMs, then click Rescan in Setup.";return false;}
 const auto managedOS=managedAsset(o,e),managedWave=managedAsset(b,e);if(managedOS==juce::File{} || managedWave==juce::File{})return false;
 std::vector<juce::File> managedData;for(const auto& file:dataFiles){auto saved=managedAsset(file,e);if(saved==juce::File{})return false;managedData.push_back(saved);}
 auto outgoing=captureFirmwareSound(e);
 if(!outgoing)return false;
 const auto& recalled=ownedDevice?*outgoing:lastKSSound;
 auto saved=command::decodeControllerArchive(recalled.archive);
 auto part=recalled.part;
 auto patchName=recalled.name;
 std::vector<uint8_t> program;
 const bool fromK=bool(ownedK);
 if(fromK && firmwareSwitch()==FirmwareSwitch::KeepPatch && !outgoing->archive.empty()){
  const auto source=command::decodeControllerArchive(outgoing->archive);
  if(!source || source->model!=command::Model::K){e="Cannot read the current K-Station patch";return false;}
  const bool returningToOriginal=saved && keptKProgramKey.isNotEmpty() && kProgramKey(*source)==keptKProgramKey;
  if(!returningToOriginal){
   program=convertPatch(source->activeProgram,Model::KS).data;
   if(program.empty()){e="Cannot convert the current K-Station patch";return false;}
   if(outgoing->name.isNotEmpty()){
    std::fill_n(program.begin()+13+151,16,' ');
    for(int i=0;i<std::min(16,outgoing->name.length());++i){
     const auto c=outgoing->name[i];
     program[13+151+i]=uint8_t(c>=32 && c<127?c:'?');
    }
   }
   part=0;
   patchName=outgoing->name;
  }
 }
 const auto dataHash=juce::String(baseLib::SHA256(storage).toString());
 try{
  std::vector<uint8_t> archive;
  if(saved){
   saved->resourceHash=baseLib::SHA256(waves).toString();
   if(recalled.dataHash!=dataHash){
    saved->userFlash.assign(storage.begin()+0x1fc00,storage.begin()+0x60000);
    saved->ksCheckpoint.clear();
   }
   if(!program.empty()){
    saved->activeProgram=program;
    saved->ksCheckpoint.clear();
   }
   archive=command::encodeControllerArchive(*saved);
  }
  auto candidate=std::make_unique<KsEngine>(firmwareImage,waves,storage,program,archive,
      hostRate,hostBlock,CommandEngine::BufferFloors[bufferProfile()],ksTwoCoreProcessing(),userDrums);
  auto initial=candidate->saveArchive();
  if(initial.empty()){e="Cannot read the loaded KS sound";return false;}
  const bool rebuildLibrary=flash.empty() || ksDataHash!=dataHash;
  stopEngines();
  if(fromK && !outgoing->archive.empty())lastKSound=std::move(*outgoing);
  keptKProgramKey.clear();
  os=std::move(firmwareImage);bank=std::move(waves);flash=std::move(storage);
  ksDataHash=dataHash;ksCommandState=std::move(initial);programToRestore=std::move(program);
  osFile=managedOS;bankFile=managedWave;flashFiles=std::move(managedData);flashFile=flashFiles.front();
  loadedWaveFile=bankFile;loadedDataFiles=flashFiles;
  placeholder=std::all_of(bank.begin(),bank.end(),[](uint8_t v){return v==0;});
  currentPatch=std::move(patchName);selectedPart.store(part);
  if(rebuildLibrary)buildLibrary();
  publishKS(std::move(candidate));
  return true;
 }catch(const std::exception& failure){e=failure.what();return false;}
}
void Processor::prepareToPlay(double r,int b){
 const juce::ScopeGuard notify{[this]{notifyHostLatency();}};
 std::lock_guard guard(controlMutex);
 if(hostRate==r && hostBlock==b && firmwareReady())return;
 juce::String error;
 if(!reconfigureEngines(r,b,bufferProfile(),ksTwoCoreProcessing(),error))message=error;
}
bool Processor::reconfigureEngines(double rate,int block,int profile,bool parallel,juce::String& error){
 try {
  const auto floor=CommandEngine::BufferFloors[profile];
  std::unique_ptr<KEngine> nextK;
  std::unique_ptr<KsEngine> nextKS;
  std::vector<uint8_t> saved;
  if(ownedK){
   saved=ownedK->saveArchive();
   if(saved.empty()){error="K-Station is busy. Try again after pending MIDI completes.";return false;}
   nextK=std::make_unique<KEngine>(kRom,rate,block,saved,floor);
  }else if(ownedDevice){
   saved=ownedDevice->saveArchive();
   if(saved.empty()){error="KS is busy. Try again after pending MIDI completes.";return false;}
   collectPatchLoads();
   nextKS=std::make_unique<KsEngine>(os,bank,flash,programToRestore,saved,rate,block,floor,parallel,userDrums);
  }
  if(nextK || nextKS)stopEngines();
  hostRate=rate;hostBlock=block;buffering.store(profile);twoCoreProcessing.store(parallel);
  if(nextK){kState=std::move(saved);publishK(std::move(nextK));}
  if(nextKS){ksCommandState=std::move(saved);publishKS(std::move(nextKS));}
  return true;
 } catch(const std::exception& failure){error=failure.what();return false;}
}
Processor::HostTiming Processor::prepareClockPosition(){
 if(resetClock.exchange(false)){hostClock.reset();clockSourceMode=-1;externalNrpn.fill(0);}
 activeClockSource=requestedClockSource.load();
 HostTiming timing;
 auto& position=timing.position;
 if(activeClockSource==ClockSource::Host)if(auto* playHead=getPlayHead())if(auto info=playHead->getPosition()){
  if(auto bpm=info->getBpm();bpm && std::isfinite(*bpm) && *bpm>=1 && *bpm<=1000){
   position.available=true;
   position.bpm=*bpm;
  }
  position.playing=info->getIsPlaying();
  if(auto ppq=info->getPpqPosition()){position.hasPpq=true;position.ppq=*ppq;}
  if(info->getIsLooping())if(auto loop=info->getLoopPoints())
   timing.loop=HostLoop{loop->ppqStart,loop->ppqEnd};
 }
 return timing;
}
void Processor::setClockSource(ClockSource source){
 if(source!=ClockSource::Host && source!=ClockSource::Internal && source!=ClockSource::Midi)return;
 if(requestedClockSource.exchange(source)!=source)
  updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
}
void Processor::setFirmwareSwitch(FirmwareSwitch mode){
 if(mode!=FirmwareSwitch::KeepPatch && mode!=FirmwareSwitch::RecallLast)return;
 if(firmwareSwitchMode.exchange(mode)==mode)return;
 savePreferences();
 updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
}
void Processor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){
 using namespace baseLib::performance;
 juce::ScopedNoDenormals no;
 activeAudio.fetch_add(1);
 struct Guard{std::atomic<unsigned>& n;~Guard(){--n;}} guard{activeAudio};
 auto* k=kDevice.load();
 auto* d=device.load();
 if((!k && !d) || !audio.getNumSamples()){audio.clear();midi.clear();return;}
 Scope callbackTiming(&performanceDiagnostics,Metric::Callback,uint64_t(audio.getNumSamples()),hostRate>0?uint64_t(audio.getNumSamples()*1.0e9/hostRate):0);
 const auto diagnosticEpoch=performanceDiagnostics.epoch();
 if(diagnosticEpoch){performanceDiagnostics.add(Metric::HostFrames,diagnosticEpoch,uint64_t(audio.getNumSamples()));performanceDiagnostics.add(Metric::OfflineCallbacks,diagnosticEpoch,isNonRealtime()?1:0);}
 const auto [position,loop]=prepareClockPosition();
 if(k){hostClock.begin(position,hostRate,audio.getNumSamples());processK(*k,audio,midi);return;}
 Scope controlTiming(&performanceDiagnostics,Metric::Controls);
 // KS receives the DAW's position directly at its audio deadline. The
 // firmware's Auto mode still accepts MIDI Clock when the host has no tempo.
 selectClockSource(false,[&](const uint8_t* data,size_t size){d->midi(data,size);});
 uint8_t uiBytes[4096];size_t uiCount=0;uint16_t item=0;
 for(size_t n=0;n<sizeof(uiBytes) && uiMidi.pop(item);++n){
  if(item<UiSoundLoadToken){uiBytes[uiCount++]=uint8_t(item);continue;}
  if(uiCount){d->midi(uiBytes,uiCount);uiCount=0;}
  d->enqueueSoundLoad(uint8_t(item-UiSoundLoadToken));
 }
 if(uiCount)d->midi(uiBytes,uiCount);
 d->setDrumEditorSlot(drumEditEnabled()?drumEditorSlot():command::KSDrumEditMode::Off);
 const auto state=d->fullSnapshot(true);
 if(resetWire.exchange(false))for(auto& mask:wirePendingMask)mask.fill(0);
 for(unsigned part=0;part<ks::PartCount;++part){auto& shadow=wireShadow[part];auto& mask=wirePendingMask[part];const auto& patch=state.parts[part];
  for(unsigned n=0;n<256;++n){if(((patch[n]^shadow[n])&mask[n])==0)mask[n]=0;shadow[n]=uint8_t((patch[n]&~mask[n])|(shadow[n]&mask[n]));}
  uint8_t channel=state.performance?(patch[181]&15):controlChannel.load();
  for(size_t index=0;index<controls.size();++index){const size_t n=part*controls.size()+index;
   const auto requested=pending[n].exchange(-1);
   if(requested<0 || (!state.performance && part))continue;
   const auto& c=controls[index];
   const auto address=uint64_t(requested)>>DrumParameterAddress::ValueBits;
   const auto value=int(requested&DrumParameterAddress::ValueMask);
   if(address){
    if(!d->editDrumVoice(DrumParameterAddress::edit(address,c,value))){
     int64_t empty=-1;pending[n].compare_exchange_strong(empty,requested);
    }
    continue;
   }
   if(n==0)volume.store(uint8_t(value));
   uint8_t wire=uint8_t(value);
   if(c.patchOffset>=0){auto& raw=shadow[c.patchOffset];raw=uint8_t((raw&~c.patchMask)|((value<<c.patchShift)&c.patchMask));mask[c.patchOffset]|=c.patchMask;
    wire=uint8_t((raw&~c.patchMask)|((value<<c.wireShift)&c.wireMask));
    if(c.mapping==Mapping::CC && c.number==70)wire=uint8_t(value);
   }
   const auto status=uint8_t(0xb0|channel);
   if(c.mapping==Mapping::NRPN){uint8_t b[]{status,99,0,status,98,uint8_t(c.number),status,6,wire};if(state.performance)d->partMidi(part,b,9);else d->midi(b,9);}else{uint8_t b[]{status,uint8_t(c.number),wire};if(state.performance)d->partMidi(part,b,3);else d->midi(b,3);}
   if(state.performance && c.patchOffset==181)channel=uint8_t(value&15);
  }
 }

 sendTimedMidi(midi,position.available,nullptr,[&](const uint8_t* data,size_t size,int offset){
  if(size==3 && data[0]==0xb0 && data[1]==7)volume.store(data[2]);
  if(position.available && position.playing)d->hostMidi(data,size,uint32_t(offset));
  else d->midi(data,size,uint32_t(offset));
 });
 std::array<float*,ks::OutputCount> channels{audio.getWritePointer(0),audio.getWritePointer(1),nullptr,nullptr};
 if(wrapperType==wrapperType_Standalone && getMainBusNumOutputChannels()==4){channels[2]=audio.getWritePointer(2);channels[3]=audio.getWritePointer(3);}
 else if(getBusCount(false)>1 && getBus(false,1)->isEnabled()){auto aux=getBusBuffer(audio,false,1);channels[2]=aux.getWritePointer(0);channels[3]=aux.getWritePointer(1);}
 controlTiming.finish();Scope renderTiming(&performanceDiagnostics,Metric::AudioRender,uint64_t(audio.getNumSamples()));
 d->render(channels,audio.getNumSamples(),isNonRealtime(),getTotalNumInputChannels()?audio.getReadPointer(0):nullptr,getTotalNumInputChannels()>1?audio.getReadPointer(1):nullptr,position,loop);midi.clear();
}
juce::String Processor::status()const{
 if(isFirmwareLoading())return "Loading firmware...";
 std::lock_guard guard(controlMutex);
 const auto failure=ownedK?ownedK->failure():ownedDevice?ownedDevice->failure():std::string{};
 return failure.empty()?message:juce::String(failure);
}
uint64_t Processor::underruns()const{std::lock_guard guard(controlMutex);return ownedK?ownedK->underruns():ownedDevice?ownedDevice->underruns():0;}
void Processor::getStateInformation(juce::MemoryBlock& out){std::lock_guard guard(controlMutex);
 collectPatchLoads();
 // The active engine may still use its previous assets after an incomplete scan.
 const auto stateWaveFile=ownedDevice?loadedWaveFile:bankFile;
 const auto stateDataFiles=ownedDevice?loadedDataFiles:selectedKSDataFiles();
 const auto stateFlashFile=stateDataFiles.empty()?juce::File{}:stateDataFiles.front();
 auto state=parameters.copyState();state.setProperty("ksDataFiles",juce::JSON::toString(ksDataReferences(stateDataFiles)),nullptr);state.setProperty("clockSource",int(clockSource()),nullptr);state.setProperty("bufferProfile",buffering.load(),nullptr);state.setProperty("ksControllerMode",ksControllerMode(),nullptr);state.setProperty("dataFolder",dataFolder.getFullPathName(),nullptr);for(const auto& asset:{std::pair<const char*,juce::File>{"os",osFile},{"bank",stateWaveFile},{"flash",stateFlashFile}})if(asset.second.isAChildOf(dataFolder))state.setProperty(juce::String(asset.first)+"Relative",asset.second.getRelativePathFrom(dataFolder),nullptr);state.setProperty("romFolder",romFolder.getFullPathName(),nullptr);state.setProperty("skinChoice",skinChoice,nullptr);state.setProperty("editorScale",editorScale.load(),nullptr);state.setProperty("browserState",browserState,nullptr);state.setProperty("customSkin",customSkin,nullptr);state.setProperty("currentPatch",currentPatch,nullptr);state.setProperty("editorChannel",int(controlChannel.load()),nullptr);state.setProperty("kModel",isKStation(),nullptr);state.setProperty("bank",stateWaveFile.getFullPathName(),nullptr);state.setProperty("flash",stateFlashFile.getFullPathName(),nullptr);
 state.setProperty("ksTwoCoreProcessing",ksTwoCoreProcessing(),nullptr);
 const auto drums=command::encodeKSUserDrums(userDrums,false);
 state.setProperty("ksUserDrums",juce::MemoryBlock(drums.data(),drums.size()).toBase64Encoding(),nullptr);
 // Keep the drum grid readable by older builds when saving a DAW project.
 juce::StringArray drumSelections;
 int drumRetriggerOff=0;
 for(size_t i=0;i<userDrums.size();++i){
  const auto& drum=userDrums[i];
  drumSelections.add(juce::String(int(drum.selectedPattern)));
  if(!drum.retrigger)drumRetriggerOff|=1<<i;
 }
 state.setProperty("ksDrumSelections",drumSelections.joinIntoString(","),nullptr);
 state.setProperty("ksDrumRetriggerOff",drumRetriggerOff,nullptr);
 state.setProperty("firmwareSwitch",int(firmwareSwitch()),nullptr);
 if(isKStation() && keptKProgramKey.isNotEmpty())state.setProperty("keptKProgramKey",keptKProgramKey,nullptr);
 else state.removeProperty("keptKProgramKey",nullptr);
 const auto& recall=isKStation()?lastKSSound:lastKSound;
 if(recall.archive.empty()){
  for(const auto* key:{"firmwareRecall","firmwareRecallName","firmwareRecallData","firmwareRecallPart"})state.removeProperty(key,nullptr);
 }else{
  state.setProperty("firmwareRecall",juce::MemoryBlock(recall.archive.data(),recall.archive.size()).toBase64Encoding(),nullptr);
  state.setProperty("firmwareRecallName",recall.name,nullptr);
  state.setProperty("firmwareRecallData",recall.dataHash,nullptr);
  state.setProperty("firmwareRecallPart",int(recall.part),nullptr);
 }
 if(ownedK){
  if(auto saved=ownedK->saveArchive();!saved.empty())kState=std::move(saved);
  state.setProperty("os",osFile.getFullPathName(),nullptr);
  state.setProperty("osHash",juce::String(baseLib::SHA256(kRom).toString()),nullptr);
  state.setProperty("underruns",juce::int64(ownedK->underruns()),nullptr);
  auto legacy=kState;
  if(auto archive=command::decodeControllerArchive(kState)){
   legacy=command::encodeLegacyKArchive(*archive);
   state.setProperty("kCommandState",juce::MemoryBlock(kState.data(),kState.size()).toBase64Encoding(),nullptr);
  }else state.removeProperty("kCommandState",nullptr);
  state.setProperty("kState",juce::MemoryBlock(legacy.data(),legacy.size()).toBase64Encoding(),nullptr);
  if(auto xml=state.createXml())copyXmlToBinary(*xml,out);
  return;
 }
 state.setProperty("os",osFile.getFullPathName(),nullptr);state.setProperty("bank",stateWaveFile.getFullPathName(),nullptr);state.setProperty("flash",stateFlashFile.getFullPathName(),nullptr);state.setProperty("osHash",os.empty()?juce::String{}:juce::String(baseLib::SHA256(os).toString()),nullptr);state.setProperty("bankHash",bank.empty()?juce::String{}:juce::String(baseLib::SHA256(bank).toString()),nullptr);state.setProperty("placeholder",placeholder,nullptr);state.setProperty("selectedPart",int(selectedPart.load()),nullptr);
 auto d=device.load();if(d){auto stats=d->diagnostics();state.setProperty("produced",juce::int64(stats[0]),nullptr);state.setProperty("consumed",juce::int64(stats[1]),nullptr);state.setProperty("midiReceived",juce::int64(stats[2]),nullptr);state.setProperty("underruns",juce::int64(d->underruns()),nullptr);state.setProperty("midiOverflows",juce::int64(d->overflows()),nullptr);if(auto saved=d->saveArchive();!saved.empty())ksCommandState=std::move(saved);
 auto checkpoint=command::decodeControllerArchive(ksCommandState);
 auto p=checkpoint?std::move(checkpoint->activeProgram):d->dumpProgram();
 auto f=checkpoint?std::move(checkpoint->userFlash):std::vector<uint8_t>(flash.begin()+0x1fc00,flash.begin()+0x60000);
 state.setProperty("ksCommandState",juce::MemoryBlock(ksCommandState.data(),ksCommandState.size()).toBase64Encoding(),nullptr);
 state.setProperty("program",juce::MemoryBlock(p.data(),p.size()).toBase64Encoding(),nullptr);
 if(p.size()==270 || p.size()==1038)for(unsigned part=0;part<(p.size()==1038?ks::PartCount:1);++part)for(const auto& c:controls){const auto id=parameterId(c,part);const int raw=c.patchOffset<0?(juce::String(c.id)=="volume"?volume.load():int(parameters.getRawParameterValue(id)->load())):juce::jlimit(int(c.minimum),int(c.maximum),int(decode(c,p.data()+13+part*256)));for(int n=0;n<state.getNumChildren();++n){auto child=state.getChild(n);if(child.getProperty("id").toString()==id)child.setProperty("value",raw,nullptr);}}
state.setProperty("userFlash",juce::MemoryBlock(f.data(),f.size()).toBase64Encoding(),nullptr);}
 if(auto old=state.getChildWithName("PresetLibrary");old.isValid())state.removeChild(old,nullptr);
 juce::ValueTree imported("PresetLibrary");for(size_t n=libraryBaseCount;n<library.size();++n){juce::ValueTree entry("Preset");entry.setProperty("name",library[n].name,nullptr);entry.setProperty("data",juce::MemoryBlock(library[n].data.data(),270).toBase64Encoding(),nullptr);imported.appendChild(entry,nullptr);}state.appendChild(imported,nullptr);
 if(auto old=state.getChildWithName("PerformanceLibrary");old.isValid())state.removeChild(old,nullptr);
 juce::ValueTree multis("PerformanceLibrary");for(size_t n=performanceBaseCount;n<performanceLibrary.size();++n){juce::ValueTree entry("Performance");entry.setProperty("name",performanceLibrary[n].name,nullptr);entry.setProperty("data",juce::MemoryBlock(performanceLibrary[n].data.data(),1038).toBase64Encoding(),nullptr);multis.appendChild(entry,nullptr);}state.appendChild(multis,nullptr);
 if(auto xml=state.createXml())copyXmlToBinary(*xml,out);
}
void Processor::setStateInformation(const void* data,int size){
 auto xml=getXmlFromBinary(data,size);if(!xml)return;auto state=juce::ValueTree::fromXml(*xml);if(!state.isValid()||state.getType()!=juce::Identifier("SilverParameters"))return;
 // A host restore takes precedence over the delayed new-instance default boot.
 const juce::ScopeGuard notify{[this]{notifyHostLatency();}};
 std::lock_guard loadGuard(firmwareLoadMutex);hostStateRestored.store(true);startupPending.store(false);
 if(firmwareLoader.joinable())firmwareLoader.join();firmwareLoadDone.store(false);firmwareLoading.store(false);++firmwareRevision;
 std::lock_guard guard(controlMutex);
 struct PreferenceRestore{std::function<void()> finish;~PreferenceRestore(){finish();}} restore{[this]{preferenceBaseline=preferenceValues();}};
 stopEngines();os.clear();kRom.clear();kState.clear();bank.clear();flash.clear();programToRestore.clear();ksCommandState.clear();
 juce::MemoryBlock drumBytes;
 const auto savedDrums=state.getProperty("ksUserDrums").toString();
 if(savedDrums.length()<=int(2*command::KSUserDrumBytes{}.size()))
  drumBytes.fromBase64Encoding(savedDrums);
 userDrums=command::decodeKSUserDrums(static_cast<const uint8_t*>(drumBytes.getData()),drumBytes.getSize());
 const auto drumSelections=juce::StringArray::fromTokens(state.getProperty("ksDrumSelections").toString(),",","");
 const int drumRetriggerOff=state.getProperty("ksDrumRetriggerOff",0);
 uint8_t userMask=0;
 for(size_t i=0;i<userDrums.size();++i) {
  auto& drum=userDrums[i];
  const auto pattern=int(i)<drumSelections.size()?drumSelections[int(i)].getIntValue():
      int(drum.unrestrictedPattern().value_or(0));
  if(juce::isPositiveAndBelow(pattern,int(DrumGridEdit::Patterns)))drum.selectedPattern=uint8_t(pattern);
  if(drumRetriggerOff & (1<<i))drum.retrigger=false;
  if(drum.selected)userMask|=uint8_t(1u<<i);
 }
 userDrumMask.store(userMask);
 lastKSound={};lastKSSound={};keptKProgramKey.clear();ksDataHash.clear();loadedWaveFile=juce::File{};loadedDataFiles.clear();
 if(state.hasProperty("firmwareSwitch"))
  firmwareSwitchMode.store(int(state["firmwareSwitch"])==int(FirmwareSwitch::RecallLast)?FirmwareSwitch::RecallLast:FirmwareSwitch::KeepPatch);
 const bool savedK=bool(state.getProperty("kModel"));
 auto recallBytes=readArchiveBytes(state["firmwareRecall"].toString());
 if(const auto recall=command::decodeControllerArchive(recallBytes);recall && recall->model==(savedK?command::Model::KS:command::Model::K)){
  auto& entry=savedK?lastKSSound:lastKSound;
  entry={std::move(recallBytes),state["firmwareRecallName"].toString(),state["firmwareRecallData"].toString(),
         unsigned(juce::jlimit(0,3,int(state["firmwareRecallPart"])))};
  if(savedK)keptKProgramKey=state["keptKProgramKey"].toString();
 }
 const auto projectFolder=state["dataFolder"].toString();
 if(projectFolder.isNotEmpty() && juce::File(projectFolder).isDirectory())dataFolder=juce::File(projectFolder);
 for(const auto* child:{"ROMs","Patches","Skins"})if(auto result=dataFolder.getChildFile(child).createDirectory();result.failed()){message=result.getErrorMessage();return;}
 const auto projectROMs=dataFolder.getChildFile("ROMs");
 if(projectROMs!=romFolder){
  juce::String ignored;scanROMs(projectROMs,ignored);
 }
 browserState=state.getProperty("browserState",browserState).toString();
 const int savedClock=int(state.getProperty("clockSource",0));requestedClockSource.store(savedClock>=0 && savedClock<=2?ClockSource(savedClock):ClockSource::Host);
 const double savedScale=double(state.getProperty("editorScale",editorScale.load()));editorScale.store(savedScale>=.5&&savedScale<=1.5?savedScale:.75);
 twoCoreProcessing.store(bool(state.getProperty("ksTwoCoreProcessing",false)));
 buffering.store(juce::jlimit(0,3,int(state.getProperty("bufferProfile",1))));bankFile=resolveAsset(state,"bank");flashFile=resolveAsset(state,"flash");skinChoice=state.getProperty("skinChoice",defaultSkin).toString();customSkin=state.getProperty("customSkin").toString();currentPatch=state.getProperty("currentPatch").toString();controlChannel.store(uint8_t(int(state.getProperty("editorChannel",0))&15));
 restoreKSDataFiles(juce::JSON::parse(state.getProperty("ksDataFiles").toString()));
 if(savedK){auto o=resolveAsset(state,"os");auto rom=readK(o);if(!rom.isValid() || baseLib::SHA256(rom.data()).toString()!=state.getProperty("osHash").toString().toStdString()){message="K-Station firmware missing or hash mismatch";return;}kState=readNativeState(state,"kCommandState",command::Model::K,"osHash");
 if(kState.empty()){
  juce::MemoryBlock saved;
  if(!saved.fromBase64Encoding(state.getProperty("kState").toString()) || !saved.getSize())return;
  const auto* first=static_cast<const uint8_t*>(saved.getData());kState.assign(first,first+saved.getSize());
 }
 const auto managed=managedAsset(o,message);if(managed==juce::File{})return;
 kRom=rom.data();osFile=managed;parameters.replaceState(state);rebuildK();return;}
 juce::String e;auto o=resolveAsset(state,"os"),b=bankFile;
 if(!b.existsAsFile() && bool(state.getProperty("placeholder"))){
  auto zero=projectROMs.getChildFile("ks-wave-zero.bin");
  if(!zero.existsAsFile()){
   const juce::MemoryBlock waves(ks::Board::WaveSize,true);
   juce::TemporaryFile temp(zero);
   if(!temp.getFile().replaceWithData(waves.getData(),waves.getSize()) || !temp.overwriteTargetFileWithTemporary()){
    message="Cannot create zero Wave ROM in "+projectROMs.getFullPathName();return;
   }
  }
  b=zero;
 }
 std::vector<uint8_t> requiredData;if(!readKSData(selectedKSDataFiles(),requiredData,message))return;
 ksDataHash=juce::String(baseLib::SHA256(requiredData).toString());
 auto rom=firmware(o);auto waves=bytes(b);if(!rom.isValid()||waves.size()!=ks::Board::WaveSize || baseLib::SHA256(rom.data()).toString()!=state.getProperty("osHash").toString().toStdString() || baseLib::SHA256(waves).toString()!=state.getProperty("bankHash").toString().toStdString()){message="State OS/wave bank missing or hash mismatch";return;}
 const auto managedOS=managedAsset(o,message),managedWave=managedAsset(b,message);if(managedOS==juce::File{} || managedWave==juce::File{})return;
 std::vector<juce::File> managedData;for(const auto& file:selectedKSDataFiles()){const auto saved=managedAsset(file,message);if(saved==juce::File{})return;managedData.push_back(saved);}
 juce::MemoryBlock user;user.fromBase64Encoding(state.getProperty("userFlash").toString());if(user.getSize()!=0x60000-0x1fc00)return;
 flash.assign(0x80000,0xff);std::memcpy(flash.data()+0x1fc00,user.getData(),user.getSize());
 os=rom.data();bank=std::move(waves);osFile=managedOS;bankFile=managedWave;flashFiles=std::move(managedData);flashFile=flashFiles.front();placeholder=std::all_of(bank.begin(),bank.end(),[](uint8_t v){return v==0;});
 loadedWaveFile=bankFile;loadedDataFiles=flashFiles;
 juce::MemoryBlock p;p.fromBase64Encoding(state.getProperty("program").toString());if(p.getSize()!=270 && p.getSize()!=1038)return;auto* patch=static_cast<const uint8_t*>(p.getData());programToRestore.assign(patch,patch+p.getSize());
 ksCommandState=readNativeState(state,"ksCommandState",command::Model::KS,"bankHash");
 buildLibrary();auto imported=state.getChildWithName("PresetLibrary");for(int n=0;n<imported.getNumChildren() && n<12000;++n){auto entry=imported.getChild(n);juce::MemoryBlock data;if(!data.fromBase64Encoding(entry.getProperty("data").toString()) || data.getSize()!=270)continue;const auto* bytes=static_cast<const uint8_t*>(data.getData());constexpr uint8_t prefix[]{0xf0,0,0x20,0x29,1,0x42};if(!std::equal(std::begin(prefix),std::end(prefix),bytes) || bytes[7]!=0 || bytes[269]!=0xf7 || !std::all_of(bytes+1,bytes+269,[](uint8_t v){return v<128;}))continue;Preset preset;preset.name=entry.getProperty("name").toString();std::copy_n(bytes,270,preset.data.begin());library.push_back(std::move(preset));}
 auto multis=state.getChildWithName("PerformanceLibrary");for(int n=0;n<multis.getNumChildren() && n<12000;++n){auto entry=multis.getChild(n);juce::MemoryBlock data;if(!data.fromBase64Encoding(entry.getProperty("data").toString()) || data.getSize()!=1038)continue;const auto* bytes=static_cast<const uint8_t*>(data.getData());constexpr uint8_t prefix[]{0xf0,0,0x20,0x29,1,0x42};if(!std::equal(std::begin(prefix),std::end(prefix),bytes) || bytes[7]!=2 || bytes[1037]!=0xf7 || !std::all_of(bytes+1,bytes+1037,[](uint8_t v){return v<128;}))continue;Performance preset;preset.name=entry.getProperty("name").toString();std::copy_n(bytes,1038,preset.data.begin());performanceLibrary.push_back(std::move(preset));}
 selectedPart.store(unsigned(juce::jlimit(0,3,int(state.getProperty("selectedPart",0)))));parameters.replaceState(state);volume.store(uint8_t(parameters.getRawParameterValue("volume")->load()));for(auto& n:pending)n.store(-1);rebuild();

}
juce::AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new silverK::Processor();}
namespace silverK {
void Processor::applyBufferProfile(){
 pendingHostLatency.store(ownedK?int(ownedK->latency()):ownedDevice?int(ownedDevice->latency()):0);
}
void Processor::notifyHostLatency(){
 // Firmware workers only queue changes. Their caller publishes after joining,
 // outside firmwareLoadMutex as well as controlMutex. Hosts may reenter here.
 if(isFirmwareLoading())return;
 const auto latency=pendingHostLatency.exchange(-1);
 if(latency>=0 && latency!=getLatencySamples())setLatencySamples(latency);
}
void Processor::timerCallback(){
 if(startupPending.exchange(false) && !isFirmwareLoading())beginFirmwareLoad(startupFirmware,true);
 if(isFirmwareLoading()){juce::String error;finishFirmwareLoad(error);if(isFirmwareLoading())return;}
 bool changed=false;
 {
  std::lock_guard lock(controlMutex);
  changed=collectPatchLoads();
 }
 if(changed)updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
 savePreferences();
}
void Processor::setBufferProfile(int profile){
 {
  std::lock_guard guard(controlMutex);
  const auto requested=juce::jlimit(0,3,profile);
  if(requested!=bufferProfile()){
   juce::String error;
   if(!reconfigureEngines(hostRate,hostBlock,requested,ksTwoCoreProcessing(),error))message=error;
  }
 }
 notifyHostLatency();
 savePreferences();
}
bool Processor::setKSTwoCoreProcessing(bool enabled,juce::String& error){
 error.clear();
 {
  std::lock_guard guard(controlMutex);
  if(isFirmwareLoading()){error="Wait for firmware loading to finish";return false;}
  if(enabled==ksTwoCoreProcessing())return true;
  if(ownedDevice){
   if(performanceDiagnostics.mode()){
    performanceDiagnostics.stop();
    performanceStoppedMs=juce::Time::getMillisecondCounterHiRes();
    performanceStopReason="dsp_execution_changed";
   }
   if(!ownedDevice->setParallelDsp(enabled)){error="Could not switch KS DSP workers";return false;}
  }
  twoCoreProcessing.store(enabled);
 }
 notifyHostLatency();
 savePreferences();
 updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
 return true;
}
bool Processor::setKSControllerMode(int mode,juce::String& error){
 error.clear();
 if(mode==ksControllerMode())return true;
 error="This build uses the native KS command controller";
 return false;
}
void Processor::stopEngines(){if(performanceDiagnostics.mode()){performanceDiagnostics.stop();performanceStoppedMs=juce::Time::getMillisecondCounterHiRes();performanceStopReason="engine_rebuilt_or_stopped";}setDrumEdit(false);drumLoadState.store(DrumLoadStatus::Idle);pendingHostLatency.store(-1);resetClock.store(true);device.store(nullptr);kDevice.store(nullptr);while(activeAudio.load())std::this_thread::yield();collectPatchLoads();pendingPatchLoads.clear();ownedDevice.reset();ownedK.reset();}
void Processor::rebuildK(){
 stopEngines();
 kMode.store(true);
 try{
  auto candidate=std::make_unique<KEngine>(kRom,hostRate,hostBlock,kState,CommandEngine::BufferFloors[bufferProfile()]);
  if(kState.empty())kState=candidate->saveArchive();
  publishK(std::move(candidate));
 }catch(const std::exception& error){message=error.what();}
}
void Processor::publishK(std::unique_ptr<KEngine> ready){
 kMode.store(true);multi.store(false);activeMask.store(1);selectedPart.store(0);
 resetPending();
 {std::lock_guard guard(uiMidiMutex);uiMidi.reset();}
 ownedK=std::move(ready);
 applyBufferProfile();
 kWire.fill(0);kMask.fill(0);resetClock.store(true);
 kDevice.store(ownedK.get());
 message="K-Station OS 2.1.01";
}
std::optional<Processor::FirmwareSound> Processor::captureFirmwareSound(juce::String& error){
 CommandEngine* source=ownedK?static_cast<CommandEngine*>(ownedK.get()):ownedDevice.get();
 if(!source)return FirmwareSound{};
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 const auto capture=[&]() -> std::optional<FirmwareSound> {
  if(uiMidi.size())return {};
  auto archive=command::decodeControllerArchive(source->saveArchive());
  if(!archive)return {};
  const auto& savedControls=archive->pendingControl;
  if(!savedControls.empty()){
   // K-Station stores the already applied volume, mod wheel and breath
   // values here for recall, alongside any still-pending program change.
   if(archive->model!=command::Model::K || savedControls.size()%3!=0)return {};
   for(size_t i=0;i<savedControls.size();i+=3){
    const auto cc=savedControls[i+1];
    if((savedControls[i]&0xf0)!=0xb0 || (cc!=7 && cc!=1 && cc!=2))return {};
   }
  }
  if(!archive->ksCheckpoint.empty()){
   auto checkpoint=command::decodeKSControllerCheckpoint(archive->ksCheckpoint);
   if(!checkpoint || checkpoint->reload || !checkpoint->decoder.sysExPrefix.empty())return {};
   const auto editsSound=[](const command::KSQueuedMidi& item){
    const auto status=item.message.front();
    return status==0xf0 || (status&0xf0)==0xc0 ||
           ((status&0xf0)==0xb0 && item.message[1]<120 && item.message[1]!=64);
   };
   if(std::any_of(checkpoint->midi.begin(),checkpoint->midi.end(),editsSound))return {};
   // Keep the stored Single and routing in a saved Performance. A firmware
   // switch starts a new voice/clock session, so never replay its old notes.
   checkpoint->midi.clear();
   checkpoint->decoder={};
   checkpoint->nextByteDelay=0;
   archive->ksCheckpoint=command::encodeKSControllerCheckpoint(*checkpoint);
  }
  return FirmwareSound{command::encodeControllerArchive(*archive),currentPatch,ksDataHash,currentPart()};
 };
 do{
  if(auto sound=capture())return sound;
  if(!source->failure().empty()){
   // Allow another firmware to load even if the previous worker stopped.
   const auto& previous=ownedK?lastKSound:lastKSSound;
   return previous;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(8));
 }while(std::chrono::steady_clock::now()<deadline);
 error="The current sound is still loading. Try switching firmware after it finishes.";
 return {};
}

bool Processor::loadK(const juce::File& file,const kstation::Rom& rom,juce::String& error){
 const auto managed=managedAsset(file,error);
 if(managed==juce::File{})return false;
 auto outgoing=captureFirmwareSound(error);
 if(!outgoing)return false;
 const auto& recalled=ownedK?*outgoing:lastKSound;
 auto archive=recalled.archive;
 auto name=recalled.name;
 const bool fromKS=bool(ownedDevice);
 const bool keep=fromKS && firmwareSwitch()==FirmwareSwitch::KeepPatch && !outgoing->archive.empty();
 try{
  if(keep){
   const auto source=command::decodeControllerArchive(outgoing->archive);
   auto converted=convertPatch(source->activeProgram,Model::KStation,outgoing->part);
   if(converted.data.empty()){error="Cannot convert the current KS sound";return false;}
   const auto initial=kstation::command::readKCommandState(rom,archive);
   auto state=kstation::command::saveKCommandState(initial,initial.selection,initial.initial.expression,{});
   auto target=command::decodeControllerArchive(state);
   target->activeProgram=std::move(converted.data);
   archive=command::encodeControllerArchive(*target);
   name=ksPartName(source->activeProgram,outgoing->part);
  }
  auto candidate=std::make_unique<KEngine>(rom.data(),hostRate,hostBlock,archive,CommandEngine::BufferFloors[bufferProfile()]);
  auto initial=candidate->saveArchive();
  if(initial.empty()){error="Cannot read the loaded K-Station sound";return false;}
  juce::String projection;
  if(keep){
   const auto saved=command::decodeControllerArchive(initial);
   if(!saved || saved->model!=command::Model::K){error="Cannot read the converted K-Station sound";return false;}
   projection=kProgramKey(*saved);
  }
  stopEngines();
  if(fromKS){
   if(!outgoing->archive.empty())lastKSSound=std::move(*outgoing);
   keptKProgramKey=std::move(projection);
  }
  osFile=managed;kRom=rom.data();kState=std::move(initial);currentPatch=std::move(name);
  publishK(std::move(candidate));
  return true;
 }catch(const std::exception& failure){error=failure.what();return false;}
}

bool Processor::loadFirmware(const juce::File& file,juce::String& error){
 const juce::ScopeGuard notify{[this]{notifyHostLatency();}};
 startupPending.store(false);
 error.clear();
 if(!firmwareFileSizeValid(file)){error="Firmware file is too large or empty";return false;}
 auto rom=readK(file);auto ksRom=rom.isValid()?ks::Rom{}:firmware(file);
 if(!rom.isValid() && !ksRom.isValid()){error="Not a supported K-Station 2.1.01 or KS 2.1.02 firmware";return false;}
 {
  std::lock_guard lock(controlMutex);
  if(rom.isValid()? !loadK(file,rom,error) : !loadKS(file,bankFile,flashFile,ksRom.data(),error))return false;
  if(std::none_of(firmwares.begin(),firmwares.end(),[&](const auto& f){return f.file==osFile;}))firmwares.push_back({osFile,rom.isValid()});
  juce::String ignored;
  rememberDataFolder(ignored);
  if(!rememberAssets(error))return false;
 }
 if(!rom.isValid())publishFactoryPresets();
 return true;
}

bool Processor::beginFirmwareLoad(const juce::File& file,bool automatic){
 std::lock_guard guard(firmwareLoadMutex);if(automatic && hostStateRestored.load())return false;
 startupPending.store(false);if(firmwareLoading.exchange(true))return false;
 if(firmwareLoader.joinable())firmwareLoader.join();
 firmwareLoadDone.store(false);firmwareLoadError.clear();
 try{firmwareLoader=std::thread([this,file]{
  try{juce::String error;if(!loadFirmware(file,error))firmwareLoadError=error;}
  catch(const std::exception& e){firmwareLoadError=e.what();}
  firmwareLoadDone.store(true);
 });}catch(const std::exception& e){firmwareLoadError=e.what();firmwareLoadDone.store(true);}
 return true;
}
bool Processor::finishFirmwareLoad(juce::String& error){
 const juce::ScopeGuard notify{[this]{notifyHostLatency();}};
 std::lock_guard loadGuard(firmwareLoadMutex);
 if(!firmwareLoadDone.exchange(false))return false;
 if(firmwareLoader.joinable())firmwareLoader.join();
 error=firmwareLoadError;firmwareLoading.store(false);if(error.isNotEmpty()){std::lock_guard guard(controlMutex);message=error;}++firmwareRevision;return true;
}
bool Processor::scanROMs(const juce::File& folder,juce::String& error){
 error.clear();
 if(!folder.isDirectory()){error="The ROMs folder is missing";return false;}
 auto files=folder.findChildFiles(juce::File::findFiles,true,"*");files.sort();
 std::array<juce::File,2> supported;
 for(const auto& file:files){
  if(!file.hasFileExtension("bin;os;syx;zip") || !firmwareFileSizeValid(file))continue;
  if(supported[0]!=juce::File{} && supported[1]!=juce::File{} && file!=osFile)continue;
  if(readK(file).isValid()){
   if(supported[0]==juce::File{} || file==osFile)supported[0]=file;
  }else if(firmware(file).isValid() && (supported[1]==juce::File{} || file==osFile))supported[1]=file;
 }
 if(!scanKSAssets(folder,files,error))return false;
 firmwares.clear();
 for(size_t i=0;i<supported.size();++i)if(supported[i]!=juce::File{})firmwares.push_back({supported[i],i==0});
 romFolder=folder;
 return true;
}
std::vector<Control> Processor::visibleControls()const{if(isKStation())return kControls();return {std::begin(controls),std::end(controls)};}
const Control* Processor::spec(const juce::String& id)const{static const auto k=kControls();if(isKStation()){for(const auto& c:k)if(id==c.id)return &c;}else for(const auto& c:controls)if(semanticId(id)==semanticId(c.id))return &c;return nullptr;}
juce::String Processor::uiParameterId(const juce::String& id)const{return isKStation()?"k_"+id:currentPart()?"part"+juce::String(currentPart()+1)+"_"+id:id;}
juce::RangedAudioParameter* Processor::uiParameter(const juce::String& id){return parameters.getParameter(uiParameterId(isKStation()?id:semanticId(id)));}
void Processor::processK(KEngine& engine,juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){
 baseLib::performance::Scope controlTiming(&performanceDiagnostics,baseLib::performance::Metric::Controls);
 selectClockSource(hostClock.ownsTransport(),[&](const uint8_t* data,size_t size){engine.midi(data,size);});
 uint8_t ui[4096];size_t count=0;uint16_t item=0;
 while(count<sizeof(ui) && uiMidi.pop(item))ui[count++]=uint8_t(item);
 if(count)engine.midi(ui,count);
 std::array<uint8_t,128> patch;
 if(engine.patch(patch,true)){if(resetWire.exchange(false))kMask.fill(0);for(const auto& c:kstation::controls::Catalog){const size_t slot=c.number+(c.mapping==kstation::controls::Mapping::NRPN?128:0);const auto raw=c.patchOffset>=0?patch[c.patchOffset]:kWire[slot];if(((raw^kWire[slot])&kMask[slot])==0)kMask[slot]=0;kWire[slot]=uint8_t((raw&~kMask[slot])|(kWire[slot]&kMask[slot]));}}
 // Keep the latest values in the existing slots and stagger their writes.
 // Ordinary MIDI and UI packets retain the existing relative block order.
 constexpr auto automationCount=arpAutomationParameters.size();
 bool flushAutomation=false;
 size_t permittedAutomation=automationCount;
 if(engine.arpAutomationInterval){
  flushAutomation=count!=0;
  if(!flushAutomation)
   for(const auto event:midi)if(event.data[0]<0xf8){flushAutomation=true;break;}
  if(!flushAutomation && !engine.arpAutomationSpacing){
   for(size_t n=0;n<automationCount;++n){
    const auto slot=(engine.nextArpAutomation+n)%automationCount;
    if(!engine.arpAutomationRemaining[slot] && pending[ParameterCount+arpAutomationParameters[slot]].load()>=0){
     permittedAutomation=slot;
     break;
    }
   }
  }
 }
 for(size_t i=0;i<KCount;++i){
  size_t automationSlot=automationCount;
  if(engine.arpAutomationInterval){
   const auto it=std::find(arpAutomationParameters.begin(),arpAutomationParameters.end(),i);
   automationSlot=size_t(it-arpAutomationParameters.begin());
   if(automationSlot!=automationCount && !flushAutomation && automationSlot!=permittedAutomation)continue;
  }
  const int value=pending[ParameterCount+i].exchange(-1);
  if(value<0)continue;
  const auto& c=kstation::controls::Catalog[i];
  const bool nrpn=c.mapping==kstation::controls::Mapping::NRPN;
  const size_t slot=c.number+(nrpn?128:0);
  auto& raw=kWire[slot];raw=uint8_t((raw&~c.wireMask)|((value<<c.wireShift)&c.wireMask));kMask[slot]|=c.wireMask;
  const auto status=uint8_t(0xb0|controlChannel.load());
  bool accepted;
  if(nrpn){const uint8_t bytes[]{status,99,0,status,98,uint8_t(c.number),status,6,raw};accepted=engine.midi(bytes,9);}
  else{const uint8_t bytes[]{status,uint8_t(c.number),raw};accepted=engine.midi(bytes,3);}
  if(accepted && automationSlot!=automationCount){
   engine.arpAutomationRemaining[automationSlot]=engine.arpAutomationInterval;
   engine.arpAutomationSpacing=engine.arpAutomationInterval/8;
   engine.nextArpAutomation=(automationSlot+1)%automationCount;
  }
 }
 if(engine.arpAutomationInterval){
  const auto samples=unsigned(audio.getNumSamples());
  engine.arpAutomationSpacing-=std::min(engine.arpAutomationSpacing,samples);
  for(auto& remaining:engine.arpAutomationRemaining)remaining-=std::min(remaining,samples);
 }
 sendTimedMidi(midi,hostClock.ownsTransport(),&hostClock,[&](const uint8_t* data,size_t size,int offset){engine.midi(data,size,uint32_t(offset));});
 controlTiming.finish();baseLib::performance::Scope renderTiming(&performanceDiagnostics,baseLib::performance::Metric::AudioRender,uint64_t(audio.getNumSamples()));
 engine.render({audio.getWritePointer(0),audio.getWritePointer(1),nullptr,nullptr},audio.getNumSamples(),isNonRealtime(),getTotalNumInputChannels()?audio.getReadPointer(0):nullptr,getTotalNumInputChannels()>1?audio.getReadPointer(1):nullptr);for(int ch=2;ch<audio.getNumChannels();++ch)audio.clear(ch,0,audio.getNumSamples());midi.clear();
}
Processor::DrumEditInfo Processor::drumEditInfo()const{
 std::lock_guard lock(controlMutex);
 if(isFirmwareLoading() || !ownedDevice)return {};
 const auto state=ownedDevice->editorState(false);
 if(!state)return {};
 const auto part=state->performance?currentPart():0;
 const auto& voice=state->parts[part];
 if(voice.program.bank<5 || voice.program.bank>8 || voice.program.program>=49)return {};
 const auto& editor=state->drumEditor;
 const bool editing=drumEditEnabled() && editor.slot==(state->performance?part+1:0);
 const auto* patch=editing?editor.voice.data():voice.parameters.data();
 return {voice.program.bank,DrumGridEdit::FirstNote+(editing?editor.program:voice.program.program),
         juce::String(command::editorProgramName(patch+151).data()).trim(),editing};
}
void Processor::setDrumEdit(bool enabled){
 if(enabled){
  if(drumEditInfo().bank)requestedDrumEditSlot.store(drumEditorSlot());
 }else{
  requestedDrumEditSlot.store(command::KSDrumEditMode::Off);
  drumParameterAddress.store(0);
 }
}
Processor::DrumSequencerInfo Processor::drumSequencerInfo(){
 std::lock_guard lock(controlMutex);
 collectPatchLoads();
 DrumSequencerInfo info;
 if(!ownedDevice || isFirmwareLoading())return info;
 const auto state=ownedDevice->editorState(false);
 if(!state)return info;
 const auto part=state->performance?currentPart():0;
 const auto& voice=state->parts[part];
 const auto& patch=voice.parameters;
 info.slot=uint8_t(state->performance?part+1:0);
 info.sequences=userDrums[info.slot];
 if(voice.program.bank<5 || voice.program.bank>8)return info;
 info.bank=voice.program.bank;
 if(const auto stored=ownedDevice->drumBank(info.bank))
  for(size_t i=0;i<info.names.size();++i)
   info.names[i]=juce::String(command::editorProgramName((*stored)[i].data()+151).data()).trim();
 if(voice.program.program<info.names.size())
  info.names[voice.program.program]=juce::String::fromUTF8(reinterpret_cast<const char*>(patch.data()+151),16).trim();
 return info;
}
Processor::DrumLoadStatus Processor::drumMapLoadStatus()const{
 const auto previous=drumLoadState.load();
 if(previous!=DrumLoadStatus::Loading)return previous;
 std::unique_lock lock(controlMutex,std::try_to_lock);
 if(!lock.owns_lock() || !ownedDevice)return previous;
 const auto current=ownedDevice->drumLoadStatus();
 drumLoadState.store(current);
 return current;
}
Processor::DrumLoadStatus Processor::ksPresetLoadStatus()const{
 std::unique_lock lock(controlMutex,std::try_to_lock);
 if(!lock.owns_lock())return DrumLoadStatus::Loading;
 return ownedDevice?ownedDevice->presetLoadStatus():DrumLoadStatus::Idle;
}
std::optional<Processor::DrumPlayhead> Processor::drumPlayhead(uint8_t slot)const{
 std::unique_lock lock(controlMutex,std::try_to_lock);
 if(!lock.owns_lock())return {};
 if(!ownedDevice || isFirmwareLoading() || slot!=drumEditorSlot())return DrumPlayhead{};
 return ownedDevice->drumPlayhead(slot?slot-1:0);
}
bool Processor::editDrumGrid(DrumGridEdit edit,juce::String& error){
 std::unique_lock lock(controlMutex);
 collectPatchLoads();
 if(!pendingPatchLoads.empty()){error="Wait for the preset to finish loading";return false;}
 if(!edit.valid() || !ownedDevice || isFirmwareLoading()){error="KS Drum Map is unavailable";return false;}
 const auto state=ownedDevice->editorState(false);
 if(!state){error="KS Drum Map is unavailable";return false;}
 const auto part=state->performance?currentPart():0;
 const auto selectedBank=state->parts[part].program.bank;
 const bool selecting=edit.kind==DrumGridEdit::Kind::SelectUser;
 if(edit.slot!=(state->performance?part+1:0) || (edit.needsDrumMap() && (selectedBank<5 || selectedBank>8))){
  error="Select the part's Drum Map and reopen the sequencer";return false;
 }
 if(selecting && userDrums[edit.slot].selected==(edit.value!=0))return true;
 if(!ownedDevice->editDrumGrid(edit)){error="KS is busy. Try the edit again.";return false;}
 userDrums[edit.slot].apply(edit);
 if(selecting) {
  const auto bit=uint8_t(1u<<edit.slot);
  if(edit.value)userDrumMask.fetch_or(bit);
  else userDrumMask.fetch_and(uint8_t(~bit));
 }
 lock.unlock();
 updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
 return true;
}
bool Processor::selectDrumRhythm(uint8_t slot,uint8_t rhythm,juce::String& error){
 if(rhythm>UserDrumRhythm){error="Invalid drum rhythm";return false;}
 const bool user=rhythm==UserDrumRhythm;
 if(!editDrumGrid({DrumGridEdit::Kind::SelectUser,slot,0,0,uint8_t(user)},error))return false;
 const auto prefix=slot>1?"part"+juce::String(slot)+"_":juce::String{};
 const auto set=[&](const char* name,int value){
  auto* parameter=parameters.getParameter(prefix+name);
  const auto normalized=parameter->convertTo0to1(float(value));
  if(parameter->getValue()==normalized)return;
  parameter->beginChangeGesture();
  parameter->setValueNotifyingHost(normalized);
  parameter->endChangeGesture();
 };
 if(user) {
  set("arp_pattern",7);
  set("arp_on",1);
 }else set("arp_rhythm",rhythm);
 return true;
}
bool Processor::activatePatch(const pluginLib::patchDB::PatchPtr& patch,juce::String& error){return activatePatchForPart(patch,currentPart(),error);}
bool Processor::collectPatchLoads(){
 if(pendingPatchLoads.empty())return false;
 bool changed=false;
 while(!pendingPatchLoads.empty()){
  const auto& pending=*pendingPatchLoads.front();
  const auto status=pending.status.load(std::memory_order_acquire);
  if(status==DrumLoadStatus::Loading){
   if(!ownedDevice || !ownedDevice->failure().empty())pendingPatchLoads.clear();
   break;
  }
  if(status==DrumLoadStatus::Applied){
   const bool whole=pending.appliedSlots==30;
   for(size_t slot=0;slot<userDrums.size();++slot)if(pending.appliedSlots&(1u<<slot)){
    userDrums[slot]=pending.sequences[whole?slot:0];
    const auto bit=uint8_t(1u<<slot);
    if(userDrums[slot].selected)userDrumMask.fetch_or(bit);
    else userDrumMask.fetch_and(uint8_t(~bit));
   }
   changed=true;
  }
  pendingPatchLoads.erase(pendingPatchLoads.begin());
 }
 return changed;
}

bool Processor::activatePatchForPart(const pluginLib::patchDB::PatchPtr& patch,unsigned part,juce::String& error){
 if(part>=ks::PartCount){error="Invalid part";return false;}
 if(isFirmwareLoading()){error="Wait for firmware loading to finish";return false;}
 if(!patch || !firmwareReady()){error="Load firmware first";return false;}
 const auto native=nativePatch(patch->sysex);
 const auto source=patchDump(*patch);
 if(native.extended() && isKStation()){error="KS+ presets require KS firmware";return false;}
 std::shared_ptr<command::KSDesktopPatchExtras> extras;
 if(native && !isKStation()){
  extras=std::make_shared<command::KSDesktopPatchExtras>();
  if(!decodePatchDrums(native,extras->sequences)){error="Damaged Silver-K sequencer data";return false;}
  for(size_t i=0;i<native.drumMaps.size();++i)if(const auto map=native.drumMaps[i];!map.empty()){
   auto& bank=extras->drumMaps[i].emplace();
   for(size_t n=0;n<bank.size();++n)std::copy_n(map.begin()+n*270+13,256,bank[n].begin());
  }
 }
 if(ksDrum(source.data(),source.size())){
  if(isKStation()){error="Drum Maps require KS firmware";return false;}
  if(!validPatch(source.data(),source.size())){error="Malformed KS Drum Map";return false;}
  std::lock_guard lock(controlMutex);if(!ownedDevice){error="Load KS firmware first";return false;}
  if(const auto failure=ownedDevice->failure();!failure.empty()){error=juce::String(failure);return false;}
  const auto bankNumber=source[11];const bool complete=drumMap(source.data(),source.size());
  if(!complete) {
   const auto stored=ownedDevice->drumBank(bankNumber);
   if(!stored || !std::equal(source.begin()+13,source.begin()+269,(*stored)[source[12]].begin())){
    error="Import the complete Drum Map (49 sounds + effects), or put its KS factory Data in ROMs and rescan";return false;
   }
  }
  command::KSDesktopDrumMap request;
  request.selection={bankNumber,complete?native.drumProgram:source[12]};
  request.part=uint8_t(part);
  request.performance=ownedDevice->fullSnapshot().performance;
  request.complete=complete;
  for(size_t n=0;n<(complete?request.records.size():1);++n)
   std::copy_n(source.begin()+n*270+13,256,request.records[n].begin());
  {
   std::lock_guard producer(uiMidiMutex);
   if(!ownedDevice->loadDrumMap(request,[&](uint8_t index){return uiMidi.push(uint16_t(UiSoundLoadToken+index));},extras)){
    error="Drum Map queue is full";return false;
   }
  }
  collectPatchLoads();
  if(extras)pendingPatchLoads.push_back(std::move(extras));
  drumLoadState.store(DrumLoadStatus::Loading);
  resetPending();if(part==currentPart())currentPatch=patchName(*patch);error.clear();return true;
 }
 auto converted=convertPatch(source.data(),source.size(),isKStation()?Model::KStation:Model::KS,part);if(converted.data.empty()){error="Unsupported or malformed program dump";return false;}
 // V-Station banks can drive the K fixed-point effect sum to its ceiling.
 // Reserve firmware program level; preserve the authored filter/drive and the
 // untouched library payload. A post-output limiter cannot undo that clipping.
 if(isKStation() && patchModel(*patch)==Model::VStation){converted.data[13+125]=uint8_t(std::max(0,int(converted.data[13+125])-6));converted.changes.add("V-Station level: -6 dB");}
 if(!isKStation() && patchModel(*patch)==Model::VStation){std::fill_n(converted.data.begin()+13+151,16,' ');const juce::String name(patch->getName());for(int n=0;n<std::min(16,name.length());++n){const auto c=name[n];converted.data[13+151+n]=uint8_t(c>=32 && c<127?c:'?');}}
 bool ok=false;
 {
  std::lock_guard lock(controlMutex);
  if(ownedK){
   if(const auto failure=ownedK->failure();!failure.empty()){error=juce::String(failure);return false;}
   ok=queueUiMidi(converted.data.data(),converted.data.size());
   if(ok){keptKProgramKey.clear();resetPending();}
  }else if(ownedDevice){
   if(const auto failure=ownedDevice->failure();!failure.empty()){error=juce::String(failure);return false;}
   command::KSDesktopPreset request;
   request.wholePerformance=converted.data.size()==1038;
   request.part=uint8_t(part);
   for(size_t n=0;n<(request.wholePerformance?request.records.size():1);++n)
    std::copy_n(converted.data.begin()+13+n*256,256,request.records[n].begin());
   {
    std::lock_guard producer(uiMidiMutex);
    ok=ownedDevice->loadPreset(request,[&](uint8_t index){return uiMidi.push(uint16_t(UiSoundLoadToken+index));},extras);
   }
   if(ok){
    collectPatchLoads();
    if(extras)pendingPatchLoads.push_back(std::move(extras));
    if(request.wholePerformance)resetPending();
    else{
     const auto start=part*controls.size();
     for(size_t n=start;n<start+controls.size();++n){pending[n].store(-1);awaiting[n].store(-1);}
     resetWire.store(true);
    }
   }
  }
  if(ok && part==currentPart())currentPatch=patchName(*patch);
  if(ok)drumLoadState.store(DrumLoadStatus::Idle);
 }
 if(!ok){error="Preset queue is full";return false;}error=converted.changes.joinIntoString("\n");return true;
}
namespace {
std::vector<uint8_t> kProgramDump(const std::array<uint8_t,128>& patch){
 std::vector<uint8_t> data{0xf0,0,0x20,0x29,1,0x41,0x7f,0,0,0x11,1,0,0};
 data.insert(data.end(),patch.begin(),patch.end());
 data.push_back(0xf7);
 return data;
}

std::vector<uint8_t> drumMapDump(uint8_t bank,const command::KSDrumBank& records){
 std::vector<uint8_t> data;
 data.reserve(DrumMapSize);
 for(uint8_t n=0;n<records.size();++n){
  data.insert(data.end(),{0xf0,0,0x20,0x29,1,0x42,0x7f,1,1,0x11,2,bank,n});
  data.insert(data.end(),records[n].begin(),records[n].end());
  data.push_back(0xf7);
 }
 return data;
}

void nameKSDump(std::vector<uint8_t>& dump,const juce::String& name){
 if(modelOf(dump)!=Model::KS)return;
 const auto offset=dump.size()==DrumMapSize?49*270+13+151:dump.size()==1038?13+192:13+151;
 std::array<uint8_t,16> text;
 text.fill(' ');
 int i=0;
 for(auto each=name.getCharPointer();!each.isEmpty() && i<int(text.size());++i){
  const auto c=each.getAndAdvance();
  text[size_t(i)]=c>=32 && c<127?uint8_t(c):uint8_t('?');
 }
 std::copy(text.begin(),text.end(),dump.begin()+offset);
}
}

bool Processor::saveUserPatch(const juce::File& file,PatchSaveScope scope,juce::String& error){
 const auto userFolder=userPatchesFolder();
 if(!file.isAChildOf(userFolder) || !file.hasFileExtension(NativePatchExtension)){
  error="Save Silver-K presets in Patches/User";
  return false;
 }
 std::vector<uint8_t> dump;
 std::array<std::vector<uint8_t>,4> maps;
 auto grids=std::make_unique<command::KSUserDrumPatterns>();
 bool includesGrid=false;
 uint8_t drumProgram=0;
 {
  std::lock_guard lock(controlMutex);
  collectPatchLoads();
  if(isFirmwareLoading() || !pendingPatchLoads.empty()){
   error="Wait for the preset to finish loading";
   return false;
  }
  const bool whole=scope==PatchSaveScope::Performance;
  if(ownedK && !whole){
   std::array<uint8_t,128> patch;
   if(!ownedK->patch(patch)){error="Sound snapshot unavailable";return false;}
   dump=kProgramDump(patch);
  }else if(ownedDevice){
   const auto state=ownedDevice->editorState(false);
   if(!state || state->loadingProgram || ownedDevice->presetLoadStatus()==DrumLoadStatus::Loading ||
      ownedDevice->drumLoadStatus()==DrumLoadStatus::Loading){
    error="Wait for the KS sound to finish loading";
    return false;
   }
   if(whole && !state->performance){error="Enable Performance before saving it";return false;}
   const size_t selected=state->performance?currentPart():0;
   ks::State sound;
   sound.performance=whole;
   const auto first=whole?0:selected;
   const auto count=whole?ks::PartCount:1;
   for(size_t i=0;i<count;++i){
    const auto part=first+i;
    const auto& current=state->parts[part];
    auto& record=sound.parts[i];
    record=current.parameters;
    ks::packMirrors(record);
    const auto bank=current.program.bank,program=current.program.program;
    record[187]=bank;
    record[188]=program;
    if(bank<5 || bank>8 || program>=50 || (!whole && program==49))continue;
    if(program<49){
     auto& grid=(*grids)[whole?part+1:0];
     grid=userDrums[state->performance?part+1:0];
     includesGrid|=hasDrumEdits(grid);
    }
    if(!maps[bank-5].empty())continue;
    auto stored=ownedDevice->drumBank(bank);
    if(!stored){error="Drum Map snapshot unavailable";return false;}
    if(!whole){
     // Carry the live patch and its shared effects together with the bank.
     std::copy_n(record.begin(),179,(*stored)[program].begin());
     std::copy_n(record.begin()+84,41,(*stored)[49].begin()+84);
     (*stored)[49][126]=record[126];
     drumProgram=program;
     dump=drumMapDump(bank,*stored);
    }else maps[bank-5]=drumMapDump(bank,*stored);
   }
   if(whole)sound.parts[0][190]=state->activeMask;
   if(dump.empty())dump=ks::stateMessage(sound);
  }else{
   error=whole?"Enable KS Performance before saving it":"Load firmware first";
   return false;
  }
 }
 const auto name=file.getFileNameWithoutExtension();
 nameKSDump(dump,name);
 const auto bytes=encodeNativePatch(dump,name,includesGrid?grids.get():nullptr,maps,drumProgram);
 if(bytes.empty()){error="Could not encode current sound as a Silver-K preset";return false;}
 if(!file.replaceWithData(bytes.data(),bytes.size())){error="Could not write "+file.getFileName();return false;}
 patchLibrary.indexUserPreset(file,userFolder);
 error.clear();
 return true;
}

bool Processor::saveCurrentPatch(const juce::File& file,juce::String& error){std::lock_guard lock(controlMutex);std::vector<uint8_t> data;if(ownedDevice){
 collectPatchLoads();
 if(!pendingPatchLoads.empty()){error="Wait for the preset to finish loading";return false;}
 const auto state=ownedDevice->editorState(false);
 if(!state){error="Snapshot unavailable; try again";return false;}
 const auto& current=state->parts[state->performance?currentPart():0];
 const auto bank=current.program.bank,slot=current.program.program;
 const bool selectedMap=bank>=5 && bank<=8 && slot<49;
 for(size_t part=0;part<(state->performance?ks::PartCount:1);++part){
  const auto& voice=state->parts[part];
  if(voice.program.bank>=5 && voice.program.bank<=8 && voice.program.program<49 &&
     (!selectedMap || part==(state->performance?currentPart():0)) &&
     hasDrumEdits(userDrums[state->performance?part+1:0])){
   error="KS+ presets must be saved as a Silver-K preset with the drum sequence";
   return false;
  }
 }
 if(bank>=5 && bank<=8 && slot<49){
  const auto stored=ownedDevice->saveFlash();const size_t offset=0x20000-0x1fc00+(400+(bank-5)*50)*256;
  if(stored.size()<offset+50*256){error="Drum Map snapshot unavailable";return false;}
  data.reserve(DrumMapSize);
  for(unsigned n=0;n<50;++n){
   data.insert(data.end(),{0xf0,0,0x20,0x29,1,0x42,0x7f,1,1,0x11,2,uint8_t(bank),uint8_t(n)});
   const auto begin=data.size();data.insert(data.end(),stored.begin()+offset+n*256,stored.begin()+offset+(n+1)*256);
   // The working bank holds the voice that is played for each note.
   if(n==49){std::copy(current.parameters.begin()+84,current.parameters.begin()+125,data.begin()+begin+84);data[begin+126]=current.parameters[126];}
   data.push_back(0xf7);
  }
 }else data=ownedDevice->dumpProgram();
 }else if(ownedK){std::array<uint8_t,128> patch;if(!ownedK->patch(patch)){error="Snapshot unavailable; try again";return false;}data=kProgramDump(patch);}if(data.empty() || !file.replaceWithData(data.data(),data.size())){error="Cannot save current sound";return false;}patchLibrary.addPath(file);error.clear();return true;}
}
