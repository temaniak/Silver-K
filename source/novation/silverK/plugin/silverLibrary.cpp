#include "silverLibrary.h"
#include "silverVStation.h"
#include <cstring>
#include "baseLib/md5.h"
#include "baseLib/sha256.h"
#include "jucePluginLib/patchdb/search.h"
#include "jucePluginLib/patchdb/patchmodifications.h"
#include "jucePluginLib/filetype.h"
namespace silverK {
using namespace pluginLib::patchDB;
namespace {
constexpr uint8_t NamedRecord[]{'S','K','P','N'};
juce::String numberedName(Model model,unsigned number){return modelName(model)+" "+juce::String(number).paddedLeft('0',3);}
Data namedRecord(const Data& sysex,const juce::String& name){Data record(std::begin(NamedRecord),std::end(NamedRecord));record.insert(record.end(),sysex.begin(),sysex.end());const auto utf8=name.toRawUTF8();record.insert(record.end(),utf8,utf8+std::strlen(utf8));return record;}
bool unpackNamedRecord(Data& record,std::string& name){
 if(record.size()<=146 || record.size()>146+512 || !std::equal(std::begin(NamedRecord),std::end(NamedRecord),record.begin()) || !validPatch(record.data()+4,142))return false;
 name=juce::String::fromUTF8(reinterpret_cast<const char*>(record.data()+146),int(record.size()-146)).trim().toStdString();
 Data sysex(record.begin()+4,record.begin()+146);record=std::move(sysex);return true;
}
}
PatchBytes patchDump(const Patch& patch){return patchDump(PatchBytes(patch.sysex));}
bool isKSPlus(const Patch& patch){return nativePatch(patch.sysex).extended();}
juce::String patchModelName(const Patch& patch){return isKSPlus(patch)?"KS+":modelName(patchModel(patch));}
Model patchModel(const Patch& patch){
 const auto dump=patchDump(patch);
 const auto model=modelOf(dump.data(),dump.size());
 // CustomA is the source synth type. Recognize the old release's V tag too,
 // so cached banks keep their identity before the one-time source refresh.
 if(model==Model::KStation && (patch.tags.containsAdded(TagType::CustomA,"V-Station") || patch.tags.containsAdded(TagType::Tag,"V-Station")))return Model::VStation;
 return model;
}
bool isFactoryInit(const Patch& patch){
 const auto dump=patchDump(patch);
 if(dump.empty() || dump.size()!=patch.sysex.size() || ksDrum(dump.data(),dump.size()))return false;
 const juce::CharPointer_UTF8 name(patch.name.c_str());
 const auto named=[name](const char* label){return name.compareIgnoreCase(juce::CharPointer_UTF8(label))==0;};
 if(dump.size()==1038){
  if(named("INIT PERF") || named("INIT PERFORMANCE"))return true;
  // The original KS factory data calls its unprogrammed Performances
  // "PERFORMANCE", with "PROGRAM NAME" in all four part records.
  if(!named("PERFORMANCE"))return false;
  for(size_t part=0;part<4;++part){
   const auto* label=reinterpret_cast<const char*>(dump.data()+13+part*256+151);
   if(juce::String::fromUTF8(label,16).trim()!="PROGRAM NAME")return false;
  }
  return true;
 }
 return named("INIT PROG") || named("INIT PROGRAM");
}
// This classifies bank names, not silence; these sounds must never be discarded.
bool emptyVStationSlot(const Patch& patch){return patch.tags.containsAdded(TagType::CustomB,"V-Station user slot") && !(patch.modifications && !patch.modifications->name.empty());}
juce::String presetKind(const Patch& patch){
 const auto dump=patchDump(patch);
 if(patchModel(patch)==Model::VStation)return dump[7]==1?"Bank program":"Preset";
 return patchType(dump.data(),dump.size());
}
juce::StringArray presetFacets(const Patch& patch){
 const auto model=patchModel(patch);
 const auto kind=presetKind(patch);
 const auto dump=patchDump(patch);
 const auto genericKind=patchType(dump.data(),dump.size());
 juce::StringArray values{"Synth / "+patchModelName(patch),"Type / "+kind};
 // KS OS 2.1.02, category field 176 in the Novation KS SysEx specification.
 static constexpr const char* ksCategories[]{"Bass","Arpeggio","Motion","Dance","Pad","EP/Clav",
  "Strings","Brass","Organ","Sweeps","Soft Lead","Hard Lead","Bells","SFX","Vocoder","Audio Trigger",
  "Drums","User 1","User 2","User 3","User 4","User 5","User 6","User 7"};
 if(model==Model::KS){
  if(ksDrum(dump.data(),dump.size()))values.add("Instrument / Drums");
  else if(dump.size()==270){
   const auto category=dump[13+176];
   if(category<std::size(ksCategories))values.add("Instrument / "+juce::String(ksCategories[category]));
  }
 }
 for(const auto& tag:patch.getTags(TagType::Category).getAdded()){
  const juce::String label(tag);
  if(label.isNotEmpty() && !label.equalsIgnoreCase(kind) && !label.equalsIgnoreCase(genericKind))values.addIfNotAlreadyThere("Instrument / "+label);
 }
 for(const auto& tag:patch.getTags(TagType::Tag).getAdded())
  if(!tag.empty())values.add("Tag / "+juce::String(tag));
 return values;
}
juce::String patchName(const Patch& patch){
 const auto name=juce::String(patch.getName()).trim();const auto model=patchModel(patch);
 if(patch.modifications && !patch.modifications->name.empty())return name;
 if((model==Model::KStation || model==Model::AStation) && (name.isEmpty() || name.containsOnly("0123456789")))return numberedName(model,patch.program==g_invalidProgram?1:patch.program+1);
 return name;
}
std::shared_ptr<Library> sharedLibrary(const juce::File& dir){static std::mutex mutex;static std::map<juce::String,std::weak_ptr<Library>> instances;std::lock_guard guard(mutex);auto& weak=instances[dir.getFullPathName()];if(auto existing=weak.lock())return existing;auto library=std::make_shared<Library>(dir);weak=library;return library;}

Library::Library(const juce::File& dir):DB(dir),importVersion(dir.getChildFile("silver-import-version.txt")),sourceSettings(dir.getChildFile("silver-sources.json")),migrateImports(importVersion.loadFileAsString().trim()!="6"){
 const auto saved=juce::JSON::parse(sourceSettings);managedFolder=saved["managedFolder"].toString();
 if(const auto* paths=saved["excluded"].getArray())for(const auto& path:*paths)if(juce::File::isAbsolutePath(path.toString()))excludedPaths.insert(path.toString());
 if(const auto* paths=saved["included"].getArray())for(const auto& path:*paths)if(juce::File::isAbsolutePath(path.toString()))includedPaths.insert(path.toString());
 if(const auto* paths=saved["inactive"].getArray())for(const auto& path:*paths)
  if(juce::File::isAbsolutePath(path.toString()))inactivePaths.insert(path.toString());
 if(const auto* paths=saved["readOrder"].getArray())for(const auto& path:*paths)
  if(juce::File::isAbsolutePath(path.toString()))readOrder.try_emplace(path.toString().toStdString(),readOrder.size());
 startLoaderThread();
 runOnLoaderThread([this]{
  std::vector<DataSourceNodePtr> all;getDataSources(all);for(const auto& ds:all)if(!isSourceEnabled(*ds))removeDataSource(*ds,true,true);
  // Older cached sources predate the first-read index. They precede new imports.
  for(const auto& ds:all)if(ds->type==SourceType::File && !ds->patches.empty())recordSourceRead(ds->name);
  // A specifically reconnected child stays connected even when its parent is excluded.
  for(const auto& path:includedPaths)runOnLoaderThread([this,path]{const juce::File file(path);if(!file.exists())return;DataSource ds;ds.type=file.isDirectory()?SourceType::Folder:SourceType::File;ds.name=path.toStdString();ds.origin=DataSourceOrigin::Manual;if(!getDataSource(ds))addDataSource(ds);});
 });
 if(migrateImports)runOnLoaderThread([this]{std::vector<DataSourceNodePtr> sources;getDataSources(sources);for(const auto& ds:sources)if(ds->origin==DataSourceOrigin::Manual && (ds->type==SourceType::File || ds->type==SourceType::Folder) && juce::File(ds->name).exists())refreshDataSource(ds);});
}
Library::~Library(){stopLoaderThread();}
void Library::onLoadFinished(){if(migrateImports && importVersion.replaceWithText("6\n"))migrateImports=false;}
PatchPtr Library::initializePatch(Data&& data,const std::string& fallback){
 if(const auto native=nativePatch(data);native){
  if(!validNativePatch(native))return {};
  auto patch=std::make_shared<Patch>();
  patch->name=juce::String::fromUTF8(reinterpret_cast<const char*>(native.name.data()),int(native.name.size())).toStdString();
  patch->bank=native.dump[11];
  patch->program=native.dump[12];
  patch->tags.add(TagType::CustomA,modelName(modelOf(native.dump.data(),native.dump.size())).toStdString());
  patch->tags.add(TagType::Category,patchType(native.dump.data(),native.dump.size()).toStdString());
  const baseLib::MD5 digest(data.data(),uint32_t(data.size()));
  std::memcpy(patch->hash.data(),digest.getWords().data(),patch->hash.size());
  patch->sysex=std::move(data);
  return patch;
 }
 const auto original=data;std::string importedName;const bool vStation=unpackVStationPatch(data,importedName);if(!vStation)unpackNamedRecord(data,importedName);if(!validPatch(data))return {};auto p=std::make_shared<Patch>();const auto model=modelOf(data);p->sysex=std::move(data);p->bank=p->sysex[11];p->program=p->sysex[12];const bool perf=p->sysex.size()==1038;const unsigned offset=perf?192:(model==Model::KS || model==Model::XStation)?151:0;
 if(model==Model::KS || model==Model::XStation){juce::String name;for(unsigned i=offset;i<offset+16;++i){auto c=p->sysex[13+i];name+=juce::String::charToString(c>=32 && c<127?c:' ');}p->name=name.trim().toStdString();}if(!importedName.empty())p->name=importedName;if(p->name.empty()){const auto stem=juce::String(fallback).trim();p->name=(stem.isNotEmpty()?stem:numberedName(vStation?Model::VStation:model,p->bank*100+p->program+1)).toStdString();}
 if(drumMap(p->sysex.data(),p->sysex.size())){
  juce::String name;for(unsigned i=151;i<167;++i){const auto c=p->sysex[49*270+13+i];name+=juce::String::charToString(c>=32 && c<127?c:' ');}
  p->name=("Drum Map "+juce::String(p->bank)+(name.trim().isNotEmpty()?" - "+name.trim():juce::String{})).toStdString();
 }
 p->tags.add(TagType::CustomA,modelName(vStation?Model::VStation:model).toStdString());p->tags.add(TagType::Category,patchType(p->sysex.data(),p->sysex.size()).toStdString());p->setHashFromMessages(13,1);if(vStation){
 const auto label=juce::String(importedName).trim();const auto suffix=label.substring(5);
 if(p->sysex[7]==1 && (label.isEmpty() || (label.startsWithIgnoreCase("User ") && suffix.isNotEmpty() && suffix.containsOnly("0123456789"))))p->tags.add(TagType::CustomB,"V-Station user slot");
 p->tags.add(TagType::Tag,"V-Station");const baseLib::MD5 digest(original.data(),uint32_t(original.size()));std::memcpy(p->hash.data(),digest.getWords().data(),p->hash.size());}return p;}
bool Library::isValidLoadedPatch(const PatchPtr& patch)const{
 return DB::isValid(patch) || (patch && bool(nativePatch(patch->sysex)));
}
Data Library::applyModifications(const PatchPtr& patch,const pluginLib::FileType& type,pluginLib::ExportType target)const{
 if(!patch)return {};
 const auto native=nativePatch(patch->sysex);
 if(!native)return patch->sysex;
 if(target==pluginLib::ExportType::File && type.type()==NativePatchExtension)return patch->sysex;
 if(native.extended())return {};
 Data output;
 for(const auto map:native.drumMaps)if(!map.empty())output.insert(output.end(),map.begin(),map.end());
 output.insert(output.end(),native.dump.begin(),native.dump.end());
 return output;
}
bool Library::writePatchesToFile(const juce::File& file,const std::vector<PatchPtr>& patches){
 if(patches.empty() || std::any_of(patches.begin(),patches.end(),[](const auto& patch){return !patch;}))return false;
 const bool native=file.hasFileExtension(NativePatchExtension);
 if(native && (patches.size()!=1 || !nativePatch(patches.front()->sysex)))return false;
 if(!native && std::all_of(patches.begin(),patches.end(),[](const auto& patch){
  return !patch->sysex.empty() && patch->sysex.front()==0xf0;
 }))return DB::writePatchesToFile(file,patches);
 Data output;
 for(const auto& patch:patches){
  const auto data=applyModifications(patch,native?pluginLib::FileType(NativePatchExtension):pluginLib::FileType::Syx,
                                    pluginLib::ExportType::File);
  if(data.empty())return false;
  output.insert(output.end(),data.begin(),data.end());
 }
 return file.replaceWithData(output.data(),output.size());
}
void Library::processDirty(const Dirty& d)const{if(d.patches || d.dataSources || !d.tags.empty())++revision;if(!d.errors.empty())error=d.errors.back();}
bool Library::modifyTags(const std::vector<PatchPtr>& patches,const TypedTags& tags){
 if(!DB::modifyTags(patches,tags))return false;
 ++revision;
 return true;
}
bool Library::isSourceEnabled(const DataSource& ds)const{
 if(ds.type!=SourceType::File && ds.type!=SourceType::Folder)return true;
 const juce::File file(ds.name);
 int denied=-1,allowed=-1;
 for(const auto& path:excludedPaths)if(file==juce::File(path) || file.isAChildOf(juce::File(path)))denied=std::max(denied,path.length());
 for(const auto& path:includedPaths)if(file==juce::File(path) || file.isAChildOf(juce::File(path)))allowed=std::max(allowed,path.length());
 return denied<0 || allowed>denied;
}
void Library::saveSources(){
 juce::DynamicObject::Ptr settings=new juce::DynamicObject;settings->setProperty("managedFolder",managedFolder);
 juce::Array<juce::var> paths;for(const auto& path:excludedPaths)paths.add(path);settings->setProperty("excluded",paths);
 paths.clear();for(const auto& path:includedPaths)paths.add(path);settings->setProperty("included",paths);
 paths.clear();for(const auto& path:inactivePaths)paths.add(path);settings->setProperty("inactive",paths);
 const auto order=sourceReadOrder();
 std::vector<std::string> byRank(order.size());
 for(const auto& [path,rank]:order)byRank[rank]=path;
 paths.clear();for(const auto& path:byRank)paths.add(juce::String(path));settings->setProperty("readOrder",paths);
 juce::TemporaryFile temp(sourceSettings);if(!temp.getFile().replaceWithText(juce::JSON::toString(juce::var(settings.get()))) || !temp.overwriteTargetFileWithTemporary())runOnUiThread([this]{error="Cannot save preset source settings: "+sourceSettings.getFullPathName();});
}
std::map<std::string,size_t> Library::sourceReadOrder()const{
 std::lock_guard guard(orderMutex);
 return readOrder;
}
void Library::recordSourceRead(const std::string& path){
 {
  std::lock_guard guard(orderMutex);
  if(!readOrder.try_emplace(path,readOrder.size()).second)return;
 }
 if(orderSaveQueued)return;
 orderSaveQueued=true;
 runOnLoaderThread([this]{orderSaveQueued=false;saveSources();});
}
void Library::addPath(const juce::File& f,bool automatic){
 runOnLoaderThread([this,f,automatic]{
  DataSource ds;ds.type=f.isDirectory()?SourceType::Folder:SourceType::File;ds.origin=DataSourceOrigin::Manual;ds.name=f.getFullPathName().toStdString();
  if(automatic){
   if(managedFolder.isNotEmpty() && managedFolder!=f.getFullPathName()){
    DataSource old;old.type=SourceType::Folder;old.name=managedFolder.toStdString();removeDataSource(old,true,true);
   }
   managedFolder=f.getFullPathName();
  }else{
   {
    const std::lock_guard lock(inactivePathsMutex);
    inactivePaths.erase(f.getFullPathName());
   }
   // Reconnect the chosen subtree, without enabling its excluded ancestors.
   for(auto it=excludedPaths.begin();it!=excludedPaths.end();)if(f==juce::File(*it) || juce::File(*it).isAChildOf(f))it=excludedPaths.erase(it);else ++it;
   for(auto it=includedPaths.begin();it!=includedPaths.end();)if(f==juce::File(*it) || juce::File(*it).isAChildOf(f))it=includedPaths.erase(it);else ++it;
   includedPaths.insert(f.getFullPathName());
  }
  saveSources();if(!isSourceEnabled(ds))return;
  if(auto existing=getDataSource(ds)){if(!automatic)refreshDataSource(existing);return;}
  addDataSource(ds);
 });
}
void Library::discoverUserPresets(const juce::File& userFolder,juce::Array<juce::File> templates){
 runOnLoaderThread([this,userFolder,templates=std::move(templates)]{
  const auto patches=userFolder.getParentDirectory().getFullPathName();
  if(patches!=managedFolder)return;
  DataSource root;
  root.type=SourceType::Folder;
  root.name=patches.toStdString();
  const auto parent=getDataSource(root);
  // A new Patches root will scan User as part of its first loader job.
  if(!parent || !isSourceEnabled(*parent))return;
  DataSource user;
  user.type=SourceType::Folder;
  user.name=userFolder.getFullPathName().toStdString();
  user.origin=DataSourceOrigin::Autogenerated;
  if(!isSourceEnabled(user))return;
  if(const auto existing=getDataSource(user)){
   for(const auto& file:templates){
    DataSource preset;
    preset.type=SourceType::File;
    preset.name=file.getFullPathName().toStdString();
    preset.origin=DataSourceOrigin::Autogenerated;
    if(getDataSource(preset))continue;
    auto child=std::make_shared<DataSourceNode>(preset);
    child->setParent(existing);
    addDataSource(child);
   }
  }else{
   // Cached Patches can predate the creation of User. Scan only this subtree.
   auto child=std::make_shared<DataSourceNode>(user);
   child->setParent(parent);
   addDataSource(child);
  }
 });
}
void Library::indexUserPreset(const juce::File& file,const juce::File& userFolder){
 runOnLoaderThread([this,file,userFolder]{
  if(!file.isAChildOf(userFolder) || !file.existsAsFile())return;
  DataSource key;
  key.type=SourceType::File;
  key.origin=DataSourceOrigin::Autogenerated;
  key.name=file.getFullPathName().toStdString();
  if(auto existing=getDataSource(key)){refreshDataSource(existing);return;}
  auto parent=file.getParentDirectory();
  // Add the highest new ancestor. Its normal scan discovers this preset
  // and links new folders under the managed User source.
  while(parent==userFolder || parent.isAChildOf(userFolder)){
   DataSource folder;
   folder.type=SourceType::Folder;
   folder.origin=DataSourceOrigin::Autogenerated;
   folder.name=parent.getFullPathName().toStdString();
   if(auto existing=getDataSource(folder)){
    auto child=std::make_shared<DataSourceNode>(key);
    child->setParent(existing);
    addDataSource(child);
    return;
   }
   key=std::move(folder);
   parent=parent.getParentDirectory();
  }
  addDataSource(key,false);
 });
}
void Library::removePath(const DataSourceNodePtr& source){
 if(!source)return;DataSource key;key.type=source->type;key.name=source->name;key.bank=source->bank;
 runOnLoaderThread([this,key]{
  if(key.type==SourceType::Folder || key.type==SourceType::File){
   const juce::File file(key.name);
   excludedPaths.insert(file.getFullPathName());
   const auto inside=[&](const juce::String& path){return file==juce::File(path) || juce::File(path).isAChildOf(file);};
   for(auto it=includedPaths.begin();it!=includedPaths.end();)
    if(inside(*it))it=includedPaths.erase(it);else ++it;
   {
    const std::lock_guard lock(inactivePathsMutex);
    for(auto it=inactivePaths.begin();it!=inactivePaths.end();)
     if(inside(*it))it=inactivePaths.erase(it);else ++it;
   }
   saveSources();
  }
  // A child may also have been connected independently before its parent was
  // scanned. Remove every source at that path, including those disconnected
  // from the cached parent/child tree.
  if(key.type==SourceType::Folder){
   std::vector<DataSourceNodePtr> all;getDataSources(all);const juce::File folder(key.name);
   for(const auto& ds:all)if((ds->type==SourceType::Folder || ds->type==SourceType::File) && juce::File(ds->name).isAChildOf(folder))removeDataSource(*ds,true,true);
  }
  removeDataSource(key,true,true);
 });
}
bool Library::presetPathActive(const DataSource& source)const{
 if(source.type!=SourceType::File && source.type!=SourceType::Folder)return true;
 const std::lock_guard lock(inactivePathsMutex);
 if(inactivePaths.empty())return true;
 const juce::File file(source.name);
 return std::none_of(inactivePaths.begin(),inactivePaths.end(),[&](const auto& path){
  return file==juce::File(path) || file.isAChildOf(juce::File(path));
 });
}
void Library::setPresetPathActive(juce::File file,bool enable){
 runOnLoaderThread([this,file=std::move(file),enable]{
  const auto path=file.getFullPathName();
  bool changed;
  {
   const std::lock_guard lock(inactivePathsMutex);
   changed=enable?inactivePaths.erase(path)!=0:inactivePaths.insert(path).second;
  }
  if(changed){saveSources();++revision;}
 });
}
void Library::requestPresetCatalog(bool hideKSDrums,std::function<void(std::vector<PresetPath>,juce::StringArray)> callback){
 runOnLoaderThread([this,hideKSDrums,callback=std::move(callback)]{
  std::vector<DataSourceNodePtr> all;
  getDataSources(all);
  std::vector<PresetPath> paths;
  std::set<juce::String> features;
  for(const auto& source:all) {
   if(!isSourceEnabled(*source))continue;
   if((source->type==SourceType::Folder || source->type==SourceType::File) &&
      (source->origin==DataSourceOrigin::Manual || !source->hasParent() || includedPaths.count(juce::String(source->name))))
    paths.push_back({source,presetPathActive(*source)});
   if(!presetPathActive(*source))continue;
   const auto patchLock=lockPatchesForReading();
   for(const auto& patch:source->patches) {
    const auto dump=patchDump(*patch);
    if(hideKSDrums && (ksDrum(dump.data(),dump.size()) || isKSPlus(*patch)))continue;
    for(const auto& facet:presetFacets(*patch))features.insert(facet);
   }
  }
  juce::StringArray facets;
  for(const auto& feature:features)facets.add(feature);
  facets.sortNatural();
  std::sort(paths.begin(),paths.end(),[](const auto& a,const auto& b){return a.source->name<b.source->name;});
  runOnUiThread([callback,paths=std::move(paths),facets=std::move(facets)]()mutable{
   callback(std::move(paths),std::move(facets));
  });
 });
}
void Library::rescan(std::function<void()> completed){
 runOnLoaderThread([this,completed=std::move(completed)]{
  // Only an explicit Rescan rediscovers removed paths inside managed Patches.
  // External sources stay disconnected until the user adds them again.
  if(managedFolder.isNotEmpty()){
   const juce::File folder(managedFolder);
   for(auto it=excludedPaths.begin();it!=excludedPaths.end();)if(juce::File(*it)==folder || juce::File(*it).isAChildOf(folder))it=excludedPaths.erase(it);else ++it;
   saveSources();
   if(folder.isDirectory()){
    DataSource ds;ds.type=SourceType::Folder;ds.name=managedFolder.toStdString();ds.origin=DataSourceOrigin::Manual;
    if(!getDataSource(ds))addDataSource(ds);
   }
  }
  std::vector<DataSourceNodePtr> all;getDataSources(all);
  // Scan top-level file/folder sources once; their complete subtrees are rebuilt.
  for(const auto& ds:all)if((ds->type==SourceType::Folder || ds->type==SourceType::File) && !ds->hasParent() && isSourceEnabled(*ds) && presetPathActive(*ds))refreshDataSource(ds);
  runOnLoaderThread([this,completed]{if(completed)runOnUiThread(completed);});
 });
}
void Library::requestBankSources(std::function<void(std::vector<DataSourceNodePtr>&&) > callback,bool hideKSDrums){
 // Read the import tree on its owning loader thread. Individual files remain
 // searchable in the table; only collections belong in the bank selector.
 runOnLoaderThread([this,callback=std::move(callback),hideKSDrums]()mutable{
  std::vector<DataSourceNodePtr> all,banks;getDataSources(all);std::set<DataSourceNodePtr> populatedFolders;
  for(const auto& ds:all)if(!ds->patches.empty())for(auto parent=ds->getParent();parent;parent=parent->getParent())populatedFolders.insert(parent);
  for(const auto& ds:all)if(isSourceEnabled(*ds) && ((ds->type==SourceType::Folder && (ds->origin==DataSourceOrigin::Manual || populatedFolders.count(ds))) || (ds->type==SourceType::File && (ds->patches.size()>1 || ds->origin==DataSourceOrigin::Manual || juce::File(ds->name).hasFileExtension("fxb;zip"))) || ((ds->type==SourceType::LocalStorage || ds->type==SourceType::Rom) && !ds->patches.empty())))banks.push_back(ds);
  if(hideKSDrums){
   std::set<DataSourceNodePtr> populated,playable;
   for(const auto& ds:all)for(const auto& patch:ds->patches)for(auto node=ds;node;node=node->getParent()){
    const auto dump=patchDump(*patch);
    populated.insert(node);if(!ksDrum(dump.data(),dump.size()) && !isKSPlus(*patch))playable.insert(node);
   }
   banks.erase(std::remove_if(banks.begin(),banks.end(),[&](const auto& ds){return populated.count(ds) && !playable.count(ds);}),banks.end());
  }
  runOnUiThread([callback=std::move(callback),banks=std::move(banks)]()mutable{callback(std::move(banks));});
 });
}
bool Library::parseFileData(DataList& output,const Data& input,const std::string& filename){
 if(juce::File(filename).hasFileExtension(NativePatchExtension)){
  if(!validNativePatch(nativePatch(input)))return false;
  output.push_back(input);
  return true;
 }
 if(juce::File(filename).hasFileExtension("fxb;fxp") || (input.size()>=4 && std::memcmp(input.data(),"CcnK",4)==0))return parseVStationBank(output,input) || parseVStationProgram(output,input);
 DataList frames;if(!DB::parseFileData(frames,input,filename))return false;
 const auto before=output.size();
 const bool singleNamed=frames.size()==1 && frames.front().size()==142;
 auto append=[&](Data&& frame){
  const auto model=modelOf(frame);
  if(model==Model::KStation || model==Model::AStation){
   const auto stem=juce::File(filename).getFileNameWithoutExtension();
   const auto address=frame[7]==1 && (frame[11] || frame[12])?unsigned(frame[11])*100+frame[12]:unsigned(output.size()-before)+1;
   const auto name=singleNamed && stem.isNotEmpty()?stem:numberedName(model,address);
   output.push_back(namedRecord(frame,name));
  }else output.push_back(std::move(frame));
 };
 for(auto& frame:frames){
  if(parseVStationSysex(output,frame))continue;
  if(validPatch(frame)){append(std::move(frame));continue;}
  const auto model=modelOf(frame);
  // A/K bank messages hold two consecutive 128-byte programs, rather than
  // a KS Performance. Split them without altering either sound payload.
  if((model!=Model::AStation && model!=Model::KStation) || frame.size()!=270 || frame[7]!=2 || frame.back()!=0xf7 || (frame[12]>98 || (frame[12]&1)) || !std::all_of(frame.begin()+1,frame.end()-1,[](uint8_t b){return b<128;}))continue;
  for(unsigned part=0;part<2;++part){
   Data program(frame.begin(),frame.begin()+13);program[7]=1;program[12]+=uint8_t(part);
   program.insert(program.end(),frame.begin()+13+128*part,frame.begin()+13+128*(part+1));program.push_back(0xf7);
   append(std::move(program));
  }
 }
 std::vector<std::vector<uint8_t>> imported;for(size_t i=before;i<output.size();++i)imported.emplace_back(output[i].begin(),output[i].end());
 output.resize(before);for(auto& bytes:groupDrumMaps(imported))output.emplace_back(bytes.begin(),bytes.end());
 return output.size()!=before;
}
bool Library::parsePresetFile(DataList& output,const Data& input,const std::string& path){
 // Loose files and ZIP entries use the same decode. The leaf name can supply
 // the name of an otherwise unnamed SysEx program.
 const auto key=std::make_pair(baseLib::SHA256(input.data(),input.size()).toString(),juce::File(path).getFileName());
 const auto found=decodedFiles.find(key);
 if(found!=decodedFiles.end()){
  output.insert(output.end(),found->second.begin(),found->second.end());
  return true;
 }
 DataList parsed;
 if(!parseFileData(parsed,input,path))return false;
 size_t size=0;
 for(const auto& record:parsed)size+=record.size()+sizeof(Data);
 output.insert(output.end(),parsed.begin(),parsed.end());
 constexpr size_t budget=32*1024*1024;
 if(size<=budget){
  if(size>budget-decodedBytes){decodedFiles.clear();decodedBytes=0;}
  decodedFiles.emplace(key,std::move(parsed));
  decodedBytes+=size;
 }
 return true;
}
bool Library::loadFile(DataList& data,const std::string& path){
 const juce::File file(path);if(!file.hasFileExtension("syx;mid;midi;fxb;fxp;zip;silver-k"))return false;
 auto report=[this](const juce::String& text){runOnUiThread([this,text]{error=text;});};
 if(!file.hasFileExtension("zip")){
  juce::MemoryBlock block;
  if(file.getSize()>0 && file.getSize()<8*1024*1024 && file.loadFileAsData(block) && !block.isEmpty()){
   const auto* bytes=static_cast<const uint8_t*>(block.getData());
   if(parsePresetFile(data,Data(bytes,bytes+block.getSize()),path)){recordSourceRead(path);return true;}
  }
  if(file.hasFileExtension("fxb;fxp"))report("Unsupported or damaged V-Station preset: "+file.getFileName());
  else if(file.hasFileExtension(NativePatchExtension))report("Unsupported or damaged Silver-K preset: "+file.getFileName());
  return false;
 }
 if(file.getSize()>8*1024*1024){report("Preset archive exceeds 8 MB: "+file.getFileName());return false;}
 const auto before=data.size();size_t expanded=0;juce::ZipFile zip(file);
 for(int i=0;i<zip.getNumEntries();++i){
  const auto* entry=zip.getEntry(i);const juce::File name(entry->filename);
  if(entry->filename.startsWith("__MACOSX/") || name.getFileName().startsWith("._") || !name.hasFileExtension("syx;mid;midi;fxb;fxp;silver-k"))continue;
  if(entry->uncompressedSize<0 || entry->uncompressedSize>8*1024*1024 || expanded+size_t(entry->uncompressedSize)>64*1024*1024){report("Preset archive expansion limit: "+file.getFileName());break;}
  expanded+=size_t(entry->uncompressedSize);std::unique_ptr<juce::InputStream> in(zip.createStreamForEntry(i));juce::MemoryBlock block;if(!in)continue;
  in->readIntoMemoryBlock(block,8*1024*1024);const auto* bytes=static_cast<const uint8_t*>(block.getData());
  if(block.isEmpty() || !parsePresetFile(data,Data(bytes,bytes+block.getSize()),entry->filename.toStdString()))if(name.hasFileExtension("fxb;fxp"))report("Unsupported or damaged V-Station preset: "+file.getFileName()+" / "+name.getFileName());
 }
 if(data.size()==before){report("No supported presets in archive: "+file.getFileName());return false;}
 recordSourceRead(path);
 return true;
}

void Library::addFactory(const std::vector<std::vector<uint8_t>>& data,const juce::String& name){DataSource ds;ds.type=SourceType::LocalStorage;ds.origin=DataSourceOrigin::Manual;ds.name=name.toStdString();std::vector<PatchPtr> patches;for(const auto& bytes:groupDrumMaps(data))if(auto p=createPatch(Data(bytes.begin(),bytes.end()),name.toStdString()))patches.push_back(p);replaceLocalStorage(ds,patches);}
}
