#include "silverLibrary.h"
#include "silverVStation.h"
#include <cstring>
#include "baseLib/md5.h"
#include "jucePluginLib/patchdb/search.h"
#include "jucePluginLib/patchdb/patchmodifications.h"
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
Model patchModel(const Patch& patch){
 const auto model=modelOf(patch.sysex);
 // CustomA is the source synth type. Recognize the old release's V tag too,
 // so cached banks keep their identity before the one-time source refresh.
 if(model==Model::KStation && (patch.tags.containsAdded(TagType::CustomA,"V-Station") || patch.tags.containsAdded(TagType::Tag,"V-Station")))return Model::VStation;
 return model;
}
// This classifies bank names, not silence; these sounds must never be discarded.
bool emptyVStationSlot(const Patch& patch){return patch.tags.containsAdded(TagType::CustomB,"V-Station user slot") && !(patch.modifications && !patch.modifications->name.empty());}
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
 startLoaderThread();
 runOnLoaderThread([this]{
  std::vector<DataSourceNodePtr> all;getDataSources(all);for(const auto& ds:all)if(!isSourceEnabled(*ds))removeDataSource(*ds,true,true);
  // A specifically reconnected child stays connected even when its parent is excluded.
  for(const auto& path:includedPaths)runOnLoaderThread([this,path]{const juce::File file(path);if(!file.exists())return;DataSource ds;ds.type=file.isDirectory()?SourceType::Folder:SourceType::File;ds.name=path.toStdString();ds.origin=DataSourceOrigin::Manual;if(!getDataSource(ds))addDataSource(ds);});
 });
 if(migrateImports)runOnLoaderThread([this]{std::vector<DataSourceNodePtr> sources;getDataSources(sources);for(const auto& ds:sources)if(ds->origin==DataSourceOrigin::Manual && (ds->type==SourceType::File || ds->type==SourceType::Folder) && juce::File(ds->name).exists())refreshDataSource(ds);});
}
Library::~Library(){stopLoaderThread();}
void Library::onLoadFinished(){if(migrateImports && importVersion.replaceWithText("6\n"))migrateImports=false;}
PatchPtr Library::initializePatch(Data&& data,const std::string& fallback){const auto original=data;std::string importedName;const bool vStation=unpackVStationPatch(data,importedName);if(!vStation)unpackNamedRecord(data,importedName);if(!validPatch(data))return {};auto p=std::make_shared<Patch>();const auto model=modelOf(data);p->sysex=std::move(data);p->bank=p->sysex[11];p->program=p->sysex[12];const bool perf=p->sysex.size()==1038;const unsigned offset=perf?192:(model==Model::KS || model==Model::XStation)?151:0;
 if(model==Model::KS || model==Model::XStation){juce::String name;for(unsigned i=offset;i<offset+16;++i){auto c=p->sysex[13+i];name+=juce::String::charToString(c>=32 && c<127?c:' ');}p->name=name.trim().toStdString();}if(!importedName.empty())p->name=importedName;if(p->name.empty()){const auto stem=juce::String(fallback).trim();p->name=(stem.isNotEmpty()?stem:numberedName(vStation?Model::VStation:model,p->bank*100+p->program+1)).toStdString();}
 if(drumMap(p->sysex.data(),p->sysex.size())){
  juce::String name;for(unsigned i=151;i<167;++i){const auto c=p->sysex[49*270+13+i];name+=juce::String::charToString(c>=32 && c<127?c:' ');}
  p->name=("Drum Map "+juce::String(p->bank)+(name.trim().isNotEmpty()?" - "+name.trim():juce::String{})).toStdString();
 }
 p->tags.add(TagType::CustomA,modelName(vStation?Model::VStation:model).toStdString());p->tags.add(TagType::Category,patchType(p->sysex.data(),p->sysex.size()).toStdString());p->setHashFromMessages(13,1);if(vStation){
 const auto label=juce::String(importedName).trim();const auto suffix=label.substring(5);
 if(p->sysex[7]==1 && (label.isEmpty() || (label.startsWithIgnoreCase("User ") && suffix.isNotEmpty() && suffix.containsOnly("0123456789"))))p->tags.add(TagType::CustomB,"V-Station user slot");
 p->tags.add(TagType::Tag,"V-Station");const baseLib::MD5 digest(original.data(),uint32_t(original.size()));std::memcpy(p->hash.data(),digest.getWords().data(),p->hash.size());}return p;}
Data Library::applyModifications(const PatchPtr& p,const pluginLib::FileType&,pluginLib::ExportType)const{return p->sysex;}
void Library::processDirty(const Dirty& d)const{if(d.patches || d.dataSources || !d.tags.empty())++revision;if(!d.errors.empty())error=d.errors.back();}
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
 juce::TemporaryFile temp(sourceSettings);if(!temp.getFile().replaceWithText(juce::JSON::toString(juce::var(settings.get()))) || !temp.overwriteTargetFileWithTemporary())runOnUiThread([this]{error="Cannot save preset source settings: "+sourceSettings.getFullPathName();});
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
   // Reconnect the chosen subtree, without enabling its excluded ancestors.
   for(auto it=excludedPaths.begin();it!=excludedPaths.end();)if(f==juce::File(*it) || juce::File(*it).isAChildOf(f))it=excludedPaths.erase(it);else ++it;
   for(auto it=includedPaths.begin();it!=includedPaths.end();)if(f==juce::File(*it) || juce::File(*it).isAChildOf(f))it=includedPaths.erase(it);else ++it;
   if(!isSourceEnabled(ds))includedPaths.insert(f.getFullPathName());
  }
  saveSources();if(!isSourceEnabled(ds))return;
  if(auto existing=getDataSource(ds)){if(!automatic)refreshDataSource(existing);return;}
  addDataSource(ds);
 });
}
void Library::removePath(const DataSourceNodePtr& source){
 if(!source)return;DataSource key;key.type=source->type;key.name=source->name;key.bank=source->bank;
 runOnLoaderThread([this,key]{
  if(key.type==SourceType::Folder || key.type==SourceType::File){const juce::File file(key.name);excludedPaths.insert(file.getFullPathName());for(auto it=includedPaths.begin();it!=includedPaths.end();)if(file==juce::File(*it) || juce::File(*it).isAChildOf(file))it=includedPaths.erase(it);else ++it;saveSources();}
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
  for(const auto& ds:all)if((ds->type==SourceType::Folder || ds->type==SourceType::File) && !ds->hasParent() && isSourceEnabled(*ds))refreshDataSource(ds);
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
    populated.insert(node);if(!ksDrum(patch->sysex.data(),patch->sysex.size()))playable.insert(node);
   }
   banks.erase(std::remove_if(banks.begin(),banks.end(),[&](const auto& ds){return populated.count(ds) && !playable.count(ds);}),banks.end());
  }
  runOnUiThread([callback=std::move(callback),banks=std::move(banks)]()mutable{callback(std::move(banks));});
 });
}
bool Library::parseFileData(DataList& output,const Data& input,const std::string& filename){
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
bool Library::loadFile(DataList& data,const std::string& path){
 const juce::File file(path);if(!file.hasFileExtension("syx;mid;midi;fxb;fxp;zip"))return false;
 auto report=[this](const juce::String& text){runOnUiThread([this,text]{error=text;});};
 if(!file.hasFileExtension("zip")){
  const bool ok=DB::loadFile(data,path);if(!ok && file.hasFileExtension("fxb;fxp"))report("Unsupported or damaged V-Station preset: "+file.getFileName());return ok;
 }
 if(file.getSize()>8*1024*1024){report("Preset archive exceeds 8 MB: "+file.getFileName());return false;}
 const auto before=data.size();size_t expanded=0;juce::ZipFile zip(file);
 for(int i=0;i<zip.getNumEntries();++i){
  const auto* entry=zip.getEntry(i);const juce::File name(entry->filename);
  if(entry->filename.startsWith("__MACOSX/") || name.getFileName().startsWith("._") || !name.hasFileExtension("syx;mid;midi;fxb;fxp"))continue;
  if(entry->uncompressedSize<0 || entry->uncompressedSize>8*1024*1024 || expanded+size_t(entry->uncompressedSize)>64*1024*1024){report("Preset archive expansion limit: "+file.getFileName());break;}
  expanded+=size_t(entry->uncompressedSize);std::unique_ptr<juce::InputStream> in(zip.createStreamForEntry(i));juce::MemoryBlock block;if(!in)continue;
  in->readIntoMemoryBlock(block,8*1024*1024);const auto* bytes=static_cast<const uint8_t*>(block.getData());
  if(block.isEmpty() || !parseFileData(data,Data(bytes,bytes+block.getSize()),entry->filename.toStdString()))if(name.hasFileExtension("fxb;fxp"))report("Unsupported or damaged V-Station preset: "+file.getFileName()+" / "+name.getFileName());
 }
 if(data.size()==before)report("No supported presets in archive: "+file.getFileName());
 return data.size()!=before;
}

void Library::addFactory(const std::vector<std::vector<uint8_t>>& data,const juce::String& name){DataSource ds;ds.type=SourceType::LocalStorage;ds.origin=DataSourceOrigin::Manual;ds.name=name.toStdString();std::vector<PatchPtr> patches;for(const auto& bytes:groupDrumMaps(data))if(auto p=createPatch(Data(bytes.begin(),bytes.end()),name.toStdString()))patches.push_back(p);replaceLocalStorage(ds,patches);}
}
