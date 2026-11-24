#pragma once
#include "jucePluginLib/patchdb/db.h"
#include "silverPatch.h"
#include <atomic>
#include <mutex>
namespace silverK {
Model patchModel(const pluginLib::patchDB::Patch&);
PatchBytes patchDump(const pluginLib::patchDB::Patch&);
juce::String patchModelName(const pluginLib::patchDB::Patch&);
bool isKSPlus(const pluginLib::patchDB::Patch&);
bool isFactoryInit(const pluginLib::patchDB::Patch&);
juce::String patchName(const pluginLib::patchDB::Patch&);
bool emptyVStationSlot(const pluginLib::patchDB::Patch&);
juce::String presetKind(const pluginLib::patchDB::Patch&);
juce::StringArray presetFacets(const pluginLib::patchDB::Patch&);
class Library final:public pluginLib::patchDB::DB {
public:
 struct PresetPath {
  pluginLib::patchDB::DataSourceNodePtr source;
  bool active=true;
 };
 explicit Library(const juce::File& directory);
 ~Library()override;
 mutable std::atomic<unsigned> revision{0};
 mutable juce::String error;
 bool requestPatchForPart(pluginLib::patchDB::Data&,uint32_t,uint64_t)override{return false;}
 bool loadRomData(pluginLib::patchDB::DataList&,uint32_t,uint32_t)override{return false;}
 pluginLib::patchDB::PatchPtr initializePatch(pluginLib::patchDB::Data&&,const std::string&)override;
 pluginLib::patchDB::Data applyModifications(const pluginLib::patchDB::PatchPtr&,const pluginLib::FileType&,pluginLib::ExportType)const override;
 bool writePatchesToFile(const juce::File&,const std::vector<pluginLib::patchDB::PatchPtr>&)override;
 void processDirty(const pluginLib::patchDB::Dirty&)const override;
 bool modifyTags(const std::vector<pluginLib::patchDB::PatchPtr>&,const pluginLib::patchDB::TypedTags&);
 bool parseFileData(pluginLib::patchDB::DataList&,const pluginLib::patchDB::Data&,const std::string&)override;
 bool loadFile(pluginLib::patchDB::DataList&,const std::string&)override;
 void addPath(const juce::File&,bool automatic=false);
 void discoverUserPresets(const juce::File& userFolder,juce::Array<juce::File> templates);
 void indexUserPreset(const juce::File& file,const juce::File& userFolder);
 void removePath(const pluginLib::patchDB::DataSourceNodePtr&);
 void setPresetPathActive(juce::File,bool);
 bool presetPathActive(const pluginLib::patchDB::DataSource&)const;
 void requestPresetCatalog(bool hideKSDrums,std::function<void(std::vector<PresetPath>,juce::StringArray)>);
 void rescan(std::function<void()> completed={});
 std::map<std::string,size_t> sourceReadOrder()const;
 bool isSourceEnabled(const pluginLib::patchDB::DataSource&)const override;
 void addFactory(const std::vector<std::vector<uint8_t>>&,const juce::String&);
 void requestBankSources(std::function<void(std::vector<pluginLib::patchDB::DataSourceNodePtr>&&) >,bool hideKSDrums=false);
protected:
 bool isValidLoadedPatch(const pluginLib::patchDB::PatchPtr&)const override;
 void onLoadFinished()override;
private:
 juce::File importVersion;
 juce::File sourceSettings;
 juce::String managedFolder;
 std::set<juce::String> excludedPaths;
 std::set<juce::String> includedPaths;
 std::set<juce::String> inactivePaths;
 mutable std::mutex inactivePathsMutex;
 mutable std::mutex orderMutex;
 std::map<std::string,size_t> readOrder;
 bool orderSaveQueued=false;
 void recordSourceRead(const std::string&);
 bool parsePresetFile(pluginLib::patchDB::DataList&,const pluginLib::patchDB::Data&,const std::string&);
 std::map<std::pair<std::string,juce::String>,pluginLib::patchDB::DataList> decodedFiles;
 size_t decodedBytes=0;
 void saveSources();
 bool migrateImports=false;
};
std::shared_ptr<Library> sharedLibrary(const juce::File&);
}
