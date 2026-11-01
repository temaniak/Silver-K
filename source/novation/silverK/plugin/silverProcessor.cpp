#include "silverProcessor.h"
#include "../../ks/ksLib/ksDemo.h"
#include "silverEditor.h"
#include "silverKEngine.h"
#include "kstationLib/kstationRomLoader.h"
#include "ksLib/ksRom.h"
#include "baseLib/sha256.h"
namespace silverK {
using namespace ksPlugin;
namespace {
thread_local const Processor* syncingProcessor=nullptr;
std::vector<uint8_t> bytes(const juce::File& f){juce::MemoryBlock b;if(!f.loadFileAsData(b))return {};auto* p=static_cast<const uint8_t*>(b.getData());return {p,p+b.getSize()};}
kstation::Rom readK(const juce::File& file){if(!file.hasFileExtension("zip"))return kstation::Rom(file.getFullPathName().toStdString());juce::ZipFile zip(file);for(int i=0;i<zip.getNumEntries();++i){const auto* e=zip.getEntry(i);if(e->uncompressedSize>2*1024*1024 || !juce::File(e->filename).hasFileExtension("syx;os;bin"))continue;std::unique_ptr<juce::InputStream> in(zip.createStreamForEntry(i));juce::MemoryBlock b;if(!in)continue;in->readIntoMemoryBlock(b,2*1024*1024);auto* p=static_cast<const uint8_t*>(b.getData());kstation::Rom rom(std::vector<uint8_t>(p,p+b.getSize()),e->filename.toStdString());if(rom.isValid())return rom;}return {};}
ks::Rom firmware(const juce::File& file) {
 if (!file.hasFileExtension("zip")) return ks::Rom(file.getFullPathName().toStdString());
 juce::ZipFile archive(file);
 for (int i=0;i<archive.getNumEntries();++i) {
  const auto* entry=archive.getEntry(i);
  if (!entry || entry->uncompressedSize>131072 || !entry->filename.endsWithIgnoreCase(".syx")) continue;
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
 initializeDataFolder();startTimerHz(10);
}
Processor::~Processor(){stopTimer();if(firmwareLoader.joinable())firmwareLoader.join();firmwareLoading.store(false);savePreferences();for(const auto& id:parameterIds)parameters.removeParameterListener(id,this);stopEngines();}
bool Processor::isBusesLayoutSupported(const BusesLayout& l)const{
 if(l.inputBuses.size()>1 || (l.inputBuses.size()==1 && !l.inputBuses[0].isDisabled() && l.inputBuses[0]!=juce::AudioChannelSet::mono() && l.inputBuses[0]!=juce::AudioChannelSet::stereo()) || l.outputBuses.isEmpty())return false;
 // JUCE's standalone player describes physical device channels as one bus.
 // Plugins retain two independently enabled stereo buses for the DAW.
 if(wrapperType==wrapperType_Standalone)return l.outputBuses.size()<=2 && (l.outputBuses[0].size()==2 || l.outputBuses[0].size()==4) && (l.outputBuses.size()==1 || l.outputBuses[1].isDisabled());
 return l.outputBuses.size()==2 && l.outputBuses[0]==juce::AudioChannelSet::stereo() && (l.outputBuses[1].isDisabled() || l.outputBuses[1]==juce::AudioChannelSet::stereo());
}
void Processor::parameterChanged(const juce::String& id,float v){
 if(syncingProcessor==this)return;
 for(size_t n=0;n<TotalCount;++n)if(id==parameterIds[n]){const int value=int(std::round(v));awaiting[n].store(value);editTime[n].store(juce::Time::getMillisecondCounter());pending[n].store(value);}
}
void Processor::refreshControls(){
 if(isFirmwareLoading())return;
 std::lock_guard guard(controlMutex);
 if(ownedK){std::array<uint8_t,128> patch;std::array<uint8_t,2048> cc;uint64_t rev;if(!ownedK->device.getControlSnapshot(patch,cc,rev))return;syncingProcessor=this;
 for(size_t i=0;i<KCount;++i){const auto& c=kstation::controls::Catalog[i];if(c.patchOffset<0)continue;const size_t n=ParameterCount+i;int v=(patch[c.patchOffset]&c.patchMask)>>c.patchShift;v=juce::jlimit(int(c.minimum),int(c.maximum),v);const int requested=awaiting[n].load();if(requested>=0){if(v==requested && pending[n].load()<0)awaiting[n].store(-1);else if(uint32_t(juce::Time::getMillisecondCounter()-editTime[n].load())<2000 || pending[n].load()>=0)continue;else awaiting[n].store(-1);}auto* p=parameters.getParameter(parameterIds[n]);if(p->convertFrom0to1(p->getValue())!=v)p->setValueNotifyingHost(p->convertTo0to1(v));}syncingProcessor=nullptr;return;}
 if(!ownedDevice)return;const auto state=ownedDevice->fullSnapshot();multi.store(state.performance);activeMask.store(state.activeMask);for(unsigned part=0;part<ks::PartCount;++part)partChannels[part].store(state.parts[part][181]&15);if(!state.performance)selectedPart.store(0);
 const auto drumRevision=ownedDevice->drumEditRevision();
 if(drumRevision!=seenDrumRevision){seenDrumRevision=drumRevision;resetWire.store(true);for(size_t n=0;n<ParameterCount;++n)if(pending[n].load()<0)awaiting[n].store(-1);}
 syncingProcessor=this;
 for(size_t n=0;n<(state.performance?ParameterCount:controls.size());++n){const auto& c=controls[n%controls.size()];const auto& patch=state.parts[n/controls.size()];if(c.patchOffset<0)continue;
  const auto value=juce::jlimit(int(c.minimum),int(c.maximum),int(decode(c,patch.data())));
  const auto wanted=awaiting[n].load();
  if(wanted>=0){if(value==wanted && pending[n].load()<0)awaiting[n].store(-1);else if(uint32_t(juce::Time::getMillisecondCounter()-editTime[n].load())<2000 || pending[n].load()>=0)continue;else awaiting[n].store(-1);}
  if(parameters.getRawParameterValue(parameterIds[n])->load()!=value){auto* parameter=parameters.getParameter(parameterIds[n]);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
 }syncingProcessor=nullptr;
}
void Processor::rebuild(){
 kMode.store(false);
 controlChannel.store(flash.size()>0x1ff08?flash[0x1ff08]&15:0);
 stopEngines();
 if(os.empty()||bank.size()!=ks::Board::WaveSize||flash.size()!=0x80000)return;
 auto d=std::make_unique<ks::Device>(os,bank,flash,hostRate,hostBlock,programToRestore,&performanceDiagnostics,ksController.load(),ksTwoCoreProcessing());
 if(!d->valid()){message=d->error();return;}
 ownedDevice=std::move(d);applyBufferProfile();auto* dReady=ownedDevice.get();
 const auto state=dReady->fullSnapshot();wireShadow=state.parts;for(auto& mask:wirePendingMask)mask.fill(0);multi.store(state.performance);activeMask.store(state.activeMask);
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
 auto waves=bytes(b);std::vector<uint8_t> storage;
 const auto dataFiles=f==flashFile?selectedKSDataFiles():std::vector<juce::File>{f};
 if(!readKSData(dataFiles,storage,e))return false;
 if(waves.size()!=ks::Board::WaveSize){e="Select the required KS Wave ROM (129024 bytes) in Setup > Wave ROM.";return false;}
 const auto managedOS=managedAsset(o,e),managedWave=managedAsset(b,e);if(managedOS==juce::File{} || managedWave==juce::File{})return false;
 std::vector<juce::File> managedData;for(const auto& file:dataFiles){auto saved=managedAsset(file,e);if(saved==juce::File{})return false;managedData.push_back(saved);}
 flashFiles=std::move(managedData);
 programToRestore.clear();os=rom.data();bank=std::move(waves);flash=std::move(storage);osFile=managedOS;bankFile=managedWave;flashFile=flashFiles.front();
 placeholder=std::all_of(bank.begin(),bank.end(),[](uint8_t v){return v==0;});buildLibrary();rebuild();if(!device.load()){e=message;return false;}return true;
}
void Processor::prepareToPlay(double r,int b){
 const juce::ScopeGuard notify{[this]{notifyHostLatency();}};
 std::lock_guard guard(controlMutex);
 if(hostRate==r && hostBlock==b && firmwareReady())return;
 if(ownedK){std::vector<uint8_t> saved;if(ownedK->device.getState(saved,synthLib::StateTypeGlobal))kState=std::move(saved);hostRate=r;hostBlock=b;rebuildK();return;}
 if(ownedDevice){programToRestore=ownedDevice->dumpProgram();auto saved=ownedDevice->saveFlash();if(saved.size()==0x60000-0x1fc00)std::copy(saved.begin(),saved.end(),flash.begin()+0x1fc00);}
 hostRate=r;hostBlock=b;if(!os.empty())rebuild();
}
void Processor::prepareHostClock(int samples){
 if(resetClock.exchange(false)){hostClock.reset();clockSourceMode=-1;externalNrpn.fill(0);}
 activeClockSource=requestedClockSource.load();
 HostClock::Position position;
 if(activeClockSource==ClockSource::Host)if(auto* playHead=getPlayHead())if(auto info=playHead->getPosition()){
  if(auto bpm=info->getBpm()){position.available=true;position.bpm=*bpm;}
  position.playing=info->getIsPlaying();
  if(auto ppq=info->getPpqPosition()){position.hasPpq=true;position.ppq=*ppq;}
 }
 hostClock.begin(position,hostRate,samples);
}
void Processor::setClockSource(ClockSource source){
 if(source!=ClockSource::Host && source!=ClockSource::Internal && source!=ClockSource::Midi)return;
 if(requestedClockSource.exchange(source)!=source)
  updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
}
void Processor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){
 using namespace baseLib::performance;
 Scope callbackTiming(&performanceDiagnostics,Metric::Callback,uint64_t(audio.getNumSamples()),hostRate>0?uint64_t(audio.getNumSamples()*1.0e9/hostRate):0);
 const auto diagnosticEpoch=performanceDiagnostics.epoch();
 if(diagnosticEpoch){performanceDiagnostics.add(Metric::HostFrames,diagnosticEpoch,uint64_t(audio.getNumSamples()));performanceDiagnostics.add(Metric::OfflineCallbacks,diagnosticEpoch,isNonRealtime()?1:0);}
 juce::ScopedNoDenormals no;activeAudio.fetch_add(1);
 struct Guard{std::atomic<unsigned>& n;~Guard(){--n;}} guard{activeAudio};if(auto* k=kDevice.load()){observeAudioBlock(audio.getNumSamples());if(buffering.load()==0)k->device.ensureAudioBlock(uint32_t(audio.getNumSamples()),float(k->rate));prepareHostClock(audio.getNumSamples());processK(*k,audio,midi);return;}auto* d=device.load();
 if(!d){audio.clear();midi.clear();return;}
 Scope controlTiming(&performanceDiagnostics,Metric::Controls);
 observeAudioBlock(audio.getNumSamples());if(buffering.load()==0)d->ensureAudioBlock(unsigned(audio.getNumSamples()));
 prepareHostClock(audio.getNumSamples());
 selectClockSource([&](const uint8_t* data,size_t size){d->midi(data,size);});
 uint8_t uiBytes[4096];size_t uiCount=0;while(uiCount<sizeof(uiBytes) && uiMidi.pop(uiBytes[uiCount]))++uiCount;if(uiCount)d->midi(uiBytes,uiCount);
 const auto state=d->fullSnapshot();
 if(resetWire.exchange(false))for(auto& mask:wirePendingMask)mask.fill(0);
 for(unsigned part=0;part<ks::PartCount;++part){auto& shadow=wireShadow[part];auto& mask=wirePendingMask[part];const auto& patch=state.parts[part];
  for(unsigned n=0;n<256;++n){if(((patch[n]^shadow[n])&mask[n])==0)mask[n]=0;shadow[n]=uint8_t((patch[n]&~mask[n])|(shadow[n]&mask[n]));}
  uint8_t channel=state.performance?(patch[181]&15):controlChannel.load();
  for(size_t index=0;index<controls.size();++index){const size_t n=part*controls.size()+index;int value=pending[n].exchange(-1);if(value<0 || (!state.performance && part))continue;const auto& c=controls[index];if(n==0)volume.store(uint8_t(value));
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

 sendTimedMidi(midi,[&](const uint8_t* data,size_t size,int offset){
  if(size==3 && data[0]==0xb0 && data[1]==7)volume.store(data[2]);
  const auto part=state.performance?currentPart():0;const auto& patch=state.parts[part];
  const auto channel=state.performance?(patch[181]&15):controlChannel.load();
  if(drumEditing.load() && size==3 && data[0]==(0x90|channel) && data[2] && data[1]>=36 && data[1]<=84 && patch[187]>=5 && patch[187]<=8)d->selectDrumNote(part,data[1],uint64_t(offset));
  d->midi(data,size,uint64_t(offset));
 });
 std::array<float*,ks::OutputCount> channels{audio.getWritePointer(0),audio.getWritePointer(1),nullptr,nullptr};
 if(wrapperType==wrapperType_Standalone && getMainBusNumOutputChannels()==4){channels[2]=audio.getWritePointer(2);channels[3]=audio.getWritePointer(3);}
 else if(getBusCount(false)>1 && getBus(false,1)->isEnabled()){auto aux=getBusBuffer(audio,false,1);channels[2]=aux.getWritePointer(0);channels[3]=aux.getWritePointer(1);}
 controlTiming.finish();Scope renderTiming(&performanceDiagnostics,Metric::AudioRender,uint64_t(audio.getNumSamples()));
 d->process(channels,audio.getNumSamples(),isNonRealtime(),getTotalNumInputChannels()?audio.getReadPointer(0):nullptr,getTotalNumInputChannels()>1?audio.getReadPointer(1):nullptr);midi.clear();
}
juce::String Processor::status()const{if(isFirmwareLoading())return "Loading firmware...";std::lock_guard guard(controlMutex);return message;}
uint64_t Processor::underruns()const{std::lock_guard guard(controlMutex);return ownedK?ownedK->device.getUnderrunCount():ownedDevice?ownedDevice->underruns():0;}
void Processor::getStateInformation(juce::MemoryBlock& out){std::lock_guard guard(controlMutex);
 auto state=parameters.copyState();state.setProperty("ksDataFiles",juce::JSON::toString(ksDataReferences()),nullptr);state.setProperty("clockSource",int(clockSource()),nullptr);state.setProperty("bufferProfile",buffering.load(),nullptr);state.setProperty("ksControllerMode",ksControllerMode(),nullptr);state.setProperty("dataFolder",dataFolder.getFullPathName(),nullptr);for(const auto& asset:{std::pair<const char*,juce::File>{"os",osFile},{"bank",bankFile},{"flash",flashFile}})if(asset.second.isAChildOf(dataFolder))state.setProperty(juce::String(asset.first)+"Relative",asset.second.getRelativePathFrom(dataFolder),nullptr);state.setProperty("romFolder",romFolder.getFullPathName(),nullptr);state.setProperty("skinChoice",skinChoice,nullptr);state.setProperty("editorScale",editorScale.load(),nullptr);state.setProperty("browserState",browserState,nullptr);state.setProperty("customSkin",customSkin,nullptr);state.setProperty("currentPatch",currentPatch,nullptr);state.setProperty("editorChannel",int(controlChannel.load()),nullptr);state.setProperty("kModel",isKStation(),nullptr);state.setProperty("bank",bankFile.getFullPathName(),nullptr);state.setProperty("flash",flashFile.getFullPathName(),nullptr);
 state.setProperty("ksTwoCoreProcessing",ksTwoCoreProcessing(),nullptr);
 if(ownedK){std::vector<uint8_t> saved;ownedK->device.getState(saved,synthLib::StateTypeGlobal);state.setProperty("os",osFile.getFullPathName(),nullptr);state.setProperty("osHash",juce::String(baseLib::SHA256(kRom).toString()),nullptr);state.setProperty("underruns",juce::int64(ownedK->device.getUnderrunCount()),nullptr);state.setProperty("kState",juce::MemoryBlock(saved.data(),saved.size()).toBase64Encoding(),nullptr);if(auto xml=state.createXml())copyXmlToBinary(*xml,out);return;}
 state.setProperty("os",osFile.getFullPathName(),nullptr);state.setProperty("bank",bankFile.getFullPathName(),nullptr);state.setProperty("flash",flashFile.getFullPathName(),nullptr);state.setProperty("osHash",os.empty()?juce::String{}:juce::String(baseLib::SHA256(os).toString()),nullptr);state.setProperty("bankHash",bank.empty()?juce::String{}:juce::String(baseLib::SHA256(bank).toString()),nullptr);state.setProperty("placeholder",placeholder,nullptr);state.setProperty("selectedPart",int(selectedPart.load()),nullptr);
 auto d=device.load();if(d){auto stats=d->diagnostics();state.setProperty("produced",juce::int64(stats[0]),nullptr);state.setProperty("consumed",juce::int64(stats[1]),nullptr);state.setProperty("midiReceived",juce::int64(stats[2]),nullptr);state.setProperty("waveReads",juce::int64(stats[3]),nullptr);state.setProperty("underruns",juce::int64(d->underruns()),nullptr);state.setProperty("midiOverflows",juce::int64(d->overflows()),nullptr);auto p=d->dumpProgram(),f=d->saveFlash();state.setProperty("program",juce::MemoryBlock(p.data(),p.size()).toBase64Encoding(),nullptr);
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
 stopEngines();os.clear();kRom.clear();kState.clear();bank.clear();flash.clear();programToRestore.clear();
 if(state.hasProperty("dataFolder") && juce::File(state.getProperty("dataFolder").toString()).isDirectory())dataFolder=juce::File(state.getProperty("dataFolder").toString());
 for(const auto* child:{"ROMs","Patches","Skins"})if(auto result=dataFolder.getChildFile(child).createDirectory();result.failed()){message=result.getErrorMessage();return;}
 const auto zero=dataFolder.getChildFile("ROMs/ks-wave-zero.bin");
 if(!zero.exists()){juce::MemoryBlock bytes(ks::Board::WaveSize,true);if(!zero.replaceWithData(bytes.getData(),bytes.getSize())){message="Cannot create required zero Wave ROM";return;}}
 browserState=state.getProperty("browserState",browserState).toString();
 const int savedClock=int(state.getProperty("clockSource",0));requestedClockSource.store(savedClock>=0 && savedClock<=2?ClockSource(savedClock):ClockSource::Host);
 const double savedScale=double(state.getProperty("editorScale",editorScale.load()));editorScale.store(savedScale>=.5&&savedScale<=1.5?savedScale:.75);
 const int controller=int(state.getProperty("ksControllerMode",0));ksController.store(controller>=0 && controller<=2?ks::ControllerMode(controller):ks::ControllerMode::H8);
 twoCoreProcessing.store(bool(state.getProperty("ksTwoCoreProcessing",false)));
 buffering.store(juce::jlimit(0,3,int(state.getProperty("bufferProfile",1))));bankFile=resolveAsset(state,"bank");flashFile=resolveAsset(state,"flash");skinChoice=state.getProperty("skinChoice","Auto").toString();customSkin=state.getProperty("customSkin").toString();currentPatch=state.getProperty("currentPatch").toString();controlChannel.store(uint8_t(int(state.getProperty("editorChannel",0))&15));if(!state.hasProperty("dataFolder") && firmwares.empty())romFolder=juce::File(state.getProperty("romFolder").toString());if(!state.hasProperty("dataFolder") && firmwares.empty() && romFolder.isDirectory()){juce::String ignored;setRomFolder(romFolder,ignored);}
 restoreKSDataFiles(juce::JSON::parse(state.getProperty("ksDataFiles").toString()));
 if(bool(state.getProperty("kModel"))){auto o=resolveAsset(state,"os");auto rom=readK(o);if(!rom.isValid() || baseLib::SHA256(rom.data()).toString()!=state.getProperty("osHash").toString().toStdString()){message="K-Station firmware missing or hash mismatch";return;}juce::MemoryBlock saved;if(!saved.fromBase64Encoding(state.getProperty("kState").toString()))return;const auto managed=managedAsset(o,message);if(managed==juce::File{})return;auto* b=static_cast<const uint8_t*>(saved.getData());kState.assign(b,b+saved.getSize());kRom=rom.data();osFile=managed;parameters.replaceState(state);rebuildK();return;}
 juce::String e;auto o=resolveAsset(state,"os"),b=bankFile,f=flashFile;
 if(!b.existsAsFile() && bool(state.getProperty("placeholder")))b=zero;
 std::vector<uint8_t> requiredData;if(!readKSData(selectedKSDataFiles(),requiredData,message))return;
 auto rom=firmware(o);auto waves=bytes(b);if(!rom.isValid()||waves.size()!=ks::Board::WaveSize || baseLib::SHA256(rom.data()).toString()!=state.getProperty("osHash").toString().toStdString() || baseLib::SHA256(waves).toString()!=state.getProperty("bankHash").toString().toStdString()){message="State OS/wave bank missing or hash mismatch";return;}
 const auto managedOS=managedAsset(o,message),managedWave=managedAsset(b,message);if(managedOS==juce::File{} || managedWave==juce::File{})return;
 std::vector<juce::File> managedData;for(const auto& file:selectedKSDataFiles()){const auto saved=managedAsset(file,message);if(saved==juce::File{})return;managedData.push_back(saved);}
 juce::MemoryBlock user;user.fromBase64Encoding(state.getProperty("userFlash").toString());if(user.getSize()!=0x60000-0x1fc00)return;
 flash.assign(0x80000,0xff);std::memcpy(flash.data()+0x1fc00,user.getData(),user.getSize());
 // Demo music is external firmware data, never embedded in the DAW project.
 const auto originalFlash=bytes(f);if(ks::validDemoSong(originalFlash.data(),originalFlash.size()))std::copy(originalFlash.begin()+0x1a000,originalFlash.begin()+0x1fc00,flash.begin()+0x1a000);
 os=rom.data();bank=std::move(waves);osFile=managedOS;bankFile=managedWave;flashFiles=std::move(managedData);flashFile=flashFiles.front();placeholder=std::all_of(bank.begin(),bank.end(),[](uint8_t v){return v==0;});
 juce::MemoryBlock p;p.fromBase64Encoding(state.getProperty("program").toString());if(p.getSize()!=270 && p.getSize()!=1038)return;auto* patch=static_cast<const uint8_t*>(p.getData());programToRestore.assign(patch,patch+p.getSize());
 buildLibrary();auto imported=state.getChildWithName("PresetLibrary");for(int n=0;n<imported.getNumChildren() && n<12000;++n){auto entry=imported.getChild(n);juce::MemoryBlock data;if(!data.fromBase64Encoding(entry.getProperty("data").toString()) || data.getSize()!=270)continue;const auto* bytes=static_cast<const uint8_t*>(data.getData());constexpr uint8_t prefix[]{0xf0,0,0x20,0x29,1,0x42};if(!std::equal(std::begin(prefix),std::end(prefix),bytes) || bytes[7]!=0 || bytes[269]!=0xf7 || !std::all_of(bytes+1,bytes+269,[](uint8_t v){return v<128;}))continue;Preset preset;preset.name=entry.getProperty("name").toString();std::copy_n(bytes,270,preset.data.begin());library.push_back(std::move(preset));}
 auto multis=state.getChildWithName("PerformanceLibrary");for(int n=0;n<multis.getNumChildren() && n<12000;++n){auto entry=multis.getChild(n);juce::MemoryBlock data;if(!data.fromBase64Encoding(entry.getProperty("data").toString()) || data.getSize()!=1038)continue;const auto* bytes=static_cast<const uint8_t*>(data.getData());constexpr uint8_t prefix[]{0xf0,0,0x20,0x29,1,0x42};if(!std::equal(std::begin(prefix),std::end(prefix),bytes) || bytes[7]!=2 || bytes[1037]!=0xf7 || !std::all_of(bytes+1,bytes+1037,[](uint8_t v){return v<128;}))continue;Performance preset;preset.name=entry.getProperty("name").toString();std::copy_n(bytes,1038,preset.data.begin());performanceLibrary.push_back(std::move(preset));}
 selectedPart.store(unsigned(juce::jlimit(0,3,int(state.getProperty("selectedPart",0)))));parameters.replaceState(state);volume.store(uint8_t(parameters.getRawParameterValue("volume")->load()));for(auto& n:pending)n.store(-1);rebuild();

}
juce::AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new silverK::Processor();}
namespace silverK {
int Processor::configureBufferProfile(){
 static constexpr unsigned floors[]{0,512,1024,2048};const auto profile=buffering.load();const auto frames=floors[profile];const auto block=profile==0 && lastObservedBlock>0?lastObservedBlock:hostBlock;
 if(ownedK){ownedK->device.setAudioBuffer(uint32_t(block),float(hostRate),frames);return int(ownedK->latency());}
 else if(ownedDevice){ownedDevice->setAudioBuffer(unsigned(block),frames);return int(ownedDevice->latency());}
 return getLatencySamples();
}
void Processor::applyBufferProfile(){pendingHostLatency.store(configureBufferProfile());}
void Processor::notifyHostLatency(){
 // Firmware workers only queue changes. Their caller publishes after joining,
 // outside firmwareLoadMutex as well as controlMutex. Hosts may reenter here.
 if(isFirmwareLoading())return;
 const auto latency=pendingHostLatency.exchange(-1);
 if(latency>=0 && latency!=getLatencySamples())setLatencySamples(latency);
}
void Processor::observeAudioBlock(int samples)noexcept{
 auto previous=observedBlock.load(std::memory_order_relaxed);for(unsigned attempt=0;attempt<4 && previous<samples;++attempt)if(observedBlock.compare_exchange_weak(previous,samples,std::memory_order_relaxed))break;
}
void Processor::timerCallback(){
 if(startupPending.exchange(false) && !isFirmwareLoading())beginFirmwareLoad(startupFirmware,true);
 if(isFirmwareLoading()){juce::String error;finishFirmwareLoad(error);if(isFirmwareLoading())return;}
 savePreferences();
 const auto block=observedBlock.exchange(0);if(block<=0)return;
 {std::lock_guard lock(controlMutex);if(lastObservedBlock==block)return;lastObservedBlock=block;if(buffering.load()==0)applyBufferProfile();}
 notifyHostLatency();
}
void Processor::setBufferProfile(int profile){{std::lock_guard lock(controlMutex);buffering.store(juce::jlimit(0,3,profile));applyBufferProfile();}notifyHostLatency();savePreferences();}
bool Processor::setKSTwoCoreProcessing(bool enabled,juce::String& error){
 error.clear();
 {
  std::lock_guard lock(controlMutex);
  if(isFirmwareLoading()){error="Wait for firmware loading to finish";return false;}
  if(enabled==ksTwoCoreProcessing())return true;
  if(performanceDiagnostics.mode()){
   performanceDiagnostics.stop();performanceStoppedMs=juce::Time::getMillisecondCounterHiRes();performanceStopReason="dsp_execution_changed";
  }
  if(ownedDevice){std::string failure;if(!ownedDevice->setDspWorkersEnabled(enabled,failure)){error=failure;return false;}}
  twoCoreProcessing.store(enabled);
 }
 savePreferences();updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));return true;
}
bool Processor::setKSControllerMode(int mode,juce::String& error){
 error.clear();if(mode<0 || mode>2){error="Invalid KS controller selection";return false;}
 if(mode!=int(ks::ControllerMode::Native) && !ks::boardComparisonAvailable()){error="KS H8/hybrid is available only in comparison tests";return false;}
 {
  std::lock_guard lock(controlMutex);
  if(isFirmwareLoading()){error="Wait for firmware loading to finish";return false;}
  if(mode==ksControllerMode())return true;
  if(performanceDiagnostics.mode()){
   performanceDiagnostics.stop();performanceStoppedMs=juce::Time::getMillisecondCounterHiRes();performanceStopReason="controller_changed";
  }
  if(ownedDevice){std::string failure;if(!ownedDevice->setControllerMode(ks::ControllerMode(mode),failure)){error=failure;return false;}}
  ksController.store(ks::ControllerMode(mode));
 }
 savePreferences();return true;
}
void Processor::stopEngines(){drumEditing.store(false);seenDrumRevision=0;if(performanceDiagnostics.mode()){performanceDiagnostics.stop();performanceStoppedMs=juce::Time::getMillisecondCounterHiRes();performanceStopReason="engine_rebuilt_or_stopped";}pendingHostLatency.store(-1);resetClock.store(true);device.store(nullptr);kDevice.store(nullptr);while(activeAudio.load())std::this_thread::yield();observedBlock.store(0);lastObservedBlock=0;ownedDevice.reset();ownedK.reset();}
void Processor::rebuildK(){stopEngines();kMode.store(true);multi.store(false);activeMask.store(1);selectedPart.store(0);resetPending();{std::lock_guard lock(uiMidiMutex);uiMidi.reset();}
 try{auto k=std::make_unique<KEngine>(kRom,hostRate,hostBlock,kState,buffering.load()==0?0u:256u<<buffering.load(),&performanceDiagnostics);ownedK=std::move(k);applyBufferProfile();kWire.fill(0);kMask.fill(0);resetClock.store(true);kDevice.store(ownedK.get());message="K-Station OS 2.1.01";}catch(const std::exception& e){message=e.what();}}
bool Processor::loadFirmware(const juce::File& file,juce::String& error){
 const juce::ScopeGuard notify{[this]{notifyHostLatency();}};
 startupPending.store(false);
 if(file.getSize()<=0 || file.getSize()>2*1024*1024){error="Firmware file is too large or empty";return false;}
 auto rom=readK(file);auto ksRom=firmware(file);
 if(!rom.isValid() && !ksRom.isValid()){error="Not a supported K-Station 2.1.01 or KS 2.1.02 firmware";return false;}
 const auto managed=managedAsset(file,error);if(managed==juce::File{})return false;
 {std::lock_guard lock(controlMutex);
  if(std::none_of(firmwares.begin(),firmwares.end(),[&](const auto& f){return f.file==managed;}))firmwares.push_back({managed,rom.isValid()});
  osFile=managed;juce::String ignored;rememberDataFolder(ignored);rememberAssets(ignored);
 }
 if(rom.isValid()){
  std::lock_guard lock(controlMutex);kRom=rom.data();kState.clear();rebuildK();if(!ownedK){error=message;return false;}return rememberAssets(error);
 }
 if(!load(managed,bankFile,flashFile,error))return false;
 publishFactoryPresets();return rememberAssets(error);
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
bool Processor::setRomFolder(const juce::File& folder,juce::String& error){
 if(!folder.isDirectory()){error="Choose an existing ROM folder";return false;}std::vector<FirmwareChoice> found;
 auto files=folder.findChildFiles(juce::File::findFiles,true,"*");files.sort();
 for(const auto& f:files){if(f.getSize()>2*1024*1024)continue;if(f.hasFileExtension("bin;os;syx;zip")){if(readK(f).isValid())found.push_back({f,true});else if(firmware(f).isValid())found.push_back({f,false});else if(f.getSize()==ks::Board::WaveSize && bankFile==juce::File{})bankFile=f;else if(f.getSize()==0x80000 && flashFile==juce::File{} && !isBundledInit(f)){std::vector<uint8_t> storage;juce::String ignored;if(readKSData({f},storage,ignored)){flashFile=f;flashFiles={f};}}}if(f.hasFileExtension("mid;midi") && f.getFileName().containsIgnoreCase("all data") && flashFile==juce::File{}){std::vector<uint8_t> storage;juce::String ignored;if(readKSData({f},storage,ignored)){flashFile=f;flashFiles={f};}}}
 romFolder=folder;firmwares=std::move(found);if(folder==dataFolder.getChildFile("ROMs") && !rememberDataFolder(error))return false;if(firmwares.empty()){error="No supported OS images in this folder";return false;}return true;
}
std::vector<Control> Processor::visibleControls()const{if(isKStation())return kControls();return {std::begin(controls),std::end(controls)};}
const Control* Processor::spec(const juce::String& id)const{static const auto k=kControls();if(isKStation()){for(const auto& c:k)if(id==c.id)return &c;}else for(const auto& c:controls)if(semanticId(id)==semanticId(c.id))return &c;return nullptr;}
juce::String Processor::uiParameterId(const juce::String& id)const{return isKStation()?"k_"+id:currentPart()?"part"+juce::String(currentPart()+1)+"_"+id:id;}
juce::RangedAudioParameter* Processor::uiParameter(const juce::String& id){return parameters.getParameter(uiParameterId(isKStation()?id:semanticId(id)));}
void Processor::processK(KEngine& engine,juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){
 baseLib::performance::Scope controlTiming(&performanceDiagnostics,baseLib::performance::Metric::Controls);
 selectClockSource([&](const uint8_t* data,size_t size){engine.midi(data,size);});
 uint8_t ui[4096];size_t count=0;while(count<sizeof(ui) && uiMidi.pop(ui[count]))++count;if(count)engine.midi(ui,count);
 std::array<uint8_t,128> patch;std::array<uint8_t,2048> cc;uint64_t revision;
 if(engine.device.getControlSnapshot(patch,cc,revision)){if(resetWire.exchange(false))kMask.fill(0);for(const auto& c:kstation::controls::Catalog){const size_t slot=c.number+(c.mapping==kstation::controls::Mapping::NRPN?128:0);const auto raw=c.patchOffset>=0?patch[c.patchOffset]:kWire[slot];if(((raw^kWire[slot])&kMask[slot])==0)kMask[slot]=0;kWire[slot]=uint8_t((raw&~kMask[slot])|(kWire[slot]&kMask[slot]));}}
 for(size_t i=0;i<KCount;++i){int value=pending[ParameterCount+i].exchange(-1);if(value<0)continue;const auto& c=kstation::controls::Catalog[i];const size_t slot=c.number+(c.mapping==kstation::controls::Mapping::NRPN?128:0);auto& raw=kWire[slot];raw=uint8_t((raw&~c.wireMask)|((value<<c.wireShift)&c.wireMask));kMask[slot]|=c.wireMask;const auto status=uint8_t(0xb0|controlChannel.load());if(c.mapping==kstation::controls::Mapping::NRPN){const uint8_t bytes[]{status,99,0,status,98,uint8_t(c.number),status,6,raw};engine.midi(bytes,9);}else{const uint8_t bytes[]{status,uint8_t(c.number),raw};engine.midi(bytes,3);}}
 sendTimedMidi(midi,[&](const uint8_t* data,size_t size,int offset){engine.midi(data,size,uint32_t(offset));});
 controlTiming.finish();baseLib::performance::Scope renderTiming(&performanceDiagnostics,baseLib::performance::Metric::AudioRender,uint64_t(audio.getNumSamples()));
 engine.process(audio.getWritePointer(0),audio.getWritePointer(1),audio.getNumSamples(),isNonRealtime(),getTotalNumInputChannels()?audio.getReadPointer(0):nullptr,getTotalNumInputChannels()>1?audio.getReadPointer(1):nullptr);for(int ch=2;ch<audio.getNumChannels();++ch)audio.clear(ch,0,audio.getNumSamples());midi.clear();
}
Processor::DrumEditInfo Processor::drumEditInfo()const{
 std::lock_guard lock(controlMutex);DrumEditInfo result;if(isFirmwareLoading() || !ownedDevice)return result;
 const auto state=ownedDevice->fullSnapshot();const auto& p=state.parts[state.performance?currentPart():0];
 if(p[187]<5 || p[187]>8 || p[188]>=49)return result;
 result.bank=p[187];result.note=36+p[188];result.name=juce::String::fromUTF8(reinterpret_cast<const char*>(p.data()+151),16).trim();return result;
}
bool Processor::demoSongActive()const{std::lock_guard lock(controlMutex);return ownedDevice && ownedDevice->demoActive();}
bool Processor::setDemoSong(bool enabled,juce::String& error){
 std::lock_guard lock(controlMutex);error.clear();
 if(isFirmwareLoading() || !ownedDevice){error="Load KS firmware first";return false;}
 if(enabled && !ownedDevice->demoAvailable()){error="The selected KS Data does not contain a compatible Demo Song. Select your full 524288-byte KS Flash with the embedded song in Setup > KS factory data. Program/Performance MIDI banks and Silver Init data do not include the song.";return false;}
 if(!ownedDevice->setDemoSong(enabled)){error="KS engine is not ready";return false;}return true;
}
bool Processor::activatePatch(const pluginLib::patchDB::PatchPtr& patch,juce::String& error){return activatePatchForPart(patch,currentPart(),error);}
bool Processor::activatePatchForPart(const pluginLib::patchDB::PatchPtr& patch,unsigned part,juce::String& error){
 if(part>=ks::PartCount){error="Invalid part";return false;}
 if(!patch || !firmwareReady()){error="Load firmware first";return false;}
 const auto& source=patch->sysex;
 if(ksDrum(source.data(),source.size())){
  if(isKStation()){error="Drum Maps require KS firmware";return false;}
  if(!validPatch(source)){error="Malformed KS Drum Map";return false;}
  std::lock_guard lock(controlMutex);if(!ownedDevice){error="Load KS firmware first";return false;}
  const auto bankNumber=source[11];const bool complete=drumMap(source.data(),source.size());
  const auto saved=ownedDevice->saveFlash();const size_t offset=0x20000-0x1fc00+(400+(bankNumber-5)*50)*256;
  // A partial dump cannot supply the other keys or shared effects. It is safe
  // to select it only when it matches the instance's already loaded bank.
  const size_t soundOffset=offset+source[12]*256;
  if(!complete && (saved.size()<soundOffset+256 || !std::equal(source.begin()+13,source.begin()+269,saved.begin()+soundOffset))){error="Import the complete Drum Map (49 sounds + effects), or load its KS factory data in Setup";return false;}
  bool sameBank=saved.size()>=offset+50*256;
  if(complete && sameBank)for(unsigned n=0;n<50;++n)if(!std::equal(source.begin()+n*270+13,source.begin()+n*270+269,saved.begin()+offset+n*256)){sameBank=false;break;}
  std::vector<uint8_t> bytes;
  if(complete && !sameBank){bytes.assign(source.begin(),source.end());for(unsigned n=0;n<50;++n){bytes[n*270+6]=0x7f;bytes[n*270+8]=1;}}
  const auto state=ownedDevice->fullSnapshot();const uint8_t number=complete?0:source[12];
  if(state.performance){
   auto next=state;auto& target=next.parts[part];
   // The firmware uses 187/188 to retain each part's drum-bank identity.
   // All keys read the bank in guest Flash; shared effects live in slot 49.
   if(complete){std::copy_n(source.data()+13,179,target.begin());std::copy_n(source.data()+49*270+13+84,41,target.begin()+84);target[126]=source[49*270+13+126];}
   else std::copy_n(source.data()+13,179,target.begin());
   target[187]=bankNumber;target[188]=number;
   const auto message=ks::stateMessage(next);bytes.insert(bytes.end(),message.begin(),message.end());
  }else{const auto channel=controlChannel.load();bytes.insert(bytes.end(),{uint8_t(0xb0|channel),32,uint8_t(32+bankNumber),uint8_t(0xc0|channel),number});}
  if(!queueUiMidi(bytes.data(),bytes.size())){error="Preset queue is full";return false;}
  resetPending();if(part==currentPart())currentPatch=patchName(*patch);error.clear();return true;
 }
 auto converted=convertPatch(source,isKStation()?Model::KStation:Model::KS,part);if(converted.data.empty()){error="Unsupported or malformed program dump";return false;}
 // V-Station banks can drive the K fixed-point effect sum to its ceiling.
 // Reserve firmware program level; preserve the authored filter/drive and the
 // untouched library payload. A post-output limiter cannot undo that clipping.
 if(isKStation() && patchModel(*patch)==Model::VStation){converted.data[13+125]=uint8_t(std::max(0,int(converted.data[13+125])-6));converted.changes.add("V-Station level: -6 dB");}
 if(!isKStation() && patchModel(*patch)==Model::VStation){std::fill_n(converted.data.begin()+13+151,16,' ');const juce::String name(patch->getName());for(int n=0;n<std::min(16,name.length());++n){const auto c=name[n];converted.data[13+151+n]=uint8_t(c>=32 && c<127?c:'?');}}
 bool ok=false;{std::lock_guard lock(controlMutex);if(ownedK)ok=queueUiMidi(converted.data.data(),converted.data.size());else if(ownedDevice){if(converted.data.size()==1038){ks::State state;state.performance=true;for(unsigned i=0;i<4;++i)std::copy_n(converted.data.data()+13+256*i,256,state.parts[i].begin());state.activeMask=state.parts[0][190]&15;ok=queueState(state,true,error);}else{auto state=ownedDevice->fullSnapshot();std::copy_n(converted.data.data()+13,179,state.parts[part].begin());const bool exitDrum=state.parts[part][187]>=5 && state.parts[part][187]<=8;if(exitDrum){state.parts[part][187]=1;state.parts[part][188]=0;}ok=queueState(state,exitDrum && !state.performance,error);}}}
 if(!ok){if(error.isEmpty())error="MIDI queue full";return false;}resetPending();if(part==currentPart())currentPatch=patchName(*patch);error=converted.changes.joinIntoString("\n");return true;
}
bool Processor::saveCurrentPatch(const juce::File& file,juce::String& error){std::lock_guard lock(controlMutex);std::vector<uint8_t> data;if(ownedDevice){
 const auto state=ownedDevice->fullSnapshot();const auto& current=state.parts[state.performance?currentPart():0];const unsigned bank=current[187],slot=current[188];
 if(bank>=5 && bank<=8 && slot<49){
  const auto stored=ownedDevice->saveFlash();const size_t offset=0x20000-0x1fc00+(400+(bank-5)*50)*256;
  if(stored.size()<offset+50*256){error="Drum Map snapshot unavailable";return false;}
  data.reserve(DrumMapSize);
  for(unsigned n=0;n<50;++n){
   data.insert(data.end(),{0xf0,0,0x20,0x29,1,0x42,0x7f,1,1,0x11,2,uint8_t(bank),uint8_t(n)});
   const auto begin=data.size();data.insert(data.end(),stored.begin()+offset+n*256,stored.begin()+offset+(n+1)*256);
   if(n==slot){std::copy(current.begin(),current.begin()+84,data.begin()+begin);data[begin+125]=current[125];std::copy(current.begin()+127,current.begin()+179,data.begin()+begin+127);}
   if(n==49){std::copy(current.begin()+84,current.begin()+125,data.begin()+begin+84);data[begin+126]=current[126];}
   data.push_back(0xf7);
  }
 }else data=ownedDevice->dumpProgram();
 }else if(ownedK){std::array<uint8_t,128> patch;std::array<uint8_t,2048> cc;uint64_t rev;if(!ownedK->device.getControlSnapshot(patch,cc,rev)){error="Snapshot unavailable; try again";return false;}data={0xf0,0,0x20,0x29,1,0x41,0x7f,0,0,0x11,1,0,0};data.insert(data.end(),patch.begin(),patch.end());data.push_back(0xf7);}if(data.empty() || !file.replaceWithData(data.data(),data.size())){error="Cannot save current sound";return false;}patchLibrary.addPath(file);return true;}
}
