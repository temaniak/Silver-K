#pragma once
#include "jucePluginLib/patchdb/db.h"
#include "silverPatch.h"
#include <atomic>
namespace silverK {
Model patchModel(const pluginLib::patchDB::Patch&);
juce::String patchName(const pluginLib::patchDB::Patch&);
bool emptyVStationSlot(const pluginLib::patchDB::Patch&);
class Library final:public pluginLib::patchDB::DB {
public:
 explicit Library(const juce::File& directory);
 ~Library()override;
 mutable std::atomic<unsigned> revision{0};
 mutable juce::String error;
 bool requestPatchForPart(pluginLib::patchDB::Data&,uint32_t,uint64_t)override{return false;}
 bool loadRomData(pluginLib::patchDB::DataList&,uint32_t,uint32_t)override{return false;}
 pluginLib::patchDB::PatchPtr initializePatch(pluginLib::patchDB::Data&&,const std::string&)override;
 pluginLib::patchDB::Data applyModifications(const pluginLib::patchDB::PatchPtr&,const pluginLib::FileType&,pluginLib::ExportType)const override;
 void processDirty(const pluginLib::patchDB::Dirty&)const override;
 bool parseFileData(pluginLib::patchDB::DataList&,const pluginLib::patchDB::Data&,const std::string&)override;
 bool loadFile(pluginLib::patchDB::DataList&,const std::string&)override;
 void addPath(const juce::File&,bool automatic=false);
 void removePath(const pluginLib::patchDB::DataSourceNodePtr&);
 void rescan(std::function<void()> completed={});
 bool isSourceEnabled(const pluginLib::patchDB::DataSource&)const override;
 void addFactory(const std::vector<std::vector<uint8_t>>&,const juce::String&);
 void requestBankSources(std::function<void(std::vector<pluginLib::patchDB::DataSourceNodePtr>&&) >,bool hideKSDrums=false);
protected:
 void onLoadFinished()override;
private:
 juce::File importVersion;
 juce::File sourceSettings;
 juce::String managedFolder;
 std::set<juce::String> excludedPaths;
 std::set<juce::String> includedPaths;
 void saveSources();
 bool migrateImports=false;
};
std::shared_ptr<Library> sharedLibrary(const juce::File&);
}
