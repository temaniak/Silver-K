#include "silverProcessor.h"
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
 juce::NamedValueSet values;values.set("editorScale",editorScale.load());values.set("skinChoice",skinChoice);values.set("customSkin",customSkin);values.set("bufferProfile",buffering.load());values.set("ksControllerMode",ksControllerMode());values.set("ksTwoCoreProcessing",ksTwoCoreProcessing());values.set("editorChannel",int(editorChannelPreference.load()));values.set("browserState",browserState);return values;
}
void Processor::savePreferences(){
 if(isFirmwareLoading())return;
 std::lock_guard guard(controlMutex);const auto values=preferenceValues();juce::NamedValueSet edits;
 for(int n=0;n<values.size();++n)if(preferenceBaseline[values.getName(n)]!=values.getValueAt(n))edits.set(values.getName(n),values.getValueAt(n));
 if(edits.size()==0)return;juce::String error;if(updateSettings(edits,error))preferenceBaseline=values;else message=error;
}
void Processor::initializeDataFolder(){
 const auto test=juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",{});
 const auto settings=juce::JSON::parse(settingsFile());const auto saved=settings["dataFolder"].toString();
 const auto fallback=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Silver-K");
 dataFolder=saved.isNotEmpty()?juce::File(saved):test.isNotEmpty()?juce::File(test).getChildFile("Silver-K"):fallback;
 const auto assetRoot=settings["assetRoot"].toString();const bool sameAssetRoot=assetRoot.isEmpty() || assetRoot==dataFolder.getFullPathName();
 auto selectedAsset=[&](const char* role){
  const auto path=settings[role].toString();if(path.isEmpty())return juce::File{};const juce::File file(path);if(file.existsAsFile())return file;
  const auto relative=settings[juce::Identifier(juce::String(role)+"Relative")].toString();
  if(sameAssetRoot && relative.isNotEmpty() && !juce::File::isAbsolutePath(relative)){const auto relocated=dataFolder.getChildFile(relative);if(relocated.isAChildOf(dataFolder) && relocated.existsAsFile())return relocated;}
  return file;
 };
 osFile=selectedAsset("os");bankFile=selectedAsset("bank");flashFile=selectedAsset("flash");
 restoreKSDataFiles(settings["ksDataFiles"],sameAssetRoot);
 for(const auto& file:selectedKSDataFiles())if(isBundledInit(file)){flashFile=juce::File{};flashFiles.clear();break;}
 const double scale=double(settings.getProperty("editorScale",.75));editorScale.store(std::isfinite(scale)&&scale>=.5&&scale<=1.5?scale:.75);
 skinChoice=settings.getProperty("skinChoice","Auto").toString();customSkin=settings["customSkin"].toString();browserState=settings["browserState"].toString();
 const int controller=int(settings.getProperty("ksControllerMode",ksControllerMode()));ksController.store(controller>=0 && controller<=2?ks::ControllerMode(controller):ks::ControllerMode::H8);
 twoCoreProcessing.store(bool(settings.getProperty("ksTwoCoreProcessing",ksTwoCoreProcessing())));
 buffering.store(juce::jlimit(0,3,int(settings.getProperty("bufferProfile",1))));controlChannel.store(uint8_t(int(settings["editorChannel"])&15));editorChannelPreference.store(controlChannel.load());
 juce::String error;setDataFolder(dataFolder,error);if(error.isNotEmpty())message=error;
 preferenceBaseline=preferenceValues();startupFirmware=osFile;startupPending.store(osFile!=juce::File{});
}
bool Processor::setDataFolder(const juce::File& folder,juce::String& error){
 std::lock_guard guard(controlMutex);
 if(folder==juce::File{}){error="Choose a Silver-K folder";return false;}
 const auto roms=folder.getChildFile("ROMs"),patches=folder.getChildFile("Patches"),skins=folder.getChildFile("Skins");
 for(const auto& dir:{roms,patches,skins})if(auto result=dir.createDirectory();result.failed()){error=result.getErrorMessage();return false;}
 dataFolder=folder;
 const auto zero=roms.getChildFile("ks-wave-zero.bin");
 if(!zero.exists()){
  juce::MemoryBlock bytes(ks::Board::WaveSize,true);
  if(!zero.replaceWithData(bytes.getData(),bytes.getSize())){error="Cannot create required zero Wave ROM";return false;}
 }
 if(bankFile==juce::File{})bankFile=zero;
 for(auto* asset:{&osFile,&bankFile})if(asset->existsAsFile()){const auto managed=managedAsset(*asset,error);if(managed==juce::File{})return false;*asset=managed;}
 const auto selected=selectedKSDataFiles();std::vector<juce::File> managedData;
 for(const auto& file:selected){if(!file.existsAsFile()){managedData.push_back(file);continue;}const auto managed=managedAsset(file,error);if(managed==juce::File{})return false;managedData.push_back(managed);}
 if(!managedData.empty()){flashFiles=std::move(managedData);flashFile=flashFiles.front();}
 const auto settings=juce::JSON::parse(settingsFile());
 const bool cached=settings["dataFolder"].toString()==folder.getFullPathName() && int(settings["firmwareIndexVersion"])==1 && settings["firmwares"].isArray();
 romFolder=roms;
 if(cached){
  firmwares.clear();for(const auto& item:*settings["firmwares"].getArray()){
   const auto relative=item["file"].toString();if(relative.isEmpty() || juce::File::isAbsolutePath(relative))continue;
   const auto file=roms.getChildFile(relative);if(file.isAChildOf(roms) && file.existsAsFile() && firmwares.size()<1024)firmwares.push_back({file,bool(item["kStation"])});
  }
 }else{juce::String scan;setRomFolder(roms,scan);}
 // A cached firmware index is still useful when the required Data has not
 // been selected. Discover a real All Data file without creating substitutes.
 if(flashFile==juce::File{})for(const auto& file:roms.findChildFiles(juce::File::findFiles,false,"*"))
  if(file.hasFileExtension("mid;midi") && file.getFileName().containsIgnoreCase("all data")){
   std::vector<uint8_t> storage;juce::String ignored;if(readKSData({file},storage,ignored)){flashFile=file;flashFiles={file};break;}
  }
 patchLibrary.addPath(patches,true);
 return rememberDataFolder(error) && rememberAssets(error);
}
bool Processor::rememberDataFolder(juce::String& error){
 juce::NamedValueSet values;values.set("dataFolder",dataFolder.getFullPathName());
 juce::Array<juce::var> catalog;for(const auto& choice:firmwares)if(choice.file.isAChildOf(dataFolder.getChildFile("ROMs"))){
  juce::DynamicObject::Ptr entry=new juce::DynamicObject;entry->setProperty("file",choice.file.getRelativePathFrom(dataFolder.getChildFile("ROMs")));entry->setProperty("kStation",choice.kStation);catalog.add(juce::var(entry.get()));
 }
 values.set("firmwareIndexVersion",1);values.set("firmwares",catalog);return updateSettings(values,error);
}
bool Processor::rememberAssets(juce::String& error){
 juce::NamedValueSet values;values.set("assetRoot",dataFolder.getFullPathName());
 for(const auto& asset:{std::pair<const char*,juce::File>{"os",osFile},{"bank",bankFile},{"flash",flashFile}}){
  values.set(asset.first,asset.second.getFullPathName());values.set(juce::String(asset.first)+"Relative",asset.second.isAChildOf(dataFolder)?asset.second.getRelativePathFrom(dataFolder):juce::String{});
 }
 values.set("ksDataFiles",ksDataReferences());return updateSettings(values,error);
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
