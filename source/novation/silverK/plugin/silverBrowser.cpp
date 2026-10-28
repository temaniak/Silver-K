#include "silverBrowser.h"
#include "silverSkin.h"
#include <map>
#include <tuple>
namespace silverK {
using namespace pluginLib::patchDB;
namespace {
juce::String tagText(const PatchPtr& p,TagType type){juce::StringArray tags;for(const auto& t:p->getTags(type).getAdded())tags.add(t);tags.sort(true);return tags.joinIntoString(", ");}
bool matchesName(const Patch& patch,const juce::String& query){
 if(patchName(patch).containsIgnoreCase(query))return true;
 if(patch.sysex.size()==DrumMapSize)for(unsigned n=0;n<49;++n){const auto* name=patch.sysex.data()+n*270+13+151;if(juce::String::fromUTF8(reinterpret_cast<const char*>(name),16).containsIgnoreCase(query))return true;}
 return false;
}
}
Browser::Browser(Processor& processor):p(processor){
 for(auto* c:std::initializer_list<juce::Component*>{&table,&search,&tags,&tagFilter,&model,&source,&favourite,&onlyFavourites,&showAllSlots,&ksBankSelect,&bankHint,&addFolder,&addFile,&refresh,&remove,&saveTags,&exportPatch,&saveCurrent,&info})addAndMakeVisible(*c);
 table.setComponentID("patchTable");table.setModel(this);setSkinColour(table,juce::ListBox::backgroundColourId,juce::Colour(0xffe5e7ea));table.setRowHeight(29);int id=1;for(const auto& col:std::initializer_list<std::pair<const char*,int>>{{"Name",260},{"Model",90},{"Type",100},{"Tags",210},{"Compatibility",190},{"Source",280},{"No.",80},{"Bank",135}})table.getHeader().addColumn(col.first,id++,col.second,40,900);
 addFolder.setComponentID("addPatchFolder");addFile.setComponentID("addPatchFile");saveCurrent.setComponentID("saveCurrentPatch");saveTags.setComponentID("savePatchTags");exportPatch.setComponentID("exportPatch");tags.setComponentID("patchTags");favourite.setComponentID("patchFavourite");onlyFavourites.setComponentID("patchFavouritesOnly");info.setComponentID("patchInfo");bankHint.setComponentID("patchBankHint");
 refresh.setComponentID("rescanPatches");remove.setComponentID("removePatchSource");
 search.setComponentID("patchSearch");model.setComponentID("patchModel");tagFilter.setComponentID("patchTagFilter");source.setComponentID("patchSource");
 table.getHeader().moveColumn(7,0);table.getHeader().moveColumn(8,1);table.getHeader().setSortColumnId(7,true);
 showAllSlots.setComponentID("showAllBankSlots");showAllSlots.setTooltip("Include unnamed and default User-number slots from V-Station banks. All sound data is retained.");
 source.setTooltip("Folders, banks and individually added presets. Rescan refreshes all connected paths.");
 ksBankSelect.setComponentID("ksBank");ksBankSelect.setTooltip("KS memory banks within the selected source. Drum Maps include 49 keys and shared effects.");
 bankHint.setText("Drums: banks 5-8 / MIDI keys 36-84 / shared FX",juce::dontSendNotification);setSkinColour(bankHint,juce::Label::textColourId,juce::Colour(0xff171b20));
 ksBankSelect.addItem("All KS banks / other presets",1);
 ksBankSelect.addItem("Drums - all Drum Maps (banks 5-8)",11);
 for(int bank=1;bank<=8;++bank)ksBankSelect.addItem("Bank "+juce::String(bank)+" - "+(bank<=4?"Programs ":"Drums / Drum Map ")+juce::String(bank*100)+"-"+juce::String(bank*100+(bank<=4?99:49)),bank+1);
 ksBankSelect.addItem("Performances 100-199",10);ksBankSelect.setSelectedId(1,juce::dontSendNotification);
 ksBankSelect.onChange=[this]{dirty=true;};
 search.setTextToShowWhenEmpty("Search patch name",juce::Colours::grey);tagFilter.setTextToShowWhenEmpty("Filter tag",juce::Colours::grey);tags.setTextToShowWhenEmpty("Tags, separated by commas",juce::Colours::grey);
 for(const auto& name:juce::StringArray{"All models","K-Station","KS","A-Station","X-Station","V-Station"})model.addItem(name,model.getNumItems()+1);model.setSelectedId(1,juce::dontSendNotification);
 search.onTextChange=tagFilter.onTextChange=[this]{dirty=true;};model.onChange=onlyFavourites.onClick=showAllSlots.onClick=[this]{dirty=true;};source.onChange=[this]{const int n=source.getSelectedId()-2;preferredSource=n>=0 && n<int(sources.size())?sources[n]:nullptr;preferredSourceName=preferredSource?juce::String(preferredSource->name):juce::String{};preferredSourceType=preferredSource?int(preferredSource->type):0;dirty=true;};
 if(auto saved=juce::parseXML(p.browserState)){
  search.setText(saved->getStringAttribute("search"),false);tagFilter.setText(saved->getStringAttribute("tagFilter"),false);
  model.setSelectedId(juce::jlimit(1,model.getNumItems(),saved->getIntAttribute("model",1)),juce::dontSendNotification);
  onlyFavourites.setToggleState(saved->getBoolAttribute("favourites"),juce::dontSendNotification);showAllSlots.setToggleState(saved->getBoolAttribute("allSlots"),juce::dontSendNotification);
  preferredSourceName=saved->getStringAttribute("source");preferredSourceType=saved->getIntAttribute("sourceType");
  ksBankSelect.setSelectedId(juce::jlimit(1,11,saved->getIntAttribute("ksBank",1)),juce::dontSendNotification);
  table.getHeader().restoreFromString(saved->getStringAttribute("columns"));sortColumn=table.getHeader().getSortColumnId();sortAscending=table.getHeader().isSortedForwards();
 }
 addFolder.onClick=[this]{choose(true);};addFile.onClick=[this]{choose(false);};
 refresh.onClick=[this]{refresh.setEnabled(false);info.setText("Rescanning connected folders and banks...",juce::dontSendNotification);const juce::Component::SafePointer<Browser> self(this);p.patchLibrary.rescan([self]{if(self){self->refresh.setEnabled(true);self->dirty=true;self->info.setText("Rescan complete",juce::dontSendNotification);}});};
 remove.onClick=[this]{const int n=source.getSelectedId()-2;if(n>=0 && n<int(sources.size())){p.patchLibrary.removePath(sources[n]);preferredSource.reset();preferredSourceName.clear();preferredSourceType=0;source.setSelectedId(1,juce::dontSendNotification);dirty=true;}};
 exportPatch.onClick=[this]{choose(false,true);};saveCurrent.onClick=[this]{choose(false,true,true);};
 saveTags.onClick=[this]{const int row=table.getSelectedRow();if(row<0 || row>=int(rows.size()))return;const auto& patch=rows[row];TypedTags edits;juce::StringArray added;added.addTokens(tags.getText(),",",{});added.trim();added.removeEmptyStrings();for(const auto& old:patch->getTags(TagType::Tag).getAdded())if(!added.contains(old))edits.addRemoved(TagType::Tag,old);for(const auto& t:added)edits.add(TagType::Tag,t.toStdString());if(favourite.getToggleState())edits.add(TagType::Favourites,"Favourite");else for(const auto& old:patch->getTags(TagType::Favourites).getAdded())edits.addRemoved(TagType::Favourites,old);p.patchLibrary.modifyTags({patch},edits);};
 updateModel();startTimerHz(10);
}
Browser::~Browser(){stopTimer();rememberView();if(handle!=g_invalidSearchHandle)p.patchLibrary.cancelSearch(handle);table.setModel(nullptr);}
void Browser::resized(){auto area=getLocalBounds().reduced(8);auto top=area.removeFromTop(32);search.setBounds(top.removeFromLeft(220).reduced(2));model.setBounds(top.removeFromLeft(140).reduced(2));tagFilter.setBounds(top.removeFromLeft(160).reduced(2));source.setBounds(top.removeFromLeft(320).reduced(2));onlyFavourites.setBounds(top.removeFromLeft(170));showAllSlots.setBounds(top);if(!p.isKStation()){auto banks=area.removeFromTop(34);ksBankSelect.setBounds(banks.removeFromLeft(440).reduced(2));bankHint.setBounds(banks.reduced(8,2));}area.removeFromTop(8);auto actions=area.removeFromTop(32);for(auto* b:{&addFolder,&addFile,&refresh,&remove,&saveCurrent})b->setBounds(actions.removeFromLeft(140).reduced(2));area.removeFromTop(8);auto bottom=area.removeFromBottom(72);info.setBounds(bottom.removeFromBottom(32));tags.setBounds(bottom.removeFromLeft(420).reduced(2));favourite.setBounds(bottom.removeFromLeft(130));for(auto* b:{&saveTags,&exportPatch})b->setBounds(bottom.removeFromLeft(135).reduced(2));table.setBounds(area);applySkin(*this,false);}
void Browser::query(){resultsDirty=true;if(handle!=g_invalidSearchHandle)p.patchLibrary.cancelSearch(handle);SearchRequest request;const auto queryName=search.getText().trim();const int n=source.getSelectedId()-2;if(n>=0 && n<int(sources.size()))request.sourceNode=sources[n];const auto selected=model.getSelectedId()>1?model.getText():juce::String{};const auto tag=tagFilter.getText().trim();const bool favourites=onlyFavourites.getToggleState(),allSlots=showAllSlots.getToggleState(),k=p.isKStation();const unsigned bank=k?0:unsigned(ksBankSelect.getSelectedId()-1);request.customCompareFunc=[selected,tag,favourites,allSlots,queryName,k,bank](const Patch& patch){if(k && ksDrum(patch.sysex.data(),patch.sysex.size()))return false;if(bank==10){if(!ksDrum(patch.sysex.data(),patch.sysex.size()))return false;}else if(bank && ksBank(patch.sysex.data(),patch.sysex.size())!=bank)return false;if(!matchesName(patch,queryName))return false;if(!allSlots && emptyVStationSlot(patch))return false;if(!selected.isEmpty() && modelName(patchModel(patch))!=selected)return false;if(favourites && patch.getTags(TagType::Favourites).getAdded().empty())return false;if(!tag.isEmpty()){bool match=false;for(const auto& t:patch.getTags(TagType::Tag).getAdded())match|=juce::String(t).containsIgnoreCase(tag);if(!match)return false;}return true;};handle=p.patchLibrary.search(std::move(request));}
void Browser::timerCallback(){
 if(p.firmwareReady() && !p.isFirmwareLoading() && !p.isKStation() && indexedFirmware!=p.osFile){indexedFirmware=p.osFile;p.publishFactoryPresets();}
 p.patchLibrary.uiProcess();
 if(lastLibraryError!=p.patchLibrary.error){lastLibraryError=p.patchLibrary.error;if(lastLibraryError.isNotEmpty())info.setText(lastLibraryError,juce::dontSendNotification);}
 rememberView();
 if(lastK!=p.isKStation()){updateModel();dirty=true;updateRows(rows);table.repaint();}
 if(revision!=p.patchLibrary.revision.load()){
  revision=p.patchLibrary.revision.load();
  const auto request=++bankRequest;const juce::Component::SafePointer<Browser> self(this);
  p.patchLibrary.requestBankSources([self,request](std::vector<DataSourceNodePtr>&& banks){if(self && self->bankRequest==request)self->updateBanks(std::move(banks));},p.isKStation());
 }
 if(dirty){dirty=false;query();}
 if(auto result=p.patchLibrary.getSearch(handle)){
  std::vector<PatchPtr> next;
  {std::shared_lock lock(result->resultsMutex);next.assign(result->results.begin(),result->results.end());}
  if(resultsDirty || next!=searchRows){resultsDirty=false;searchRows=next;updateRows(std::move(next));}
 }
}
void Browser::rememberView(){
 juce::XmlElement state("Browser");state.setAttribute("columns",table.getHeader().toString());state.setAttribute("search",search.getText());state.setAttribute("tagFilter",tagFilter.getText());state.setAttribute("model",model.getSelectedId());state.setAttribute("favourites",onlyFavourites.getToggleState());state.setAttribute("allSlots",showAllSlots.getToggleState());state.setAttribute("source",preferredSourceName);state.setAttribute("sourceType",preferredSourceType);state.setAttribute("ksBank",ksBankSelect.getSelectedId());
 const auto xml=state.toString();if(xml!=p.browserState){p.browserState=xml;p.savePreferences();}
}
void Browser::updateModel(){
 lastK=p.isKStation();revision=~0u;ksBankSelect.setVisible(!lastK);bankHint.setVisible(!lastK);table.getHeader().setColumnVisible(8,!lastK);resized();
}
void Browser::updateBanks(std::vector<DataSourceNodePtr>&& banks){
 auto group=[](const DataSourceNodePtr& ds){return ds->type==SourceType::Folder?0:ds->type==SourceType::File && juce::File(ds->name).hasFileExtension("fxp")?2:1;};
 std::stable_sort(banks.begin(),banks.end(),[&](const auto& a,const auto& b){return group(a)<group(b);});
 sources=std::move(banks);source.clear(juce::dontSendNotification);source.addItem("All sources",1);int selectedId=1,lastGroup=-1;
 for(size_t i=0;i<sources.size();++i){
  const int category=group(sources[i]);if(category!=lastGroup){source.addSectionHeading(category==0?"Folders":category==1?"Banks":"Individual presets");lastGroup=category;}
  source.addItem(juce::File(sources[i]->name).getFileName(),int(i)+2);
  if(preferredSourceName==juce::String(sources[i]->name) && preferredSourceType==int(sources[i]->type)){selectedId=int(i)+2;preferredSource=sources[i];}
 }
 source.setSelectedId(selectedId,juce::dontSendNotification);dirty=true;
}
void Browser::sortOrderChanged(int column,bool ascending){sortColumn=column;sortAscending=ascending;updateRows(rows);}
void Browser::updateRows(std::vector<PatchPtr> next){
 if(p.isKStation())next.erase(std::remove_if(next.begin(),next.end(),[](const auto& patch){return ksDrum(patch->sysex.data(),patch->sysex.size());}),next.end());
 // Keep one source for identical programs, preferring extracted files over an
 // archive. Retain repeated slots within that source and keep all original
 // records so removing one copy leaves the other usable.
 using Identity=std::tuple<Model,PatchHash,juce::String>;
 const auto identity=[](const PatchPtr& patch){return Identity{patchModel(*patch),patch->hash,patchName(*patch)};};
 const auto sourceOrder=[](const DataSourceNodePtr& source){return std::make_pair(source->type==SourceType::File?(juce::File(source->name).hasFileExtension("zip")?1:0):2,source->name);};
 std::map<Identity,DataSourceNodePtr> owners;
 for(const auto& patch:next)if(auto source=patch->source.lock()){
  auto& owner=owners[identity(patch)];if(!owner || sourceOrder(source)<sourceOrder(owner))owner=source;
 }
 next.erase(std::remove_if(next.begin(),next.end(),[&](const auto& patch){auto source=patch->source.lock();return !source || owners[identity(patch)]!=source;}),next.end());
 std::sort(next.begin(),next.end(),[this](const auto& a,const auto& b){
  int order=0;
  if(sortColumn==7){const auto as=a->source.lock(),bs=b->source.lock();order=juce::String(as?as->name:"").compareNatural(bs?bs->name:"");if(!order){const auto ab=ksBank(a->sysex.data(),a->sysex.size()),bb=ksBank(b->sysex.data(),b->sysex.size());if(ab!=bb)order=ab<bb?-1:1;else if(ab && a->sysex[12]!=b->sysex[12])order=a->sysex[12]<b->sysex[12]?-1:1;else if(a->program!=b->program)order=a->program<b->program?-1:1;}}
  else order=cellText(a,sortColumn).compareNatural(cellText(b,sortColumn));
  if(!order)order=patchName(*a).compareNatural(patchName(*b));
  if(!order){const auto as=a->source.lock(),bs=b->source.lock();order=juce::String(as?as->name:"").compareNatural(bs?bs->name:"");}
  if(!order && a->program!=b->program)order=a->program<b->program?-1:1;
  if(!order && a->hash!=b->hash)order=a->hash<b->hash?-1:1;
  return sortAscending?order<0:order>0;
 });
 if(next==rows){table.repaint();return;}
 const int selected=table.getSelectedRow();const auto old=selected>=0 && selected<int(rows.size())?rows[selected]:nullptr;
 const juce::ScopedValueSetter<bool> updating(updatingRows,true);
 table.deselectAllRows();rows=std::move(next);table.updateContent();
 if(old){auto it=std::find(rows.begin(),rows.end(),old);if(it==rows.end()){const auto source=old->source.lock();it=std::find_if(rows.begin(),rows.end(),[&](const auto& patch){const auto nextSource=patch->source.lock();return source && nextSource && *source==*nextSource && patch->program==old->program && patch->hash==old->hash;});}if(it==rows.end())it=std::find_if(rows.begin(),rows.end(),[&](const auto& patch){return identity(patch)==identity(old);});if(it!=rows.end())table.selectRow(int(it-rows.begin()));}
 selectedRowsChanged(table.getSelectedRow());table.repaint();
}
void Browser::paintRowBackground(juce::Graphics& g,int row,int width,int height,bool selected){g.fillAll(selected?skinColour(*this,0xff087fe5):row%2?skinColour(*this,0xffd5d8dc):skinColour(*this,0xffe5e7ea));}
juce::String Browser::cellText(const PatchPtr& patch,int column)const{
 const auto m=patchModel(*patch);
 switch(column){
 case 1:return patchName(*patch);case 2:return modelName(m);
 case 3:if(m==Model::VStation)return patch->sysex[7]==1?"Bank program":"Preset";return patchType(patch->sysex.data(),patch->sysex.size());
 case 4:return (patch->getTags(TagType::Favourites).getAdded().empty()?juce::String{}:juce::String::fromUTF8("★ "))+tagText(patch,TagType::Tag);
 case 5:if(drumMap(patch->sysex.data(),patch->sysex.size()))return "KS / 49 keys + FX";if((p.isKStation() && (m==Model::KStation || m==Model::AStation || m==Model::VStation)) || (!p.isKStation() && (m==Model::KS || m==Model::XStation)))return "Native compatible";return p.isKStation()?"Converted: sine / LP 12":"Converted";
 case 6:if(auto src=patch->source.lock())return src->name;break;
 case 7:{const auto bank=ksBank(patch->sysex.data(),patch->sysex.size());if(!p.isKStation() && bank){if(bank==9)return juce::String(100+patch->sysex[12]);if(drumMap(patch->sysex.data(),patch->sysex.size()))return juce::String(bank*100)+"-"+juce::String(bank*100+48);return juce::String(bank*100+patch->sysex[12]);}return patch->program==g_invalidProgram?juce::String{}:juce::String(patch->program+1);}
 case 8:{if(p.isKStation())return {};const auto bank=ksBank(patch->sysex.data(),patch->sysex.size());return bank==9?"Performances":bank?"Bank "+juce::String(bank)+(bank>=5?" / Drums":""):juce::String{};}
 }
 return {};
}
void Browser::paintCell(juce::Graphics& g,int row,int column,int width,int height,bool selected){if(row<0 || row>=int(rows.size()))return;g.setColour(selected?skinColour(*this,0xffffffff):skinColour(*this,0xff171b20));g.setFont(skinFont(*this,13.f));g.drawText(cellText(rows[row],column),5,0,width-10,height,juce::Justification::centredLeft,true);}
void Browser::selectedRowsChanged(int row){if(row<0 || row>=int(rows.size())){tags.clear();favourite.setToggleState(false,juce::dontSendNotification);info.setText({},juce::dontSendNotification);return;}tags.setText(tagText(rows[row],TagType::Tag),false);favourite.setToggleState(!rows[row]->getTags(TagType::Favourites).getAdded().empty(),juce::dontSendNotification);info.setText(patchName(*rows[row])+" / "+modelName(patchModel(*rows[row])),juce::dontSendNotification);if(!updatingRows)activate(row);}
void Browser::cellClicked(int row,int,const juce::MouseEvent& event){
 if(row<0 || row>=int(rows.size()) || updatingRows)return;
 // Selection changes precede cellClicked. Re-clicking the selected row must
 // also load (e.g. after choosing another part), without loading twice for
 // the same mouse gesture that just changed the selection.
 const auto down=event.mouseDownTime.toMilliseconds();
 if(down>0 && lastActivatedPatch==rows[row] && lastActivationTime>=down)return;
 activate(row);
}
bool Browser::stepPatch(int direction,juce::String* error){
 if(!p.firmwareReady()){if(error)*error="Load firmware first";return false;}
 pollLibrary();if(rows.empty()){if(error)*error=resultsDirty?"Loading preset library...":"No presets match the browser filters";return false;}
 int current=table.getSelectedRow();
 if(current<0 && lastActivatedPatch){auto it=std::find(rows.begin(),rows.end(),lastActivatedPatch);if(it!=rows.end())current=int(it-rows.begin());}
 if(current<0){for(size_t i=0;i<rows.size();++i)if(patchName(*rows[i])==p.currentPatch){current=int(i);break;}}
 const int target=current<0?(direction<0?int(rows.size())-1:0):(current+(direction<0?-1:1)+int(rows.size()))%int(rows.size());
 {const juce::ScopedValueSetter<bool> updating(updatingRows,true);table.selectRow(target);}
 return activate(target,error);
}
std::vector<PatchPtr> Browser::filteredPrograms()const{
 std::vector<PatchPtr> result;for(const auto& patch:rows)if(patch->sysex.size()!=1038)result.push_back(patch);return result;
}
bool Browser::activateProgram(unsigned part,const PatchPtr& patch,juce::String& error){
 if(part>=4 || !p.performanceMode() || p.isKStation()){error="Enable KS Performance first";return false;}
 if(!patch || patch->sysex.size()==1038){error="Choose a Program for this part";return false;}
 if(!p.activatePatchForPart(patch,part,error))return false;
 partPrograms[part]=patch;return true;
}
bool Browser::stepProgram(unsigned part,int direction,juce::String& error){
 pollLibrary();const auto programs=filteredPrograms();
 if(part>=4 || programs.empty()){error=resultsDirty?"Loading preset library...":"No programs match the browser filters";return false;}
 int current=-1;const auto remembered=partPrograms[part];
 if(remembered){const auto it=std::find(programs.begin(),programs.end(),remembered);if(it!=programs.end())current=int(it-programs.begin());}
 if(current<0){const auto names=p.partNames();for(size_t i=0;i<programs.size();++i)if(patchName(*programs[i]).substring(0,16)==names[part]){current=int(i);break;}}
 const int target=current<0?(direction<0?int(programs.size())-1:0):(current+(direction<0?-1:1)+int(programs.size()))%int(programs.size());
 return activateProgram(part,programs[size_t(target)],error);
}
bool Browser::activate(int row,juce::String* error){if(row<0 || row>=int(rows.size())){if(error)*error="No preset selected";return false;}juce::String message;const bool ok=p.activatePatch(rows[row],message);if(ok){lastActivatedPatch=rows[row];if(rows[row]->sysex.size()!=1038)partPrograms[p.currentPart()]=rows[row];lastActivationTime=juce::Time::currentTimeMillis();}else if(error)*error=message;info.setText(ok?(message.isEmpty()?"Loaded "+patchName(*rows[row]):"Loaded; "+message.replace("\n","; ")):message,juce::dontSendNotification);info.setTooltip(message);return ok;}
void Browser::choose(bool folder,bool save,bool current){chooser=std::make_unique<juce::FileChooser>(save?"Save patch":folder?"Add patch folder":"Add patches",p.patchesFolder(),save?"*.syx":"*.syx;*.mid;*.midi;*.fxb;*.fxp;*.zip");const juce::Component::SafePointer<Browser> self(this);chooser->launchAsync((save?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|(folder?juce::FileBrowserComponent::canSelectDirectories:juce::FileBrowserComponent::canSelectFiles),[self,save,current](const juce::FileChooser& c){if(!self)return;auto file=c.getResult();if(file==juce::File{})return;if(!save){self->p.patchLibrary.addPath(file);return;}file=file.withFileExtension("syx");juce::String error;if(current){self->p.saveCurrentPatch(file,error);self->info.setText(error.isEmpty()?"Saved current sound":error,juce::dontSendNotification);}else{int row=self->table.getSelectedRow();if(row>=0 && row<int(self->rows.size()))self->p.patchLibrary.writePatchesToFile(file,{self->rows[row]});}});}
}
