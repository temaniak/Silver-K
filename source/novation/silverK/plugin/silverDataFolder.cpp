#include "silverProcessor.h"
#include "baseLib/sha256.h"
namespace silverK {
juce::File Processor::settingsFile()const{
 const auto test=juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",{});
 return (test.isEmpty()?juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("The Usual Suspects/Silver-K"):juce::File(test)).getChildFile("settings.json");
}
bool Processor::updateSettings(const juce::NamedValueSet& edits,juce::String& error)const{
 // Merge changed keys so closing another instance cannot restore stale defaults.
 static std::mutex settingsMutex;std::lock_guard guard(settingsMutex);
 const auto file=settingsFile();juce::InterProcessLock lock("Silver-K-settings-"+juce::String::toHexString(file.getFullPathName().hashCode64()));
 if(!lock.enter(250)){error="Settings are busy; try again";return false;}
 struct Unlock{juce::InterProcessLock& lock;~Unlock(){lock.exit();}} unlock{lock};
 if(auto result=file.getParentDirectory().createDirectory();result.failed()){error=result.getErrorMessage();return false;}
 auto saved=juce::JSON::parse(file);if(!saved.isObject())saved=juce::var(new juce::DynamicObject);
 bool changed=false;for(int n=0;n<edits.size();++n){const auto key=edits.getName(n);const auto& value=edits.getValueAt(n);if(!saved.hasProperty(key)||saved[key]!=value){saved.getDynamicObject()->setProperty(key,value);changed=true;}}
 if(!changed)return true;
 juce::TemporaryFile temp(file);
 if(!temp.getFile().replaceWithText(juce::JSON::toString(saved)) || !temp.overwriteTargetFileWithTemporary()){error="Cannot save Silver-K settings: "+file.getFullPathName();return false;}
 return true;
}
juce::NamedValueSet Processor::preferenceValues()const{
 juce::NamedValueSet values;values.set("editorScale",editorScale.load());values.set("skinChoice",skinChoice);values.set("customSkin",customSkin);values.set("bufferProfile",buffering.load());values.set("ksControllerMode",ksControllerMode());values.set("ksTwoCoreProcessing",ksTwoCoreProcessing());values.set("editorChannel",int(editorChannelPreference.load()));values.set("browserState",browserState);values.set("firmwareSwitch",int(firmwareSwitch()));return values;
}
void Processor::setEditorScaleFromResize(double scale){
 if(editorScale.load()==scale)return;
 lastEditorResizeMs.store(juce::Time::getMillisecondCounterHiRes());
 editorScale.store(scale);
}
void Processor::savePreferences(bool flushEditorScale){
 if(isFirmwareLoading())return;
 std::lock_guard guard(controlMutex);
 const auto values=preferenceValues();
 const auto resizedAt=lastEditorResizeMs.load();
 const bool resizing=!flushEditorScale && resizedAt!=0. && juce::Time::getMillisecondCounterHiRes()-resizedAt<250.;
 juce::NamedValueSet edits;
 for(int n=0;n<values.size();++n){
  const auto key=values.getName(n);
  if(resizing && key==juce::Identifier("editorScale"))continue;
  const auto& value=values.getValueAt(n);
  if(preferenceBaseline[key]!=value)edits.set(key,value);
 }
 if(edits.size()==0)return;
 juce::String error;
 if(!updateSettings(edits,error)){message=error;return;}
 for(int n=0;n<edits.size();++n)preferenceBaseline.set(edits.getName(n),edits.getValueAt(n));
}
void Processor::initializeDataFolder(){
 const auto test=juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",{});
 const auto settings=juce::JSON::parse(settingsFile());const auto saved=settings["dataFolder"].toString();
 const auto fallback=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Silver-K");
 const auto profile=test.isEmpty()?juce::File{}:juce::File(test);
 const auto savedFolder=saved.isEmpty()?juce::File{}:juce::File(saved);
 const bool isolatedStandalone=profile!=juce::File{} && wrapperType==wrapperType_Standalone;
 // A previous Standalone could save the global data folder into a test profile.
 const bool foreignFolder=isolatedStandalone && savedFolder!=juce::File{} && savedFolder!=profile &&
                          !savedFolder.isAChildOf(profile);
 dataFolder=savedFolder!=juce::File{} && !foreignFolder?savedFolder:
            profile!=juce::File{}?profile.getChildFile("Silver-K"):fallback;
 const auto assetRoot=settings["assetRoot"].toString();const bool sameAssetRoot=assetRoot.isEmpty() || assetRoot==dataFolder.getFullPathName();
 const bool restoreAssets=!foreignFolder && (!isolatedStandalone || sameAssetRoot);
 auto selectedAsset=[&](const char* role){
  const auto path=settings[role].toString();if(path.isEmpty())return juce::File{};const juce::File file(path);if(file.existsAsFile())return file;
  const auto relative=settings[juce::Identifier(juce::String(role)+"Relative")].toString();
  if(sameAssetRoot && relative.isNotEmpty() && !juce::File::isAbsolutePath(relative)){const auto relocated=dataFolder.getChildFile(relative);if(relocated.isAChildOf(dataFolder) && relocated.existsAsFile())return relocated;}
  return file;
 };
 if(restoreAssets){
  osFile=selectedAsset("os");bankFile=selectedAsset("bank");flashFile=selectedAsset("flash");
  restoreKSDataFiles(settings["ksDataFiles"],sameAssetRoot);
 }
 const double scale=double(settings.getProperty("editorScale",.75));editorScale.store(std::isfinite(scale)&&scale>=.5&&scale<=1.5?scale:.75);
 skinChoice=settings.getProperty("skinChoice",defaultSkin).toString();customSkin=settings["customSkin"].toString();browserState=settings["browserState"].toString();
 twoCoreProcessing.store(bool(settings.getProperty("ksTwoCoreProcessing",ksTwoCoreProcessing())));
 firmwareSwitchMode.store(int(settings["firmwareSwitch"])==int(FirmwareSwitch::RecallLast)?FirmwareSwitch::RecallLast:FirmwareSwitch::KeepPatch);
 buffering.store(juce::jlimit(0,3,int(settings.getProperty("bufferProfile",1))));controlChannel.store(uint8_t(int(settings["editorChannel"])&15));editorChannelPreference.store(controlChannel.load());
 juce::String error;
 if(setDataFolder(dataFolder,error))startupFirmware=firmwareAfterFolderScan();
 else message=error;
 preferenceBaseline=preferenceValues();startupPending.store(startupFirmware!=juce::File{});
}
bool Processor::setDataFolder(const juce::File& folder,juce::String& error,bool rescanPresets){
 std::lock_guard guard(controlMutex);
 error.clear();
 if(folder==juce::File{}){error="Choose a Silver-K folder";return false;}
 const auto roms=folder.getChildFile("ROMs"),patches=folder.getChildFile("Patches"),skins=folder.getChildFile("Skins");
 for(const auto& dir:{roms,patches,skins})if(auto result=dir.createDirectory();result.failed()){error=result.getErrorMessage();return false;}
 // Switching roots changes discovery, without copying assets from the running synth.
 if(!scanROMs(roms,error))return false;
 if(folder!=dataFolder)startupPending.store(false);
 dataFolder=folder;
 const auto user=userPatchesFolder();
 if(auto result=user.createDirectory();result.failed()){error=result.getErrorMessage();return false;}
 struct Template { const char* name; Model model; bool performance=false, drums=false; };
 juce::Array<juce::File> templates;
 for(const Template& entry:std::initializer_list<Template>{
     {"Init KS",Model::KS},{"Init K-Station",Model::KStation},
     {"Init Drum Map",Model::KS,false,true},{"Init Performance",Model::KS,true}}){
  const auto file=user.getChildFile(entry.name).withFileExtension(NativePatchExtension);
  templates.add(file);
  if(file.exists())continue;
  const auto bytes=encodeNativePatch(initPatch(entry.model,entry.performance,entry.drums),entry.name);
  // A second instance may create the same library while the first opens.
  juce::TemporaryFile temporary(file);
  if(bytes.empty() || !temporary.getFile().replaceWithData(bytes.data(),bytes.size()) ||
     (!file.exists() && !temporary.getFile().moveFileTo(file))){
   error="Could not create "+file.getFileName();
   return false;
  }
 }
 patchLibrary.addPath(patches,true);
 patchLibrary.discoverUserPresets(user,std::move(templates));
 if(rescanPresets)patchLibrary.rescan();
 return rememberDataFolder(error) && rememberAssets(error);
}
juce::File Processor::firmwareAfterFolderScan()const{
 std::lock_guard guard(controlMutex);
 const bool ksReady=bankFile.existsAsFile() && flashFile.existsAsFile();
 if(firmwareReady()){
  for(const auto& choice:firmwares)if(choice.kStation==isKStation()){
   if(choice.kStation)return choice.file!=osFile?choice.file:juce::File{};
   if(ksReady && (choice.file!=osFile || loadedWaveFile!=bankFile || loadedDataFiles!=selectedKSDataFiles()
       || scannedDataHash!=ksDataHash || scannedWaveHash!=juce::String(baseLib::SHA256(bank).toString())))return choice.file;
  }
  return {};
 }
 for(const auto& choice:firmwares)if(choice.file==osFile && (choice.kStation || ksReady))return choice.file;
 for(const auto& choice:firmwares)if(!choice.kStation && ksReady)return choice.file;
 for(const auto& choice:firmwares)if(choice.kStation)return choice.file;
 return {};
}
juce::String Processor::dataFolderStatus()const{
 if(isFirmwareLoading())return "Loading firmware...";
 std::lock_guard guard(controlMutex);
 const auto has=[this](bool k){return std::any_of(firmwares.begin(),firmwares.end(),[k](const auto& choice){return choice.kStation==k;});};
 return juce::String("K-Station firmware: ")+(has(true)?"found":"missing")+"\nKS firmware: "+(has(false)?"found":"missing")
  +" · KS Data: "+(flashFile==juce::File{}?"missing":"found")+"\nWave ROM: "+(bankFile==juce::File{}?juce::String("missing"):bankFile.getFileName());
}
bool Processor::rememberDataFolder(juce::String& error){
 juce::NamedValueSet values;values.set("dataFolder",dataFolder.getFullPathName());
 return updateSettings(values,error);
}
bool Processor::rememberAssets(juce::String& error){
 juce::NamedValueSet values;values.set("assetRoot",dataFolder.getFullPathName());
 const auto roms=dataFolder.getChildFile("ROMs");
 for(const auto& asset:{std::pair<const char*,juce::File>{"os",osFile},{"bank",bankFile},{"flash",flashFile}}){
  const bool inFolder=asset.second.isAChildOf(roms) && asset.second.existsAsFile();
  values.set(asset.first,inFolder?asset.second.getFullPathName():juce::String{});
  values.set(juce::String(asset.first)+"Relative",inFolder?asset.second.getRelativePathFrom(dataFolder):juce::String{});
 }
 values.set("ksDataFiles",ksDataReferences(selectedKSDataFiles()));return updateSettings(values,error);
}
bool Processor::setWaveRom(const juce::File& file,juce::String& error){
 std::lock_guard guard(controlMutex);
 if(file.getSize()!=ks::Board::WaveSize){error="Wave ROM must contain exactly 129024 bytes";return false;}
 const auto managed=managedAsset(file,error);if(managed==juce::File{})return false;
 bankFile=managed;return rememberAssets(error);
}
bool Processor::setFlashRom(const juce::File& file,juce::String& error){return setFlashRoms({file},error);}
bool Processor::setFlashRoms(const juce::Array<juce::File>& files,juce::String& error){
 std::lock_guard guard(controlMutex);
 std::vector<juce::File> chosen(files.begin(),files.end());std::vector<uint8_t> storage;
 if(!readKSData(chosen,storage,error))return false;
 std::vector<juce::File> managed;for(const auto& file:chosen){auto saved=managedAsset(file,error);if(saved==juce::File{})return false;managed.push_back(saved);}
 flashFiles=std::move(managed);flashFile=flashFiles.front();return rememberAssets(error);
}
juce::File Processor::resolveAsset(const juce::ValueTree& state,const char* role)const{
 const auto relative=state.getProperty(juce::String(role)+"Relative").toString();
 if(relative.isNotEmpty() && !juce::File::isAbsolutePath(relative)){const auto candidate=dataFolder.getChildFile(relative);if(candidate.isAChildOf(dataFolder) && candidate.existsAsFile())return candidate;}
 const auto path=state.getProperty(role).toString();if(path.isEmpty())return {};
 const auto old=juce::File(path);
 if(old.getFileName().isNotEmpty()){const auto relocated=dataFolder.getChildFile("ROMs").getChildFile(old.getFileName());if(relocated.existsAsFile())return relocated;}
 return old;
}
}
