#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ksLib/ksDevice.h"
#include <atomic>
#include "../../ks/ksJucePlugin/ksControls.h"
#include "../../kstation/kstationJucePlugin/kstationControls.h"
#include "silverLibrary.h"
#include "silverClock.h"
#include "silverMidiLearn.h"
#include "jucePluginLib/hostAudioWorkgroup.h"
namespace silverK {
using namespace ksPlugin;
struct KEngine;
class Processor final : public juce::AudioProcessor,private juce::AudioProcessorValueTreeState::Listener,private juce::Timer {
public:
 Processor();~Processor() override;
 const juce::String getName()const override{return "Silver-K";}
 void prepareToPlay(double,int)override;void releaseResources()override{}
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 bool isBusesLayoutSupported(const BusesLayout&)const override;
 bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 20;}
 bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return "KS";}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override;void setStateInformation(const void*,int)override;
 bool load(const juce::File&,const juce::File&,const juce::File&,juce::String&);
 juce::String status()const;uint64_t underruns()const;
 juce::AudioProcessorValueTreeState parameters;
 juce::File osFile,bankFile,flashFile;
 struct Preset {juce::String name;std::array<uint8_t,270> data;uint8_t bank=0,number=0;};
 struct Performance {juce::String name;std::array<uint8_t,1038> data;};
 std::vector<Preset> presets()const;
 bool importPresets(const juce::File&,juce::String&);
 bool selectPreset(int,juce::String&);
 void publishFactoryPresets(); // UI/cold load: expose restored KS factory data to the browser.
 std::vector<Performance> performances()const;
 bool selectPerformance(int,juce::String&);
 bool setPerformanceMode(bool,juce::String&);
 bool setPartActive(unsigned,bool,juce::String&);
 void selectPart(unsigned part){selectedPart.store(std::min(part,ks::PartCount-1));}
 unsigned currentPart()const{return selectedPart.load();}
 bool performanceMode()const{return multi.load();}
 uint8_t activeParts()const{return activeMask.load();}
 static juce::String parameterId(const Control& c,unsigned part){return part?"part"+juce::String(part+1)+"_"+c.id:juce::String(c.id);}
 bool sendShortMidi(uint8_t,uint8_t,uint8_t=0);
 void refreshControls();
 juce::String currentName()const;
 uint8_t midiChannel()const{return multi.load()?partChannels[currentPart()].load():controlChannel.load();}
 bool firmwareReady()const{return device.load()!=nullptr || kDevice.load()!=nullptr;}

bool isKStation()const{return kMode.load();}
 bool loadFirmware(const juce::File&,juce::String&);
 bool beginFirmwareLoad(const juce::File&,bool automatic=false); // Message thread: boot off the UI/audio threads.
 bool finishFirmwareLoad(juce::String& error); // Message thread: consume a completed load.
 bool isFirmwareLoading()const{return firmwareLoading.load();}
 bool setRomFolder(const juce::File&,juce::String&);
 bool setDataFolder(const juce::File&,juce::String&);
 bool setWaveRom(const juce::File&,juce::String&);
 bool setFlashRom(const juce::File&,juce::String&);
 bool setFlashRoms(const juce::Array<juce::File>&,juce::String&);
 std::vector<juce::File> ksDataFiles()const;
 void savePreferences(); // UI/cold paths only; never called by processBlock.
 juce::String browserState;
 uint64_t firmwareLoadRevision()const{return firmwareRevision.load();}
 juce::File dataFolder;
 juce::File patchesFolder()const{return dataFolder.getChildFile("Patches");}
 juce::File skinsFolder()const{return dataFolder.getChildFile("Skins");}
 struct FirmwareChoice {juce::File file;bool kStation;};
 std::vector<FirmwareChoice> firmwares;
 juce::File romFolder;
 juce::String skinChoice="Auto",customSkin,currentPatch;
 // UI preference, separate from sound parameters and host/DPI scaling.
 std::atomic<double> editorScale{.75};
 std::shared_ptr<Library> libraryOwner;
 Library& patchLibrary;
 std::vector<Control> visibleControls()const;
 const Control* spec(const juce::String&)const;
 juce::RangedAudioParameter* uiParameter(const juce::String&);
 juce::String uiParameterId(const juce::String& id)const;
 bool activatePatch(const pluginLib::patchDB::PatchPtr&,juce::String&);
 bool activatePatchForPart(const pluginLib::patchDB::PatchPtr&,unsigned,juce::String&);
 std::array<juce::String,4> partNames()const;
 uint64_t beginNoteLearn()noexcept{return noteLearn.arm();}
 void cancelNoteLearn(uint64_t token)noexcept{noteLearn.cancel(token);}
 int takeLearnedNote(uint64_t token)noexcept{return noteLearn.take(token);}
 bool saveCurrentPatch(const juce::File&,juce::String&);
 uint8_t editorChannel()const{return isKStation()?controlChannel.load():midiChannel();}
 int bufferProfile()const{return buffering.load();}
 void setBufferProfile(int);
 int ksControllerMode()const{return ks::boardComparisonAvailable()?int(ksController.load()):int(ks::ControllerMode::Native);}
 bool ksTwoCoreProcessing()const{return twoCoreProcessing.load();}
 bool setKSTwoCoreProcessing(bool,juce::String& error); // UI/control thread only.
 ClockSource clockSource()const{return requestedClockSource.load();}
 void setClockSource(ClockSource); // Message thread; applied by the next audio block.
 bool setKSControllerMode(int,juce::String& error); // UI/control thread only.
 bool demoSongActive()const;
 bool setDemoSong(bool,juce::String& error);
 struct DrumEditInfo{int bank=0,note=-1;juce::String name;};
 DrumEditInfo drumEditInfo()const;
 bool drumEditEnabled()const{return drumEditing.load();}
 void setDrumEdit(bool enabled){drumEditing.store(enabled && drumEditInfo().bank!=0);}
 // Message/control thread only. Mode: 0 stop, 1 new summary, 2 new detailed session.
 bool setPerformanceDiagnostics(int mode);
 int performanceDiagnosticsMode()const{return performanceDiagnostics.mode();}
 juce::String performanceReport()const;
 void setEditorChannel(uint8_t c){controlChannel.store(c&15);editorChannelPreference.store(c&15);savePreferences();}
private:
 baseLib::performance::Diagnostics performanceDiagnostics;
 juce::var performanceSession;
 double performanceStartedMs=0,performanceStoppedMs=0;
 juce::String performanceStopReason;
 MidiNoteLearn noteLearn;
 std::atomic<bool> drumEditing{false};
 uint64_t seenDrumRevision=0;
 pluginLib::HostAudioWorkgroup hostAudioWorkgroup;
 std::atomic<int> buffering{1};
 std::atomic<bool> twoCoreProcessing{ks::Device::environmentDspWorkersEnabled()};
 std::atomic<ks::ControllerMode> ksController{ks::Device::environmentControllerMode()};
 void applyBufferProfile();
 void notifyHostLatency();
 std::atomic<int> pendingHostLatency{-1};
 int configureBufferProfile();
 void timerCallback()override;
 void observeAudioBlock(int)noexcept;
 std::atomic<int> observedBlock{0};
 int lastObservedBlock=0;
 std::thread firmwareLoader;
 std::mutex firmwareLoadMutex;
 std::atomic<bool> firmwareLoading{false},firmwareLoadDone{false};
 juce::String firmwareLoadError;
 std::atomic<bool> startupPending{false};
 std::atomic<bool> hostStateRestored{false};
 std::atomic<uint64_t> firmwareRevision{0};
 juce::File startupFirmware;
 std::atomic<bool> kMode{false};
 std::atomic<KEngine*> kDevice{nullptr};
 std::unique_ptr<KEngine> ownedK;
 std::vector<uint8_t> kRom,kState;
 std::array<uint8_t,256> kWire{},kMask{};
 void rebuildK();
 HostClock hostClock;
 std::atomic<ClockSource> requestedClockSource{ClockSource::Host};
 ClockSource activeClockSource=ClockSource::Host;
 int clockSourceMode=-1;
 std::array<uint8_t,16> externalNrpn{};
 template<class Emit> void selectClockSource(Emit emit){
  // Both supported OS 2.1 images use 0=Auto, 1=Internal, 2=External.
  const int wanted=firmwareClockSource(activeClockSource,hostClock.ownsTransport());
  if(clockSourceMode==wanted)return;
  clockSourceMode=wanted;
  const uint8_t channel=controlChannel.load(),status=uint8_t(0xb0|channel);
  const uint8_t bytes[]{status,99,0,status,98,uint8_t(isKStation()?38:114),status,6,uint8_t(wanted),status,98,externalNrpn[channel]};
  emit(bytes,sizeof(bytes));
 }
 std::atomic<bool> resetClock{true};
 void prepareHostClock(int);
 template<class Emit> void sendTimedMidi(const juce::MidiBuffer& midi,Emit emit){
  HostClock::Event clock{};bool hasClock=hostClock.next(clock);
  for(const auto m:midi){
   noteLearn.observe(m.data,size_t(m.numBytes));
   const int offset=std::max(0,m.samplePosition);
   while(hasClock && clock.offset<=offset){emit(&clock.status,1,clock.offset);hasClock=hostClock.next(clock);}
   if(m.numBytes<=65536 && !(HostClock::transport(m.data[0]) && !acceptsExternalTransport(activeClockSource,hostClock.ownsTransport()))){
    if(m.numBytes==3 && (m.data[0]&0xf0)==0xb0 && m.data[1]==98)externalNrpn[m.data[0]&15]=m.data[2];
    emit(m.data,size_t(m.numBytes),offset);
   }
  }
  while(hasClock){emit(&clock.status,1,clock.offset);hasClock=hostClock.next(clock);}
 }
 void processK(KEngine&,juce::AudioBuffer<float>&,juce::MidiBuffer&);
 void stopEngines();
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
 void parameterChanged(const juce::String&,float)override;
 static constexpr size_t ParameterCount=std::size(controls)*ks::PartCount;
 static constexpr size_t KCount=kstation::controls::Catalog.size();
 static constexpr size_t TotalCount=ParameterCount+KCount;
 std::array<juce::String,TotalCount> parameterIds;
 std::array<std::atomic<int>,TotalCount> pending;
 std::unique_ptr<ks::Device> ownedDevice;
 kstation::Queue<uint8_t,65536> uiMidi;
 std::mutex uiMidiMutex;
 std::vector<Preset> library;size_t libraryBaseCount=0;
 std::vector<Performance> performanceLibrary;size_t performanceBaseCount=0;
 std::array<ks::Program,ks::PartCount> wireShadow{},wirePendingMask{};
 std::atomic<bool> resetWire{false};
 std::array<std::atomic<int>,TotalCount> awaiting;
 std::array<std::atomic<uint64_t>,TotalCount> editTime;
 std::atomic<unsigned> selectedPart{0};
 std::atomic<bool> multi{false};
 std::atomic<uint8_t> activeMask{1};
 std::array<std::atomic<uint8_t>,ks::PartCount> partChannels{};
 bool queueState(const ks::State&,bool selectMode,juce::String&);
 void resetPending();
 void buildLibrary();
 bool queueUiMidi(const uint8_t*,size_t);

 std::atomic<ks::Device*> device{nullptr};
 std::atomic<unsigned> activeAudio{0};
 mutable std::mutex controlMutex;
 std::vector<uint8_t> os,bank,flash,programToRestore;
 std::atomic<uint8_t> controlChannel{0};
 std::atomic<uint8_t> editorChannelPreference{0};
 std::atomic<uint8_t> volume{127};
 juce::String message="Load KS OS, external wave bank and user Flash. Zero bank is supported.";
 double hostRate=48000;int hostBlock=256;
 bool placeholder=false;
 void rebuild();
 void initializeDataFolder();
 bool rememberDataFolder(juce::String&);
 bool rememberAssets(juce::String&);
 bool updateSettings(const juce::NamedValueSet&,juce::String&)const;
 juce::NamedValueSet preferenceValues()const;
 juce::NamedValueSet preferenceBaseline;
 juce::File settingsFile()const;
 juce::File resolveAsset(const juce::ValueTree&,const char*)const;
 std::vector<juce::File> selectedKSDataFiles()const;
 std::vector<juce::File> flashFiles;
 juce::var ksDataReferences()const;
 void restoreKSDataFiles(const juce::var&,bool allowRelative=true);
 juce::File managedAsset(const juce::File&,juce::String&)const;
 static bool readKSData(const std::vector<juce::File>&,std::vector<uint8_t>&,juce::String&);
 static bool isBundledInit(const juce::File&);
};
}
