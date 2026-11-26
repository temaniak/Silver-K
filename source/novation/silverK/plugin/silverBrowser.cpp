#include "silverBrowser.h"
#include "silverSkin.h"
#include <map>
#include <string_view>
#include <tuple>
namespace silverK {
using namespace pluginLib::patchDB;
namespace {
juce::String tagText(const PatchPtr& p,TagType type){juce::StringArray tags;for(const auto& t:p->getTags(type).getAdded())tags.add(t);tags.sort(true);return tags.joinIntoString(", ");}
bool matchesName(const Patch& patch,const juce::String& query){
 if(patchName(patch).containsIgnoreCase(query))return true;
 const auto dump=patchDump(patch);
 if(dump.size()==DrumMapSize)for(unsigned n=0;n<49;++n){const auto* name=dump.data()+n*270+13+151;if(juce::String::fromUTF8(reinterpret_cast<const char*>(name),16).containsIgnoreCase(query))return true;}
 return false;
}
bool matchesMetadata(const Patch& patch,const juce::StringArray& terms,const juce::StringArray& facets){
 const auto source=patch.source.lock();
 for(const auto& term:terms){
  if(matchesName(patch,term) || (source && juce::String(source->name).containsIgnoreCase(term)))continue;
  if(std::none_of(facets.begin(),facets.end(),[&](const auto& field){return field.containsIgnoreCase(term);}))return false;
 }
 return true;
}
bool matchesFacets(const juce::StringArray& values,const juce::StringArray& selected){
 juce::StringArray required,matched;
 for(const auto& facet:selected){
  const auto group=facet.upToFirstOccurrenceOf(" / ",false,false);
  required.addIfNotAlreadyThere(group);
  if(values.contains(facet))matched.addIfNotAlreadyThere(group);
 }
 return required.size()==matched.size();
}

}
Browser::Browser(Processor& processor):p(processor){
 for(auto* c:std::initializer_list<juce::Component*>{&table,&search,&tags,&tagFilter,&model,&source,&favourite,&onlyFavourites,&showAllSlots,&showInit,&ksBankSelect,&bankHint,&addFolder,&addFile,&refresh,&remove,&saveTags,&exportPatch,&saveCurrent,&info})addAndMakeVisible(*c);
 table.setComponentID("patchTable");table.setModel(this);setSkinColour(table,juce::ListBox::backgroundColourId,juce::Colour(0xffe5e7ea));table.setRowHeight(29);int id=1;for(const auto& col:std::initializer_list<std::pair<const char*,int>>{{"Name",260},{"Model",90},{"Type",100},{"Tags",210},{"Compatibility",190},{"Source",280},{"No.",80},{"Bank",135}})table.getHeader().addColumn(col.first,id++,col.second,40,900);
 addFolder.setComponentID("addPatchFolder");addFile.setComponentID("addPatchFile");saveCurrent.setComponentID("saveCurrentPatch");saveTags.setComponentID("savePatchTags");exportPatch.setComponentID("exportPatch");tags.setComponentID("patchTags");favourite.setComponentID("patchFavourite");onlyFavourites.setComponentID("patchFavouritesOnly");info.setComponentID("patchInfo");bankHint.setComponentID("patchBankHint");
 refresh.setComponentID("rescanPatches");remove.setComponentID("removePatchSource");
 search.setComponentID("patchSearch");model.setComponentID("patchModel");tagFilter.setComponentID("patchTagFilter");source.setComponentID("patchSource");
 table.getHeader().moveColumn(7,0);table.getHeader().moveColumn(8,1);table.getHeader().setSortColumnId(7,true);
 showAllSlots.setComponentID("showAllBankSlots");showAllSlots.setTooltip("Include unnamed and default User-number slots from V-Station banks. All sound data is retained.");
 showInit.setComponentID("showInit");
 source.setTooltip("Folders, banks and individually added presets. Rescan refreshes all connected paths.");
 ksBankSelect.setComponentID("ksBank");ksBankSelect.setTooltip("KS memory banks within the selected source. Drum Maps include 49 keys and shared effects.");
 bankHint.setText("Drums: banks 5-8 / MIDI keys 36-84 / shared FX",juce::dontSendNotification);setSkinColour(bankHint,juce::Label::textColourId,juce::Colour(0xff171b20));
 ksBankSelect.addItem("All KS banks / other presets",1);
 ksBankSelect.addItem("Drums - all Drum Maps (banks 5-8)",11);
 for(int bank=1;bank<=8;++bank)ksBankSelect.addItem("Bank "+juce::String(bank)+" - "+(bank<=4?"Programs ":"Drums / Drum Map ")+juce::String(bank*100)+"-"+juce::String(bank*100+(bank<=4?99:49)),bank+1);
 ksBankSelect.addItem("Performances 100-199",10);ksBankSelect.setSelectedId(1,juce::dontSendNotification);
 ksBankSelect.onChange=[this]{dirty=true;};
 search.setTextToShowWhenEmpty("Search presets and tags",juce::Colours::grey);tagFilter.setTextToShowWhenEmpty("Filter tag",juce::Colours::grey);tags.setTextToShowWhenEmpty("Tags, separated by commas",juce::Colours::grey);
 for(const auto& name:juce::StringArray{"All models","K-Station","KS","A-Station","X-Station","V-Station","KS+"})model.addItem(name,model.getNumItems()+1);model.setSelectedId(1,juce::dontSendNotification);
 search.onTextChange=tagFilter.onTextChange=[this]{dirty=true;};model.onChange=onlyFavourites.onClick=showAllSlots.onClick=[this]{dirty=true;};showInit.onClick=[this]{cachedWebSnapshot=juce::var{};dirty=true;rememberView();};source.onChange=[this]{const int n=source.getSelectedId()-2;preferredSource=n>=0 && n<int(sources.size())?sources[n]:nullptr;preferredSourceName=preferredSource?juce::String(preferredSource->name):juce::String{};preferredSourceType=preferredSource?int(preferredSource->type):0;dirty=true;};
 if(auto saved=juce::parseXML(p.browserState)){
  search.setText(saved->getStringAttribute("search"),false);tagFilter.setText(saved->getStringAttribute("tagFilter"),false);
  for(auto* filter:saved->getChildIterator())if(filter->hasTagName("Facet"))selectedFacets.add(filter->getStringAttribute("value"));
  selectedFacets.removeEmptyStrings();
  nativeColumnLayout=juce::JSON::parse(saved->getStringAttribute("soulTrainColumns"));
  model.setSelectedId(juce::jlimit(1,model.getNumItems(),saved->getIntAttribute("model",1)),juce::dontSendNotification);
  onlyFavourites.setToggleState(saved->getBoolAttribute("favourites"),juce::dontSendNotification);showAllSlots.setToggleState(saved->getBoolAttribute("allSlots"),juce::dontSendNotification);
  showInit.setToggleState(saved->getBoolAttribute("showInit"),juce::dontSendNotification);
  preferredSourceName=saved->getStringAttribute("source");preferredSourceType=saved->getIntAttribute("sourceType");
  ksBankSelect.setSelectedId(juce::jlimit(1,11,saved->getIntAttribute("ksBank",1)),juce::dontSendNotification);
  table.getHeader().restoreFromString(saved->getStringAttribute("columns"));sortColumn=table.getHeader().getSortColumnId();sortAscending=table.getHeader().isSortedForwards();
 }
 addFolder.onClick=[this]{choose(true);};addFile.onClick=[this]{choose(false);};
 refresh.onClick=[this]{refresh.setEnabled(false);info.setText("Rescanning connected folders and banks...",juce::dontSendNotification);const juce::Component::SafePointer<Browser> self(this);p.rescanPresets([self]{if(self){self->refresh.setEnabled(true);self->dirty=true;self->info.setText("Rescan complete",juce::dontSendNotification);}});};
 remove.onClick=[this]{const int n=source.getSelectedId()-2;if(canRemoveSource()){p.patchLibrary.removePath(sources[n]);preferredSource.reset();preferredSourceName.clear();preferredSourceType=0;source.setSelectedId(1,juce::dontSendNotification);dirty=true;}};
 exportPatch.onClick=[this]{const auto row=table.getSelectedRow();if(juce::isPositiveAndBelow(row,int(rows.size())))exportSelectedPatch(rows[row]);};
 saveCurrent.onClick=[this]{chooseSaveScope();};
 saveTags.onClick=[this]{const int row=table.getSelectedRow();if(row<0 || row>=int(rows.size()))return;const auto& patch=rows[row];TypedTags edits;juce::StringArray added;added.addTokens(tags.getText(),",",{});added.trim();added.removeEmptyStrings();for(const auto& old:patch->getTags(TagType::Tag).getAdded())if(!added.contains(old))edits.addRemoved(TagType::Tag,old);for(const auto& t:added)edits.add(TagType::Tag,t.toStdString());if(favourite.getToggleState())edits.add(TagType::Favourites,"Favourite");else for(const auto& old:patch->getTags(TagType::Favourites).getAdded())edits.addRemoved(TagType::Favourites,old);p.patchLibrary.modifyTags({patch},edits);};
 updateModel();startTimerHz(10);
}
Browser::~Browser(){stopTimer();rememberView();if(handle!=g_invalidSearchHandle)p.patchLibrary.cancelSearch(handle);table.setModel(nullptr);}
void Browser::resized(){auto area=getLocalBounds().reduced(8);auto top=area.removeFromTop(32);search.setBounds(top.removeFromLeft(220).reduced(2));model.setBounds(top.removeFromLeft(140).reduced(2));tagFilter.setBounds(top.removeFromLeft(160).reduced(2));source.setBounds(top.removeFromLeft(320).reduced(2));onlyFavourites.setBounds(top.removeFromLeft(170));showAllSlots.setBounds(top.removeFromLeft(160));showInit.setBounds(top);if(!p.isKStation()){auto banks=area.removeFromTop(34);ksBankSelect.setBounds(banks.removeFromLeft(440).reduced(2));bankHint.setBounds(banks.reduced(8,2));}area.removeFromTop(8);auto actions=area.removeFromTop(32);for(auto* b:{&addFolder,&addFile,&refresh,&remove,&saveCurrent})b->setBounds(actions.removeFromLeft(140).reduced(2));area.removeFromTop(8);auto bottom=area.removeFromBottom(72);info.setBounds(bottom.removeFromBottom(32));tags.setBounds(bottom.removeFromLeft(420).reduced(2));favourite.setBounds(bottom.removeFromLeft(130));for(auto* b:{&saveTags,&exportPatch})b->setBounds(bottom.removeFromLeft(135).reduced(2));table.setBounds(area);applySkin(*this,false);}
void Browser::query(){
 resultsDirty=true;
 if(handle!=g_invalidSearchHandle)p.patchLibrary.cancelSearch(handle);
 SearchRequest request;
 const int sourceIndex=source.getSelectedId()-2;
 if(sourceIndex>=0 && sourceIndex<int(sources.size()))request.sourceNode=sources[sourceIndex];
 const auto selected=model.getSelectedId()>1?model.getText():juce::String{};
 const auto tag=tagFilter.getText().trim();
 auto terms=juce::StringArray::fromTokens(search.getText().trim(),true);
 terms.removeEmptyStrings();
 const bool favourites=onlyFavourites.getToggleState(),allSlots=showAllSlots.getToggleState(),showInitial=showInit.getToggleState(),k=p.isKStation();
 const unsigned bank=k?0:unsigned(ksBankSelect.getSelectedId()-1);
 request.customCompareFunc=[library=&p.patchLibrary,selected,tag,favourites,allSlots,showInitial,terms,k,bank,
                            filters=selectedFacets](const Patch& patch){
  if(const auto source=patch.source.lock(); !source || !library->presetPathActive(*source))return false;
  const auto dump=patchDump(patch);
  if(k && (ksDrum(dump.data(),dump.size()) || isKSPlus(patch)))return false;
  if(bank==10){if(!ksDrum(dump.data(),dump.size()))return false;}
  else if(bank && ksBank(dump.data(),dump.size())!=bank)return false;
  if(!showInitial && isFactoryInit(patch))return false;
  if(!allSlots && emptyVStationSlot(patch))return false;
  if(!selected.isEmpty() && modelName(patchModel(patch))!=selected && patchModelName(patch)!=selected)return false;
  if(favourites && patch.getTags(TagType::Favourites).getAdded().empty())return false;
  if(!tag.isEmpty()){
   bool match=false;
   for(const auto& value:patch.getTags(TagType::Tag).getAdded())match|=juce::String(value).containsIgnoreCase(tag);
   if(!match)return false;
  }
  if(!filters.isEmpty() || !terms.isEmpty()){
   const auto metadata=presetFacets(patch);
   if(!matchesFacets(metadata,filters) || !matchesMetadata(patch,terms,metadata))return false;
  }
  return true;
 };
 searchCompleted=std::make_shared<std::atomic<bool>>(false);
 handle=p.patchLibrary.search(std::move(request),[complete=searchCompleted](const Search&){
  complete->store(true,std::memory_order_release);
 });
}
void Browser::timerCallback(){
 if(pendingSound.isNotEmpty()){
  const auto state=pendingDrumMap?p.drumMapLoadStatus():p.ksPresetLoadStatus();
  if(state==Processor::DrumLoadStatus::Applied)
   info.setText("Loaded "+pendingSound+pendingConversion,juce::dontSendNotification);
  if(state==Processor::DrumLoadStatus::Rejected)info.setText("Could not load "+pendingSound,juce::dontSendNotification);
  if(state==Processor::DrumLoadStatus::Idle)info.setText({},juce::dontSendNotification);
  if(state!=Processor::DrumLoadStatus::Loading){
   pendingSound.clear();
   info.setTooltip(info.getText());
  }
 }
 p.patchLibrary.uiProcess();
 if(lastLibraryError!=p.patchLibrary.error){lastLibraryError=p.patchLibrary.error;if(lastLibraryError.isNotEmpty())info.setText(lastLibraryError,juce::dontSendNotification);}
 rememberView();
 const auto loadRevision=p.firmwareLoadRevision();
 const bool modelChanged=lastK!=p.isKStation();
 const bool firmwareChanged=!p.isFirmwareLoading() && (modelChanged || firmwareRevision!=loadRevision);
 if(firmwareChanged){
  firmwareRevision=loadRevision;
  pendingSound.clear();
  presetSelection.setSelectedPatch(selectionSlot(),{});
  pendingFirmwareSelection=p.loadedPatchName();
  lastActivatedPatch.reset();lastActivationTime=0;
  const juce::ScopedValueSetter<bool> updating(updatingRows,true);
  table.deselectAllRows();
  if(modelChanged){updateModel();dirty=true;updateRows(rows);table.repaint();}
  restoreFirmwareSelection();
 }
 if(const auto slot=selectionSlot();slot!=displayedSelectionSlot && !p.isFirmwareLoading()){
  displayedSelectionSlot=slot;
  if(!firmwareChanged && p.firmwareReady())followSelectedPart();
 }
 if(revision!=p.patchLibrary.revision.load()){
  cachedWebSnapshot=juce::var{};
  revision=p.patchLibrary.revision.load();
  const auto request=++bankRequest;const juce::Component::SafePointer<Browser> self(this);
  p.patchLibrary.requestBankSources([self,request](std::vector<DataSourceNodePtr>&& banks){if(self && self->bankRequest==request)self->updateBanks(std::move(banks));},p.isKStation());
  p.patchLibrary.requestPresetCatalog(p.isKStation(),[self,request](std::vector<Library::PresetPath> paths,juce::StringArray choices){
   if(!self || self->bankRequest!=request)return;
   self->presetPaths=std::move(paths);
   self->facets=std::move(choices);
   self->cachedWebSnapshot=juce::var{};
   self->cachedWebRows=juce::var{};
   self->sourceLabels.clear();
   if(self->sortColumn==6)self->sortAndPresentRows(self->rows);
   else self->table.repaint();
  });
 }
 if(dirty){cachedWebSnapshot=juce::var{};dirty=false;query();}
 if(searchCompleted && searchCompleted->exchange(false,std::memory_order_acquire)){
  const auto result=p.patchLibrary.getSearch(handle);
  if(!result)return;
  std::vector<PatchPtr> next;
  {std::shared_lock lock(result->resultsMutex);next.assign(result->results.begin(),result->results.end());}
  resultsDirty=false;
  updateRows(std::move(next));
 }
}
void Browser::rememberView(){
 juce::XmlElement state("Browser");state.setAttribute("columns",table.getHeader().toString());state.setAttribute("search",search.getText());state.setAttribute("tagFilter",tagFilter.getText());state.setAttribute("model",model.getSelectedId());state.setAttribute("favourites",onlyFavourites.getToggleState());state.setAttribute("allSlots",showAllSlots.getToggleState());state.setAttribute("showInit",showInit.getToggleState());state.setAttribute("source",preferredSourceName);state.setAttribute("sourceType",preferredSourceType);state.setAttribute("ksBank",ksBankSelect.getSelectedId());
 for(const auto& facet:selectedFacets)state.createNewChildElement("Facet")->setAttribute("value",facet);
 if(nativeColumnLayout.isObject())state.setAttribute("soulTrainColumns",juce::JSON::toString(nativeColumnLayout,true));
 const auto xml=state.toString();if(xml!=p.browserState){p.browserState=xml;p.savePreferences();}
}
void Browser::updateModel(){
 lastK=p.isKStation();revision=~0u;ksBankSelect.setVisible(!lastK);bankHint.setVisible(!lastK);table.getHeader().setColumnVisible(8,!lastK);resized();
}
void Browser::updateBanks(std::vector<DataSourceNodePtr>&& banks){
 cachedWebSnapshot=juce::var{};
 const auto user=p.userPatchesFolder();
 auto group=[&user](const DataSourceNodePtr& ds){return ds->type==SourceType::Folder?(juce::File(ds->name)==user?-1:0):ds->type==SourceType::File && juce::File(ds->name).hasFileExtension("fxp")?2:1;};
 std::stable_sort(banks.begin(),banks.end(),[&](const auto& a,const auto& b){return group(a)<group(b);});
 sources=std::move(banks);source.clear(juce::dontSendNotification);source.addItem("All sources",1);int selectedId=1,lastGroup=-1;
 for(size_t i=0;i<sources.size();++i){
  const int category=group(sources[i]);
  if(category>=0 && category!=lastGroup){source.addSectionHeading(category==0?"Folders":category==1?"Banks":"Individual presets");lastGroup=category;}
  const juce::File path(sources[i]->name);
  source.addItem(path.isAChildOf(user)?"User / "+path.getRelativePathFrom(user):path.getFileName(),int(i)+2);
  if(preferredSourceName==juce::String(sources[i]->name) && preferredSourceType==int(sources[i]->type)){selectedId=int(i)+2;preferredSource=sources[i];}
 }
 source.setSelectedId(selectedId,juce::dontSendNotification);dirty=true;
}
void Browser::sortOrderChanged(int column,bool ascending){
 if(sortColumn==column && sortAscending==ascending)return;
 sortColumn=column;sortAscending=ascending;sortAndPresentRows(rows);
}
void Browser::updateRows(std::vector<PatchPtr> next){
 if(p.isKStation())next.erase(std::remove_if(next.begin(),next.end(),[](const auto& patch){const auto dump=patchDump(*patch);return ksDrum(dump.data(),dump.size()) || isKSPlus(*patch);}),next.end());
 // Keep identical programs from the first source read. A bank retains its
 // repeated slots, and a second source is available if the first is removed.
 using Identity=std::tuple<Model,PatchHash,juce::String>;
 const auto identity=[](const PatchPtr& patch){return Identity{patchModel(*patch),patch->hash,juce::String(patch->name).trim()};};
 const auto readOrder=p.patchLibrary.sourceReadOrder();
 const auto sourceOrder=[&](const DataSourceNodePtr& source){
  const auto found=readOrder.find(source->name);
  return std::pair<size_t,std::string_view>{found==readOrder.end()?readOrder.size():found->second,source->name};
 };
 std::map<Identity,DataSourceNodePtr> owners;
 for(const auto& patch:next)if(auto source=patch->source.lock()){
  auto& owner=owners[identity(patch)];if(!owner || sourceOrder(source)<sourceOrder(owner))owner=source;
 }
 next.erase(std::remove_if(next.begin(),next.end(),[&](const auto& patch){auto source=patch->source.lock();return !source || owners[identity(patch)]!=source;}),next.end());
 sortAndPresentRows(std::move(next));
}
void Browser::sortAndPresentRows(std::vector<PatchPtr> next){
 cachedWebRows=cachedWebSnapshot=juce::var{};
 std::sort(next.begin(),next.end(),[this](const auto& a,const auto& b){
  int order=0;
  if(sortColumn==7){const auto as=a->source.lock(),bs=b->source.lock();order=juce::String(as?as->name:"").compareNatural(bs?bs->name:"");if(!order){const auto ad=patchDump(*a),bd=patchDump(*b);const auto ab=ksBank(ad.data(),ad.size()),bb=ksBank(bd.data(),bd.size());if(ab!=bb)order=ab<bb?-1:1;else if(ab && ad[12]!=bd[12])order=ad[12]<bd[12]?-1:1;else if(a->program!=b->program)order=a->program<b->program?-1:1;}}
  else order=cellText(a,sortColumn).compareNatural(cellText(b,sortColumn));
  if(!order)order=patchName(*a).compareNatural(patchName(*b));
  if(!order){const auto as=a->source.lock(),bs=b->source.lock();order=juce::String(as?as->name:"").compareNatural(bs?bs->name:"");}
  if(!order && a->program!=b->program)order=a->program<b->program?-1:1;
  if(!order && a->hash!=b->hash)order=a->hash<b->hash?-1:1;
  return sortAscending?order<0:order>0;
 });
 if(next==rows){restoreFirmwareSelection();table.repaint();return;}
 const int selected=table.getSelectedRow();const auto old=selected>=0 && selected<int(rows.size())?rows[selected]:nullptr;
 const juce::ScopedValueSetter<bool> updating(updatingRows,true);
 table.deselectAllRows();rows=std::move(next);table.updateContent();
 if(old){
  auto it=std::find(rows.begin(),rows.end(),old);
  if(it==rows.end()){
   const auto source=old->source.lock();
   it=std::find_if(rows.begin(),rows.end(),[&](const auto& patch){const auto nextSource=patch->source.lock();return source && nextSource && *source==*nextSource && patch->program==old->program && patch->hash==old->hash;});
  }
  if(it==rows.end())it=std::find_if(rows.begin(),rows.end(),[&](const auto& patch){
   return patchModel(*patch)==patchModel(*old) && patch->hash==old->hash &&
          juce::String(patch->name).trim()==juce::String(old->name).trim();
  });
  if(it!=rows.end())table.selectRow(int(it-rows.begin()));
 }
 if(table.getSelectedRow()<0)selectRowWithoutActivation(presetSelection.indexOf(selectionSlot(),rows));
 selectedRowsChanged(table.getSelectedRow());restoreFirmwareSelection();table.repaint();
}
void Browser::paintRowBackground(juce::Graphics& g,int row,int width,int height,bool selected){g.fillAll(selected?skinColour(*this,0xff087fe5):row%2?skinColour(*this,0xffd5d8dc):skinColour(*this,0xffe5e7ea));}

namespace {
juce::File sourcePath(const DataSourceNode& source){
 if(source.type!=SourceType::File && source.type!=SourceType::Folder)return {};
 const juce::String name(source.name);
 return juce::File::isAbsolutePath(name)?juce::File(name):juce::File{};
}
}

juce::String Browser::sourceLabel(const DataSourceNode& source)const{
 if(const auto label=sourceLabels.find(source.name);label!=sourceLabels.end())return label->second;
 const auto path=sourcePath(source);
 if(path==juce::File{})return juce::String(source.name);
 auto root=p.patchesFolder();
 if(path!=root && !path.isAChildOf(root)){
  root=juce::File{};
  for(const auto& entry:presetPaths){
   if(entry.source->type!=SourceType::Folder)continue;
   const auto folder=sourcePath(*entry.source);
   if(folder!=juce::File{} && (path==folder || path.isAChildOf(folder)) &&
      (root==juce::File{} || folder.isAChildOf(root)))root=folder;
  }
  if(root==juce::File{})root=source.type==SourceType::Folder?path:path.getParentDirectory();
 }
 auto label=root.getFileName();
 if(path!=root){
  const auto prefix=root.getFullPathName();
  const auto separator=juce::File::getSeparatorChar();
  const auto start=prefix.length()+(prefix.endsWithChar(separator)?0:1);
  if(label.isNotEmpty())label+="/";
  label+=path.getFullPathName().substring(start).replaceCharacter(separator,'/');
 }
 if(label.isEmpty())label=path.getFullPathName();
 return sourceLabels.emplace(source.name,std::move(label)).first->second;
}

void Browser::openSourceFolder(const PatchPtr& patch){
 const auto source=patch->source.lock();
 if(!source)return;
 const auto path=sourcePath(*source);
 if(path==juce::File{})return;
 const auto folder=source->type==SourceType::Folder?path:path.getParentDirectory();
 if(!folder.isDirectory() || !folder.startAsProcess())
  info.setText("Could not open the preset folder",juce::dontSendNotification);
}

juce::String Browser::cellText(const PatchPtr& patch,int column)const{
 const auto m=patchModel(*patch);
 const auto dump=patchDump(*patch);
 switch(column){
 case 1:return patchName(*patch);case 2:return patchModelName(*patch);
 case 3:return presetKind(*patch);
 case 4:return (patch->getTags(TagType::Favourites).getAdded().empty()?juce::String{}:juce::String::fromUTF8("★ "))+tagText(patch,TagType::Tag);
 case 5:if(isKSPlus(*patch))return "Silver-K only";if(drumMap(dump.data(),dump.size()))return "KS / 49 keys + FX";if((p.isKStation() && (m==Model::KStation || m==Model::AStation || m==Model::VStation)) || (!p.isKStation() && (m==Model::KS || m==Model::XStation)))return "Native compatible";return p.isKStation()?"Converted: LP 12":"Converted";
 case 6:if(auto src=patch->source.lock())return sourceLabel(*src);break;
 case 7:{const auto bank=ksBank(dump.data(),dump.size());if(!p.isKStation() && bank){if(bank==9)return juce::String(100+dump[12]);if(drumMap(dump.data(),dump.size()))return juce::String(bank*100)+"-"+juce::String(bank*100+48);return juce::String(bank*100+dump[12]);}return patch->program==g_invalidProgram?juce::String{}:juce::String(patch->program+1);}
 case 8:{if(p.isKStation())return {};const auto bank=ksBank(dump.data(),dump.size());return bank==9?"Performances":bank?"Bank "+juce::String(bank)+(bank>=5?" / Drums":""):juce::String{};}
 }
 return {};
}
void Browser::paintCell(juce::Graphics& g,int row,int column,int width,int height,bool selected){if(row<0 || row>=int(rows.size()))return;g.setColour(selected?skinColour(*this,0xffffffff):skinColour(*this,0xff171b20));g.setFont(skinFont(*this,13.f));g.drawText(cellText(rows[row],column),5,0,width-10,height,juce::Justification::centredLeft,true);}
void Browser::selectedRowsChanged(int row){
 cachedWebSnapshot=juce::var{};
 if(row<0 || row>=int(rows.size())){
  exportPatch.setButtonText("Export .syx");exportPatch.setEnabled(false);
  tags.clear();favourite.setToggleState(false,juce::dontSendNotification);
  if(!updatingRows)info.setText({},juce::dontSendNotification);
  return;
 }
 exportPatch.setButtonText(isKSPlus(*rows[row])?"Export Silver-K":"Export .syx");
 exportPatch.setEnabled(true);
 tags.setText(tagText(rows[row],TagType::Tag),false);
 favourite.setToggleState(!rows[row]->getTags(TagType::Favourites).getAdded().empty(),juce::dontSendNotification);
 if(!updatingRows)activate(row);
}
void Browser::cellClicked(int row,int,const juce::MouseEvent& event){
 if(row<0 || row>=int(rows.size()) || updatingRows)return;
 // Selection changes precede cellClicked. Re-clicking the selected row must
 // also load (e.g. after choosing another part), without loading twice for
 // the same mouse gesture that just changed the selection.
 const auto down=event.mouseDownTime.toMilliseconds();
 if(down>0 && lastActivatedPatch==rows[row] && lastActivationTime>=down)return;
 activate(row);
}
void Browser::cellDoubleClicked(int row,int column,const juce::MouseEvent& event){
 if(column==6 && row>=0 && row<int(rows.size()) && !event.mods.isPopupMenu())openSourceFolder(rows[size_t(row)]);
}
int Browser::rowForPatchName(const juce::String& name)const{
 if(name.isEmpty())return -1;
 const auto& remembered=lastPatchForModel[p.isKStation()?1:0];
 if(remembered && patchName(*remembered)==name){
  const auto at=std::find(rows.begin(),rows.end(),remembered);
  if(at!=rows.end())return int(at-rows.begin());
 }
 const auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& patch){return patchName(*patch)==name;});
 return at==rows.end()?-1:int(at-rows.begin());
}
void Browser::restoreFirmwareSelection(){
 if(pendingFirmwareSelection.isEmpty() || p.isFirmwareLoading())return;
 const auto row=rowForPatchName(pendingFirmwareSelection);
 if(row<0)return;
 presetSelection.setSelectedPatch(selectionSlot(),rows[size_t(row)]);
 if(row==table.getSelectedRow())return;
 // The source list can publish an empty result before the new search completes.
 selectRowWithoutActivation(row);
}
unsigned Browser::selectionSlot()const{
 if(p.isKStation())return KProgram;
 return p.performanceMode()?KSParts+p.currentPart():KSSingle;
}
void Browser::selectRowWithoutActivation(int row){
 if(row==table.getSelectedRow())return;
 const juce::ScopedValueSetter<bool> updating(updatingRows,true);
 if(row<0)table.deselectAllRows();else table.selectRow(row);
}
void Browser::followSelectedPart(){
 const auto slot=selectionSlot();
 auto row=presetSelection.indexOf(slot,rows);
 if(row<0)row=rowForPatchName(p.isKStation()?p.loadedPatchName():p.partNames()[p.currentPart()]);
 selectRowWithoutActivation(row);
}
bool Browser::stepPatch(int direction,juce::String* error){
 if(!p.firmwareReady() || p.isFirmwareLoading()){if(error)*error=p.isFirmwareLoading()?"Wait for firmware loading to finish":"Load firmware first";return false;}
 pollLibrary();if(rows.empty()){if(error)*error=resultsDirty?"Loading preset library...":"No presets match the browser filters";return false;}
 const auto multi=p.performanceMode() && !p.isKStation();
 int current=presetSelection.indexOf(selectionSlot(),rows);
 if(current<0)current=rowForPatchName(multi?p.partNames()[p.currentPart()]:p.loadedPatchName());
 const int target=PresetSelection::getNeighbourPreset(current,int(rows.size()),direction);
 if(!activate(target,error))return false;
 selectRowWithoutActivation(target);
 return true;
}
std::vector<PatchPtr> Browser::filteredPrograms()const{
 std::vector<PatchPtr> result;for(const auto& patch:rows)if(patchDump(*patch).size()!=1038)result.push_back(patch);return result;
}
bool Browser::activateProgram(unsigned part,const PatchPtr& patch,juce::String& error){
 if(part>=4 || !p.performanceMode() || p.isKStation()){error="Enable KS Performance first";return false;}
 if(!patch || patchDump(*patch).size()==1038){error="Choose a Program for this part";return false;}
 if(!p.activatePatchForPart(patch,part,error))return false;
 presetSelection.setSelectedPatch(KSParts+part,patch);
 if(part==p.currentPart()){
  pendingFirmwareSelection.clear();
  selectRowWithoutActivation(presetSelection.indexOf(KSParts+part,rows));
 }
 showLoadStatus(patch,part,error);
 cachedWebSnapshot=juce::var{};return true;
}
bool Browser::stepProgram(unsigned part,int direction,juce::String& error){
 pollLibrary();const auto programs=filteredPrograms();
 if(part>=4 || programs.empty()){error=resultsDirty?"Loading preset library...":"No programs match the browser filters";return false;}
 int current=presetSelection.indexOf(KSParts+part,programs);
 if(current<0){const auto names=p.partNames();for(size_t i=0;i<programs.size();++i)if(patchName(*programs[i]).substring(0,16)==names[part]){current=int(i);break;}}
 const int target=PresetSelection::getNeighbourPreset(current,int(programs.size()),direction);
 return activateProgram(part,programs[size_t(target)],error);
}
void Browser::showLoadStatus(const PatchPtr& patch,unsigned part,const juce::String& conversion){
 auto name=patchName(*patch);
 if(!p.isKStation() && p.performanceMode() && patchDump(*patch).size()!=1038)name+=" in Part "+juce::String(part+1);
 const auto changes=conversion.isEmpty()?juce::String{}:". "+conversion.replace("\n"," ");
 if(p.isKStation()){
  pendingSound.clear();
  info.setText("Loaded "+name+changes,juce::dontSendNotification);
 }else{
  pendingSound=name;
  pendingConversion=changes;
  const auto dump=patchDump(*patch);
  pendingDrumMap=ksDrum(dump.data(),dump.size());
  info.setText("Loading "+name+"...",juce::dontSendNotification);
 }
 info.setTooltip(info.getText());
}
bool Browser::activate(int row,juce::String* error){
 if(row<0 || row>=int(rows.size())){if(error)*error="No preset selected";return false;}
 const auto& patch=rows[row];
 juce::String message;
 const bool ok=p.activatePatch(patch,message);
 if(ok){
  pendingFirmwareSelection.clear();lastActivatedPatch=patch;lastPatchForModel[p.isKStation()?1:0]=patch;
  if(!p.isKStation() && patchDump(*patch).size()==1038){
   for(unsigned part=0;part<ks::PartCount;++part)presetSelection.setSelectedPatch(KSParts+part,{});
   presetSelection.setSelectedPatch(KSParts+p.currentPart(),patch);
  }else presetSelection.setSelectedPatch(selectionSlot(),patch);
  lastActivationTime=juce::Time::currentTimeMillis();
  showLoadStatus(patch,p.currentPart(),message);
 }else{
  if(error)*error=message;
  info.setText(message,juce::dontSendNotification);info.setTooltip(message);
 }
 cachedWebSnapshot=juce::var{};
 return ok;
}
bool Browser::canRemoveSource()const{
 const int index=source.getSelectedId()-2;
 if(!juce::isPositiveAndBelow(index,int(sources.size())))return false;
 const auto user=p.userPatchesFolder();
 const juce::File path(sources[size_t(index)]->name);
 return path!=user && !path.isAChildOf(user);
}

void Browser::choose(bool folder,bool paths){
 chooser=std::make_unique<juce::FileChooser>(paths?"Add preset paths":folder?"Add patch folder":"Add patches",
   p.patchesFolder(),"*.syx;*.mid;*.midi;*.fxb;*.fxp;*.zip;*.silver-k",!paths);
 const juce::Component::SafePointer<Browser> self(this);
 const auto selection=paths?(juce::FileBrowserComponent::canSelectDirectories |
                              juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::canSelectMultipleItems):
                            (folder?juce::FileBrowserComponent::canSelectDirectories:juce::FileBrowserComponent::canSelectFiles);
 chooser->launchAsync(juce::FileBrowserComponent::openMode|selection,[self](const juce::FileChooser& dialog){
  if(self)for(const auto& file:dialog.getResults())self->p.patchLibrary.addPath(file);
 });
}

void Browser::chooseSaveScope(){
 if(!p.performanceMode()){saveUserPatch(Processor::PatchSaveScope::SelectedSound);return;}
 juce::PopupMenu menu;
 menu.addItem(1,"Save Performance…");
 menu.addItem(2,"Save selected part…");
 const juce::Component::SafePointer<Browser> self(this);
 menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&saveCurrent),[self](int result){
  if(self && result)self->saveUserPatch(result==1?Processor::PatchSaveScope::Performance:Processor::PatchSaveScope::SelectedSound);
 });
}

void Browser::saveUserPatch(Processor::PatchSaveScope scope){
 if(!p.firmwareReady() || p.isFirmwareLoading()){
  info.setText("Load firmware before saving a preset",juce::dontSendNotification);
  return;
 }
 const bool performance=scope==Processor::PatchSaveScope::Performance;
 auto name=performance?juce::String("Performance"):p.loadedPatchName();
 if(!performance && p.performanceMode())name=p.partNames()[p.currentPart()];
 name=juce::File::createLegalFileName(name.trim());
 if(name.isEmpty())name="Untitled";
 const auto folder=p.userPatchesFolder();
 chooser=std::make_unique<juce::FileChooser>(performance?"Save Performance in User":"Save sound in User",
   folder.getNonexistentChildFile(name,".silver-k"),"*.silver-k");
 const juce::Component::SafePointer<Browser> self(this);
 const auto mode=juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|
                 juce::FileBrowserComponent::warnAboutOverwriting;
 chooser->launchAsync(mode,[self,scope](const juce::FileChooser& dialog){
  if(!self)return;
  const auto result=dialog.getResult();
  if(result==juce::File{})return;
  juce::String error;
  const auto file=result.withFileExtension(NativePatchExtension);
  const bool ok=self->p.saveUserPatch(file,scope,error);
  self->info.setText(ok?"Saved to User: "+file.getFileNameWithoutExtension():error,juce::dontSendNotification);
  self->cachedWebSnapshot=juce::var{};
 });
}

void Browser::exportSelectedPatch(const PatchPtr& patch){
 const bool extended=isKSPlus(*patch);
 const auto extension=extended?NativePatchExtension:"syx";
 const auto name=juce::File::createLegalFileName(patchName(*patch));
 chooser=std::make_unique<juce::FileChooser>(extended?"Export complete KS+ preset":"Export SysEx",
   p.patchesFolder().getChildFile(name.isNotEmpty()?name:"Preset").withFileExtension(extension),
   juce::String("*.")+extension);
 const juce::Component::SafePointer<Browser> self(this);
 const auto mode=juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|
                 juce::FileBrowserComponent::warnAboutOverwriting;
 chooser->launchAsync(mode,[self,patch,extension](const juce::FileChooser& dialog){
  if(!self || dialog.getResult()==juce::File{})return;
  const bool ok=self->p.patchLibrary.writePatchesToFile(dialog.getResult().withFileExtension(extension),{patch});
  self->info.setText(ok?"Exported "+patchName(*patch):"Could not export "+patchName(*patch),juce::dontSendNotification);
 });
}

namespace {
juce::String webPatchId(const pluginLib::patchDB::PatchPtr& patch){
 const auto source=patch->source.lock();
 return (source?juce::String(int(source->type))+":"+source->name:juce::String{})+"|"+juce::String(patch->bank)+"|"+juce::String(patch->program)+"|"+juce::String::toHexString(patch->hash.data(),int(patch->hash.size()),0);
}
juce::var webOptions(const juce::ComboBox& box){juce::Array<juce::var> values;for(int n=0;n<box.getNumItems();++n){auto* o=new juce::DynamicObject;o->setProperty("value",box.getItemId(n));o->setProperty("label",box.getItemText(n));values.add(juce::var(o));}return values;}
}
juce::var Browser::webSnapshot()const{
 if(cachedWebSnapshot.isObject() && cachedWebSnapshot["status"].toString()==info.getText() && bool(cachedWebSnapshot["performance"])==p.performanceMode())return cachedWebSnapshot;
 auto* state=new juce::DynamicObject;
 state->setProperty("query",search.getText());state->setProperty("tagFilter",tagFilter.getText());state->setProperty("model",model.getSelectedId());state->setProperty("source",source.getSelectedId());state->setProperty("bank",ksBankSelect.getSelectedId());
 state->setProperty("models",webOptions(model));state->setProperty("sources",webOptions(source));state->setProperty("banks",webOptions(ksBankSelect));state->setProperty("favouritesOnly",onlyFavourites.getToggleState());state->setProperty("allSlots",showAllSlots.getToggleState());state->setProperty("showInit",showInit.getToggleState());state->setProperty("canRemoveSource",canRemoveSource());state->setProperty("performance",p.performanceMode());state->setProperty("status",info.getText());state->setProperty("loading",resultsDirty);
 const int selected=table.getSelectedRow();
 if(!cachedWebRows.isArray()){
  juce::Array<juce::var> displayRows;
  displayRows.ensureStorageAllocated(int(rows.size()));
  for(const auto& patch:rows){
   auto* row=new juce::DynamicObject;
   row->setProperty("id",webPatchId(patch));
   row->setProperty("program",patchDump(*patch).size()!=1038);
   row->setProperty("name",cellText(patch,1));
   row->setProperty("model",cellText(patch,2));
   row->setProperty("ksPlus",isKSPlus(*patch));
   row->setProperty("type",cellText(patch,3));
   row->setProperty("tags",tagText(patch,pluginLib::patchDB::TagType::Tag));
   row->setProperty("compatibility",cellText(patch,5));
   row->setProperty("source",cellText(patch,6));
   row->setProperty("number",cellText(patch,7));
   row->setProperty("bank",cellText(patch,8));
   row->setProperty("favourite",!patch->getTags(pluginLib::patchDB::TagType::Favourites).getAdded().empty());
   row->setProperty("selected",int(displayRows.size())==selected);
   displayRows.add(juce::var(row));
  }
  cachedWebRows=displayRows;
 }else if(cachedWebSelection!=selected){
  juce::Array<juce::var> displayRows=*cachedWebRows.getArray();
  for(const auto index:{cachedWebSelection,selected})if(juce::isPositiveAndBelow(index,displayRows.size())){
   auto row=displayRows[index].getDynamicObject()->clone();
   row->setProperty("selected",index==selected);
   displayRows.set(index,juce::var(row.release()));
  }
  cachedWebRows=displayRows;
 }
 cachedWebSelection=selected;
 const auto& items=*cachedWebRows.getArray();
 juce::Array<juce::var> paths,availableFilters,filters;
 for(const auto& path:presetPaths){
  auto* item=new juce::DynamicObject;
  item->setProperty("path",juce::String(path.source->name));
  item->setProperty("active",path.active);
  paths.add(juce::var(item));
 }
 for(const auto& facet:facets)availableFilters.add(facet);
 for(const auto& facet:selectedFacets)filters.add(facet);
 state->setProperty("presetPaths",paths);
 state->setProperty("facets",availableFilters);
 state->setProperty("selectedFacets",filters);
 state->setProperty("columnLayout",nativeColumnLayout);
 state->setProperty("rows",cachedWebRows);state->setProperty("sortColumn",sortColumn);state->setProperty("sortAscending",sortAscending);
 juce::Array<juce::var> parts;
 for(unsigned part=0;part<ks::PartCount;++part){
  const auto row=presetSelection.indexOf(KSParts+part,rows);
  parts.add(row>=0 && patchDump(*rows[size_t(row)]).size()!=1038?items[row]["id"]:juce::var{});
 }
 state->setProperty("partPrograms",parts);
 cachedWebSnapshot=juce::var(state);return cachedWebSnapshot;
}
void Browser::webAction(const juce::var& message){
 cachedWebSnapshot=juce::var{};
 const auto action=message["action"].toString();const auto value=message["value"];
 if(action=="query"){search.setText(value.toString());dirty=true;}
 else if(action=="facets" || action=="clearFilters"){
  selectedFacets.clear();
  if(const auto* values=value.getArray())for(const auto& item:*values)
   selectedFacets.addIfNotAlreadyThere(item.toString());
  tagFilter.clear();
  model.setSelectedId(1,juce::dontSendNotification);
  if(action=="clearFilters"){
   preferredSource.reset();preferredSourceName.clear();preferredSourceType=0;
   source.setSelectedId(1,juce::dontSendNotification);
  }
  dirty=true;
 }
 else if(action=="presetPathActive" || action=="removePresetPath"){
  const auto path=message["path"].toString();
  const auto it=std::find_if(presetPaths.begin(),presetPaths.end(),[&](const auto& item){return juce::String(item.source->name)==path;});
  if(it==presetPaths.end())return;
  if(action=="removePresetPath")p.patchLibrary.removePath(it->source);
  else{
   it->active=bool(value);
   p.patchLibrary.setPresetPathActive(juce::File(path),it->active);
  }
  dirty=true;
 }
 else if(action=="columnLayout" && value.isObject())nativeColumnLayout=value;
 else if(action=="addPresetPaths")choose(false,true);
 else if(action=="tagFilter"){tagFilter.setText(value.toString());dirty=true;}
 else if(action=="model"){model.setSelectedId(int(value),juce::sendNotificationSync);dirty=true;}
 else if(action=="source"){source.setSelectedId(int(value),juce::sendNotificationSync);dirty=true;}
 else if(action=="bank"){ksBankSelect.setSelectedId(int(value),juce::sendNotificationSync);dirty=true;}
 else if(action=="favouritesOnly"){onlyFavourites.setToggleState(bool(value),juce::dontSendNotification);dirty=true;}
 else if(action=="showInit"){showInit.setToggleState(bool(value),juce::dontSendNotification);showInit.onClick();}
 else if(action=="allSlots"){showAllSlots.setToggleState(bool(value),juce::dontSendNotification);dirty=true;}
 else if(action=="sort"){
  const int col=int(message["column"]);
  if(col>=1 && col<=8){
   auto& header=table.getHeader();
   const auto direction=message["ascending"];
   const bool ascending=direction.isVoid()?header.getSortColumnId()!=col || !header.isSortedForwards():bool(direction);
   header.setSortColumnId(col,ascending);
  }
 }
 else if(action=="stepPart"){const int part=int(message["part"]);if(part>=1&&part<=4){juce::String error;if(!stepProgram(unsigned(part-1),int(value),error))info.setText(error,juce::dontSendNotification);}}
 else if(action=="addFile")choose(false);
 else if(action=="addFolder")choose(true);
 else if(action=="rescan")refresh.onClick();
 else if(action=="removeSource")remove.onClick();
 else if(action=="saveCurrent")saveUserPatch(value.toString()=="performance"?Processor::PatchSaveScope::Performance:Processor::PatchSaveScope::SelectedSound);
 else {
  const auto id=message["id"].toString();int row=-1;for(int n=0;n<int(rows.size());++n)if(webPatchId(rows[size_t(n)])==id){row=n;break;}if(row<0){info.setText("Preset list changed; select the preset again",juce::dontSendNotification);return;}
  const auto patch=rows[size_t(row)];
  if(action=="loadPart"){juce::String error;if(!activateProgram(unsigned(juce::jlimit(0,3,int(message["part"])-1)),patch,error))info.setText(error,juce::dontSendNotification);}
  else if(action=="load"){if(activate(row))selectRowWithoutActivation(row);}
  else if(action=="openSourceFolder")openSourceFolder(patch);
  else if(action=="export")exportSelectedPatch(patch);
  else if(action=="favourite" || action=="tags"){
   pluginLib::patchDB::TypedTags edits;
   if(action=="favourite"){
    if(bool(value))edits.add(pluginLib::patchDB::TagType::Favourites,"Favourite");else for(const auto& old:patch->getTags(pluginLib::patchDB::TagType::Favourites).getAdded())edits.addRemoved(pluginLib::patchDB::TagType::Favourites,old);
   }else{juce::StringArray added;added.addTokens(value.toString(),",",{});added.trim();added.removeEmptyStrings();for(const auto& old:patch->getTags(pluginLib::patchDB::TagType::Tag).getAdded())if(!added.contains(old))edits.addRemoved(pluginLib::patchDB::TagType::Tag,old);for(const auto& tag:added)edits.add(pluginLib::patchDB::TagType::Tag,tag.toStdString());}
   p.patchLibrary.modifyTags({patch},edits);
  }
 }
}

}
