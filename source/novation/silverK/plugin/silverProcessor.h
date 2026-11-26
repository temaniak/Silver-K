#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ksLib/ksDevice.h"
#include "../../ks/ksLib/command/ksUserDrumPattern.h"
#include "../../ks/ksLib/command/ksDesktopDrumMap.h"
#include <atomic>
#include <optional>
#include "../../ks/ksJucePlugin/ksControls.h"
#include "../../kstation/kstationJucePlugin/kstationControls.h"
#include "silverLibrary.h"
#include "silverClock.h"
#include "silverMidiLearn.h"
#include "jucePluginLib/hostAudioWorkgroup.h"

namespace kstation { class Rom; }

namespace silverK {
using namespace ksPlugin;
class KEngine;
class KsEngine;
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
 void rescanPresets(std::function<void()> completed);
 std::vector<Performance> performances()const;
 bool selectPerformance(int,juce::String&);
 bool setPerformanceMode(bool,juce::String&);
 bool setPartActive(unsigned,bool,juce::String&);
 void selectPart(unsigned part){
  const auto next=std::min(part,ks::PartCount-1);
  if(selectedPart.exchange(next)!=next)setDrumEdit(false);
 }
 unsigned currentPart()const{return selectedPart.load();}
 bool performanceMode()const{return multi.load();}
 uint8_t activeParts()const{return activeMask.load();}
 uint8_t displayedActiveParts()const; // Includes accepted desktop part switches until publication.
 static juce::String parameterId(const Control& c,unsigned part){return part?"part"+juce::String(part+1)+"_"+c.id:juce::String(c.id);}
 bool sendShortMidi(uint8_t,uint8_t,uint8_t=0);
 void refreshControls();
 juce::String currentName()const;
 juce::String loadedPatchName()const;
 uint8_t midiChannel()const{return multi.load()?partChannels[currentPart()].load():globalMidiChannel.load();}
 bool firmwareReady()const{return device.load()!=nullptr || kDevice.load()!=nullptr;}

bool isKStation()const{return kMode.load();}
 bool loadFirmware(const juce::File&,juce::String&);
 bool beginFirmwareLoad(const juce::File&,bool automatic=false); // Message thread: boot off the UI/audio threads.
 bool finishFirmwareLoad(juce::String& error); // Message thread: consume a completed load.
 bool isFirmwareLoading()const{return firmwareLoading.load();}
 bool setDataFolder(const juce::File&,juce::String&,bool rescanPresets=false);
 juce::File firmwareAfterFolderScan()const;
 juce::String dataFolderStatus()const;
 bool setWaveRom(const juce::File&,juce::String&);
 bool setFlashRom(const juce::File&,juce::String&);
 bool setFlashRoms(const juce::Array<juce::File>&,juce::String&);
 std::vector<juce::File> ksDataFiles()const;
 void savePreferences(bool flushEditorScale=false); // UI/cold paths only; never called by processBlock.
 void setEditorScaleFromResize(double);
#if JUCE_WINDOWS
 bool prefersOpenGLRenderer() const;
 bool setOpenGLRendererPreference(bool enabled, juce::String& error) const;
#endif
 juce::String browserState;
 uint64_t firmwareLoadRevision()const{return firmwareRevision.load();}
 juce::File dataFolder;
 juce::File patchesFolder()const{return dataFolder.getChildFile("Patches");}
 juce::File userPatchesFolder()const{return patchesFolder().getChildFile("User");}
 juce::File skinsFolder()const{return dataFolder.getChildFile("Skins");}
 struct FirmwareChoice {juce::File file;bool kStation;};
 std::vector<FirmwareChoice> firmwares;
 juce::File romFolder;
 static constexpr auto defaultSkin="SoulTrain";
 juce::String skinChoice=defaultSkin,customSkin,currentPatch;
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
 enum class PatchSaveScope { SelectedSound, Performance };
 bool saveUserPatch(const juce::File&,PatchSaveScope,juce::String&);
 uint8_t editorChannel()const{return midiChannel();}
 int bufferProfile()const{return buffering.load();}
 void setBufferProfile(int);
 static constexpr bool ksControllerChoices(){return false;}
 int ksControllerMode()const{return int(ks::ControllerMode::Native);}
 bool ksTwoCoreProcessing()const{return twoCoreProcessing.load();}
 bool setKSTwoCoreProcessing(bool,juce::String& error); // UI/control thread only.
 ClockSource clockSource()const{return requestedClockSource.load();}
 void setClockSource(ClockSource); // Message thread; applied by the next audio block.
 enum class FirmwareSwitch { KeepPatch, RecallLast };
 static constexpr std::array<const char*,2> firmwareSwitchLabels{"Keep current patch","Recall last patch"};
 FirmwareSwitch firmwareSwitch()const{return firmwareSwitchMode.load();}
 void setFirmwareSwitch(FirmwareSwitch);
 bool setKSControllerMode(int,juce::String& error); // UI/control thread only.
 struct DrumEditInfo{
  int bank=0,note=-1;
  juce::String name;
  bool editing=false;
  bool operator==(const DrumEditInfo& other)const{
   return bank==other.bank && note==other.note && name==other.name && editing==other.editing;
  }
 };
 DrumEditInfo drumEditInfo()const;
 using DrumGridEdit=novation::controller::command::KSDrumGridEdit;
 using DrumLoadStatus=novation::controller::command::KSDesktopLoadStatus;
 using DrumPlayhead=novation::controller::command::NativeDrumPlayhead;
 static constexpr uint8_t UserDrumRhythm=33;
 uint8_t drumEditorSlot()const noexcept{return performanceMode()?uint8_t(currentPart()+1):0;}
 bool userDrumRhythmSelected()const noexcept{return (userDrumMask.load() & (1u<<drumEditorSlot()))!=0;}
 DrumLoadStatus drumMapLoadStatus()const;
 DrumLoadStatus ksPresetLoadStatus()const;
 struct DrumSequencerInfo {
  uint8_t slot=0,bank=0;
  novation::controller::command::KSUserDrumBank sequences;
  std::array<juce::String,DrumGridEdit::LastNote-DrumGridEdit::FirstNote+1> names;
 };
 DrumSequencerInfo drumSequencerInfo();
 std::optional<DrumPlayhead> drumPlayhead(uint8_t slot)const;
 bool editDrumGrid(DrumGridEdit,juce::String& error);
 bool selectDrumRhythm(uint8_t slot,uint8_t rhythm,juce::String& error);
 static constexpr bool drumEditSupported(){return true;}
 bool drumEditEnabled()const noexcept{return requestedDrumEditSlot.load()==drumEditorSlot();}
 void setDrumEdit(bool);
 // Message/control thread only. Mode: 0 stop, 1 new summary, 2 new detailed session.
 bool setPerformanceDiagnostics(int mode);
 int performanceDiagnosticsMode()const{return performanceDiagnostics.mode();}
 juce::String performanceReport()const;
private:
 baseLib::performance::Diagnostics performanceDiagnostics;
 juce::var performanceSession;
 double performanceStartedMs=0,performanceStoppedMs=0;
 juce::String performanceStopReason;
 MidiNoteLearn noteLearn;
 pluginLib::HostAudioWorkgroup hostAudioWorkgroup;
 std::atomic<int> buffering{1};
 std::atomic<bool> twoCoreProcessing{false};
 void applyBufferProfile();
 void notifyHostLatency();
 std::atomic<int> pendingHostLatency{-1};
 void timerCallback()override;
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
 void publishK(std::unique_ptr<KEngine>);
 void publishKS(std::unique_ptr<KsEngine>);
 bool reconfigureEngines(double rate,int block,int profile,bool parallel,juce::String& error);
 std::atomic<ClockSource> requestedClockSource{ClockSource::Host};
 ClockSource activeClockSource=ClockSource::Host;
 double lastHostBpm=0;
 bool hostPlaying=false;
 int clockSourceMode=-1;
 uint8_t clockSourceChannel=0;
 std::array<uint8_t,16> externalNrpn{};
 template<class Emit> void selectClockSource(uint8_t channel,Emit emit){
  // Both supported OS 2.1 images use 0=Auto, 1=Internal, 2=External.
  const int wanted=int(activeClockSource);
  if(clockSourceMode==wanted && clockSourceChannel==channel)return;
  clockSourceMode=wanted;
  clockSourceChannel=channel;
  const auto status=uint8_t(0xb0|channel);
  const uint8_t bytes[]{status,99,0,status,98,uint8_t(isKStation()?38:114),status,6,uint8_t(wanted),status,98,externalNrpn[channel]};
  emit(bytes,sizeof(bytes));
 }
 std::atomic<bool> resetClock{true};
 struct HostTiming { HostClock::Position position; std::optional<HostLoop> loop; };
 HostTiming prepareClockPosition();
 template<class Emit> void sendTimedMidi(const juce::MidiBuffer& midi,Emit emit){
  for(const auto m:midi){
   noteLearn.observe(m.data,size_t(m.numBytes));
   const int offset=std::max(0,m.samplePosition);
   if(m.numBytes<=65536 && (activeClockSource==ClockSource::Midi || !HostClock::transport(m.data[0]))){
    if(m.numBytes==3 && (m.data[0]&0xf0)==0xb0 && m.data[1]==98)externalNrpn[m.data[0]&15]=m.data[2];
    emit(m.data,size_t(m.numBytes),offset);
   }
  }
 }
 void processK(KEngine&,juce::AudioBuffer<float>&,juce::MidiBuffer&,const HostTiming&);
 void stopEngines();
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
 void parameterChanged(const juce::String&,float)override;
 static constexpr size_t ParameterCount=std::size(controls)*ks::PartCount;
 static constexpr size_t KCount=kstation::controls::Catalog.size();
 static constexpr size_t TotalCount=ParameterCount+KCount;
 std::array<juce::String,TotalCount> parameterIds;
 std::array<std::atomic<int64_t>,TotalCount> pending;
 std::unique_ptr<KsEngine> ownedDevice;
 static constexpr uint16_t UiSoundLoadToken=256;
 kstation::Queue<uint16_t,65536> uiMidi;
 std::mutex uiMidiMutex;
 std::vector<Preset> library;size_t libraryBaseCount=0;
 std::vector<Performance> performanceLibrary;size_t performanceBaseCount=0;
 std::array<ks::Program,ks::PartCount> wireShadow{},wirePendingMask{};
 std::atomic<bool> resetWire{false};
 std::array<std::atomic<int64_t>,TotalCount> awaiting;
 std::array<std::atomic<uint64_t>,TotalCount> editTime;
 std::atomic<uint8_t> requestedDrumEditSlot{novation::controller::command::KSDrumEditMode::Off};
 std::atomic<uint64_t> drumParameterAddress{0};
 std::atomic<unsigned> selectedPart{0};
 std::atomic<bool> multi{false};
 std::atomic<uint8_t> activeMask{1};
 uint8_t requestedPartMask=1; // Protected by controlMutex.
 std::array<std::atomic<uint8_t>,ks::PartCount> partChannels{};
 bool queueState(const ks::State&,bool selectMode,juce::String&);
 void resetPending();
 void buildLibrary();
 bool queueUiMidi(const uint8_t*,size_t);

 std::atomic<KsEngine*> device{nullptr};
 std::atomic<unsigned> activeAudio{0};
 mutable std::mutex controlMutex;
 struct FirmwareSound {
  std::vector<uint8_t> archive;
  juce::String name, dataHash;
  unsigned part=0;
 };
 FirmwareSound lastKSound,lastKSSound;
 juce::String keptKProgramKey;
 juce::String ksDataHash;
 std::atomic<FirmwareSwitch> firmwareSwitchMode{FirmwareSwitch::KeepPatch};
 std::optional<FirmwareSound> captureFirmwareSound(juce::String&);
 bool loadK(const juce::File&,const kstation::Rom&,juce::String&);
 bool loadKS(const juce::File&,const juce::File&,const juce::File&,std::vector<uint8_t>,juce::String&);
 std::vector<uint8_t> os,bank,flash,programToRestore,ksCommandState;
 novation::controller::command::KSUserDrumPatterns userDrums;
 std::vector<std::shared_ptr<novation::controller::command::KSDesktopPatchExtras>> pendingPatchLoads;
 bool collectPatchLoads(); // controlMutex held
 std::atomic<uint8_t> userDrumMask{0};
 mutable std::atomic<DrumLoadStatus> drumLoadState{DrumLoadStatus::Idle};
 std::atomic<uint8_t> globalMidiChannel{0};
 std::atomic<uint8_t> volume{127};
 juce::String message="Add your firmware and KS factory data to ROMs, then rescan in Setup.";
 double hostRate=48000;int hostBlock=256;
 bool placeholder=false;
 void rebuild();
 void initializeDataFolder();
 bool rememberDataFolder(juce::String&);
 bool rememberAssets(juce::String&);
 bool updateSettings(const juce::NamedValueSet&,juce::String&)const;
 juce::NamedValueSet preferenceValues()const;
 juce::NamedValueSet preferenceBaseline;
 std::atomic<double> lastEditorResizeMs{0};
 juce::File settingsFile()const;
 juce::File resolveAsset(const juce::ValueTree&,const char*)const;
 std::vector<juce::File> selectedKSDataFiles()const;
 std::vector<juce::File> flashFiles;
 juce::File loadedWaveFile;
 std::vector<juce::File> loadedDataFiles;
 juce::String scannedWaveHash,scannedDataHash;
 juce::var ksDataReferences(const std::vector<juce::File>&)const;
 void restoreKSDataFiles(const juce::var&,bool allowRelative=true);
 juce::File managedAsset(const juce::File&,juce::String&)const;
 struct KSDataCoverage {unsigned programs=0,performances=0;};
 static bool readKSData(const std::vector<juce::File>&,std::vector<uint8_t>&,juce::String&,KSDataCoverage* coverage=nullptr);
 static std::vector<juce::File> findKSData(const juce::File&,const juce::Array<juce::File>&,
                                         const std::vector<juce::File>& preferred,std::vector<uint8_t>& selectedData);
 static Library::FactoryPresets factoryPresets(const std::vector<uint8_t>& flash);
 bool scanROMs(const juce::File&,juce::String&);
 bool scanKSAssets(const juce::File&,const juce::Array<juce::File>&,juce::String&);
};
}
