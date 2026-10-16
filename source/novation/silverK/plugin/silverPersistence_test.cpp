#include "silverBrowser.h"
#include "silverEditor.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <cstdlib>
using namespace silverK;
using namespace pluginLib::patchDB;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void word(Data& data,size_t offset,uint32_t value){for(unsigned i=0;i<4;++i)data[offset+i]=uint8_t(value>>(24-8*i));}
static Data preset(const std::string& name,uint8_t sound=1){
 Data bytes(209,0);std::copy_n("CcnK",4,bytes.begin());std::copy_n("FPCh",4,bytes.begin()+8);std::copy_n("NvS0",4,bytes.begin()+16);
 word(bytes,4,201);word(bytes,12,1);word(bytes,24,1);word(bytes,56,149);bytes[60]=sound;
 std::copy_n(name.begin(),std::min(size_t(20),name.size()),bytes.begin()+188);return bytes;
}
static Data bank(const std::vector<std::string>& names){
 const size_t count=names.size(),chunk=1+count*149+7;Data data(160+chunk,0);
 std::copy_n("CcnK",4,data.begin());std::copy_n("FBCh",4,data.begin()+8);std::copy_n("NvS0",4,data.begin()+16);
 word(data,4,uint32_t(data.size()-8));word(data,12,1);word(data,24,uint32_t(count));word(data,156,uint32_t(chunk));data[160]=1;
 for(size_t n=0;n<count;++n){data[161+n*128]=uint8_t(n+1);std::copy_n(names[n].begin(),std::min(size_t(20),names[n].size()),data.begin()+161+count*128+n*21);}return data;
}
static void write(const juce::File& file,const Data& data){check(file.getParentDirectory().createDirectory().wasOk(),"Create fixture folder");check(file.replaceWithData(data.data(),data.size()),"Write fixture");}
template<class F> static void until(F ready,const char* message,int attempts=1200){
 for(int n=0;n<attempts;++n){if(ready())return;juce::Timer::callPendingTimersSynchronously();std::this_thread::sleep_for(std::chrono::milliseconds(5));}throw std::runtime_error(message);
}
static void idle(Library& library){until([&]{library.uiProcess();return !library.isScanning();},"Library job timeout");library.uiProcess();}
static std::vector<PatchPtr> patches(Library& library,const DataSourceNodePtr& source={}){
 std::atomic<bool> done{false};std::vector<PatchPtr> result;SearchRequest request;request.sourceNode=source;
 const auto handle=library.search(std::move(request),[&](const Search& search){result.assign(search.results.begin(),search.results.end());done.store(true);});
 until([&]{library.uiProcess();return done.load();},"Search timeout");library.cancelSearch(handle);return result;
}
static DataSourceNodePtr sourceFor(Library& library,const juce::File& file,SourceType type){DataSource key;key.type=type;key.name=file.getFullPathName().toStdString();return library.getDataSource(key);}
static juce::Component* find(juce::Component& root,const juce::String& id){if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* result=find(*child,id))return result;return nullptr;}
static void parserChecks(const juce::File& root){
 Library library(root.getChildFile("parsers"));auto bytes=preset("Padded name");bytes[208]=255;DataList parsed;
 check(library.parseFileData(parsed,bytes,"a.fxp") && parsed.size()==1,"NUL-padded FXP rejected");auto patch=library.createPatch(Data(parsed[0]),{});check(patch->name=="Padded name","FXP padding changed the name");
 bytes=preset("User 123");parsed.clear();check(library.parseFileData(parsed,bytes,"a.fxp"),"Individual User FXP rejected");check(!emptyVStationSlot(*library.createPatch(Data(parsed[0]),{})),"Individual User FXP hidden as bank filler");
 auto packed=bank({"Zulu","User 102","","Alpha"});parsed.clear();check(library.parseFileData(parsed,packed,"a.fxb") && parsed.size()==4,"Partial chunk bank rejected");
 for(size_t i=0;i<4;++i){patch=library.createPatch(Data(parsed[i]),{});check(patch->sysex[13]==i+1,"Bank changed sound bytes/order");check(emptyVStationSlot(*patch)==(i==1||i==2),"Bank user-slot classification");}
 auto invalid=packed;invalid[161+2*128]=128;const auto count=parsed.size();check(!library.parseFileData(parsed,invalid,"bad.fxb") && parsed.size()==count,"Malformed bank partially imported");
 auto accented=preset("Cafe");accented[191]=233;parsed.clear();check(library.parseFileData(parsed,accented,"accent.fxp"),"Legacy accented name rejected");check(!library.createPatch(Data(parsed[0]),{})->name.empty(),"Accented name lost");
 Data list(156,0);std::copy_n("CcnK",4,list.begin());std::copy_n("FxBk",4,list.begin()+8);std::copy_n("NvS0",4,list.begin()+16);word(list,12,1);word(list,24,2);
 for(const auto& name:{"Second","First"}){auto entry=preset(name);list.insert(list.end(),entry.begin(),entry.end());}word(list,4,uint32_t(list.size()-8));parsed.clear();check(library.parseFileData(parsed,list,"program-list.fxb") && parsed.size()==2,"VST2 list bank rejected");
 check(library.createPatch(Data(parsed[0]),{})->name=="Second" && library.createPatch(Data(parsed[1]),{})->name=="First","List-bank order/names changed");
 list.pop_back();parsed.clear();check(!library.parseFileData(parsed,list,"truncated.fxb") && parsed.empty(),"Truncated list partially imported");
 parsed.clear();check(library.parseFileData(parsed,preset("Factory one"),"factory.fxp"),"Factory fixture");patch=library.createPatch(Data(parsed[0]),{});std::vector<uint8_t> native(patch->sysex.begin(),patch->sysex.end());
 library.addFactory({native},"Fixture Factory");idle(library);check(patches(library).size()==1,"Factory source creation");
 library.addFactory({native},"Fixture Factory");idle(library);check(patches(library).size()==1,"Factory source duplicated");
 native[13]=42;library.addFactory({native,native},"Fixture Factory");idle(library);check(patches(library).size()==2,"Factory source did not update");
 library.addFactory({native},"Fixture Factory");idle(library);auto factory=patches(library);check(factory.size()==1 && factory[0]->sysex[13]==42,"Old Flash programs remained in factory source");
 std::cout<<"PASS: FXP, padded/legacy names, partial/list FXB, original slot order, atomic rejection, factory refresh\n";
}
static void rescanChecks(const juce::File& root){
 {
  const auto parent=root.getChildFile("Connect parent"),child=parent.getChildFile("Existing child"),file=child.getChildFile("one.fxp");write(file,preset("Old"));
  Library library(root.getChildFile("parent-connect-db"));library.addPath(child);idle(library);write(file,preset("Updated",2));write(child.getChildFile("two.fxp"),preset("Added",3));library.addPath(parent);idle(library);
  auto sounds=patches(library);check(sounds.size()==2,"Connecting a parent missed new child files or duplicated programs");for(const auto& sound:sounds)check(sound->name!="Old","Connecting a parent kept stale child data");
 }
 const auto folder=root.getChildFile(juce::String::fromUTF8("Banks банки")),file=folder.getChildFile("Manual.fxp"),keep=folder.getChildFile("Nested/Keep.fxp"),database=root.getChildFile("library");
 write(file,preset("Before"));write(keep,preset("Keep",2));
 {
  Library library(database);library.addPath(file);idle(library);library.addPath(folder);idle(library);
  auto parent=sourceFor(library,folder,SourceType::Folder),child=sourceFor(library,file,SourceType::File);check(parent && child && child->hasParent() && child->origin==DataSourceOrigin::Manual,"Manual-child fixture");
  auto sounds=patches(library,parent);check(sounds.size()==2,"Folder import count");for(const auto& patch:sounds)if(patch->name=="Keep"){TypedTags tags;tags.add(TagType::Favourites,"Favourite");library.modifyTags({patch},tags);}idle(library);
  write(file,preset("After",3));library.refreshDataSource(child);idle(library);sounds=patches(library,parent);check(sounds.size()==2,"Manual child duplicated during rescan");bool after=false;for(const auto& patch:sounds)after|=patch->name=="After";check(after,"Manual child was not reread");
  write(folder.getChildFile("New folder/New.fxp"),preset("New",4));library.rescan();idle(library);sounds=patches(library,parent);check(sounds.size()==3,"Rescan missed new nested file");bool favourite=false;for(const auto& patch:sounds)if(patch->name=="Keep")favourite=patch->getTags(TagType::Favourites).containsAdded("Favourite");check(favourite,"Rescan lost favourites");
  check(file.deleteFile(),"Delete fixture");library.rescan();idle(library);sounds=patches(library,parent);check(sounds.size()==2,"Rescan kept removed file");
  auto nested=sourceFor(library,folder.getChildFile("Nested"),SourceType::Folder);library.removePath(nested);idle(library);library.rescan();idle(library);check(patches(library,parent).size()==1,"Removed nested source returned after rescan");
 }
 {
  Library library(database);idle(library);check(patches(library).size()==1,"Excluded source returned from disk cache");
  auto parent=sourceFor(library,folder,SourceType::Folder);library.removePath(parent);idle(library);library.addPath(folder,true);idle(library);library.rescan();idle(library);check(patches(library).empty(),"Automatic folder reconnection defeated Remove source");
 }
 {
  Library library(database);idle(library);check(patches(library).empty(),"Removed root returned after restart");library.addPath(keep);idle(library);library.addPath(folder,true);idle(library);check(patches(library).size()==1,"Adding one preset reconnected its excluded parent");
 }
 {
  Library library(database);idle(library);check(patches(library).size()==1,"Specifically reconnected child lost on restart");library.addPath(folder);idle(library);check(patches(library).size()==2,"Explicit Add did not reconnect source");
  check(keep.deleteFile(),"Delete second fixture");library.addPath(folder);idle(library);check(patches(library).size()==1,"Re-adding a folder did not rescan");
 }
 std::cout<<"PASS: manual child, changed/new/deleted nested files, favourites, exclusions and restart/reconnect\n";
}
static void preferenceChecks(const juce::File& root){
 const auto settings=root.getChildFile("settings.json");juce::String error;
 {
  Processor stale;Processor p;
  const auto flash=root.getChildFile(juce::String::fromUTF8("Flash флеш.bin")),wave=root.getChildFile("Wave custom.bin");check(p.flashFile.copyFileTo(flash) && p.bankFile.copyFileTo(wave),"Copy synthetic assets");
  check(p.setFlashRom(flash,error) && p.setWaveRom(wave,error),"Select external assets");
  p.editorScale.store(.92);p.skinChoice="Classic";p.setEditorChannel(5);p.setBufferProfile(3);p.savePreferences();
  stale.skinChoice="Silver-K";stale.savePreferences();
  auto saved=juce::JSON::parse(settings);check(double(saved["editorScale"])==.92 && int(saved["bufferProfile"])==3 && saved["skinChoice"].toString()=="Silver-K","Stale instance overwrote unrelated settings");
  check(p.setDataFolder(p.dataFolder,error),"Reopen folder");saved=juce::JSON::parse(settings);check(saved["flash"].toString()==flash.getFullPathName(),"ROM catalog save discarded assets");
 }
 {
  Processor p;check(p.editorScale.load()==.92 && p.skinChoice=="Silver-K" && p.bufferProfile()==3 && p.editorChannel()==5,"Global options lost on restart");
  check(p.flashFile.getFileName()==juce::String::fromUTF8("Flash флеш.bin") && p.bankFile.getFileName()=="Wave custom.bin","Flash/Wave choices lost on restart");
  auto project=std::make_unique<Processor>();juce::MemoryBlock state;p.getStateInformation(state);auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));check(bool(xml),"Read host state");xml->setAttribute("editorScale",.6);xml->setAttribute("bufferProfile",0);xml->setAttribute("skinChoice","Classic");xml->setAttribute("dataFolder",root.getFullPathName());juce::AudioProcessor::copyXmlToBinary(*xml,state);project->setStateInformation(state.getData(),int(state.getSize()));
  const auto selectedWave=p.bankFile,collision=p.dataFolder.getChildFile(selectedWave.getFileName());check(selectedWave.copyFileTo(collision),"Wave relocation fixture");check(project->setWaveRom(selectedWave,error),"Explicit asset choice inside project");project.reset();
  {Processor next;check(next.bankFile==selectedWave,"Project-relative asset path resolved against global folder");}
  auto saved=juce::JSON::parse(settings);check(double(saved["editorScale"])==.92 && int(saved["bufferProfile"])==3 && saved["skinChoice"].toString()=="Silver-K","Opening a project changed new-instance defaults");check(saved["dataFolder"].toString()!=root.getFullPathName(),"Project changed global data folder");
 }
 std::cout<<"PASS: global UI/buffer/channel options, Unicode Flash/Wave paths, merging instances, project isolation\n";
}
static void browserChecks(const juce::File& root){
 const auto file=root.getChildFile("Browser bank.fxb"),folder=root.getChildFile("Individual sounds");write(file,bank({"Zulu","User 102","","Alpha"}));write(folder.getChildFile("a.fxp"),preset("Individual A"));write(folder.getChildFile("nested/b.fxp"),preset("Individual B"));
 {
  Processor p;p.browserState.clear();p.patchLibrary.addPath(file);p.patchLibrary.addPath(folder);idle(p.patchLibrary);Browser browser(p);
  auto* table=dynamic_cast<juce::TableListBox*>(find(browser,"patchTable"));auto* source=dynamic_cast<juce::ComboBox*>(find(browser,"patchSource"));auto* all=dynamic_cast<juce::ToggleButton*>(find(browser,"showAllBankSlots"));check(table && source && all,"Browser controls missing");auto* model=table->getTableListBoxModel();
  auto select=[&](const juce::File& selected){until([&]{browser.pollLibrary();for(int i=0;i<source->getNumItems();++i)if(source->getItemText(i)==selected.getFileName()){source->setSelectedId(source->getItemId(i),juce::dontSendNotification);source->onChange();return true;}return false;},"Find collection");};
  select(folder);until([&]{browser.pollLibrary();return model->getNumRows()==2;},"Individual folder row count");for(int i=0;i<source->getNumItems();++i)check(!source->getItemText(i).endsWithIgnoreCase(".fxp"),"Folder FXPs flooded bank selector");
  select(file);until([&]{browser.pollLibrary();return model->getNumRows()==2 && model->getCellTooltip(0,1)=="Zulu";},"Bank default view/order");check(model->getCellTooltip(1,1)=="Alpha" && model->getCellTooltip(1,7)=="4","Original bank slots lost");
  all->setToggleState(true,juce::dontSendNotification);all->onClick();until([&]{browser.pollLibrary();return model->getNumRows()==4;},"Show all slots lost patches");
  auto* rescan=dynamic_cast<juce::TextButton*>(find(browser,"rescanPatches"));check(rescan!=nullptr,"Rescan button missing");rescan->onClick();until([&]{browser.pollLibrary();return rescan->isEnabled() && model->getNumRows()==4;},"UI Rescan did not complete");check(source->getText()==file.getFileName(),"Rescan lost selected bank");
  table->getHeader().setColumnVisible(2,false);table->getHeader().setColumnWidth(1,333);table->getHeader().setSortColumnId(7,false);
  auto* search=dynamic_cast<juce::TextEditor*>(find(browser,"patchSearch"));search->setText("Alpha");search->onTextChange();until([&]{browser.pollLibrary();return model->getNumRows()==1;},"Saved search filter");
 }
 {
  Processor p;Browser browser(p);auto* table=dynamic_cast<juce::TableListBox*>(find(browser,"patchTable"));auto* source=dynamic_cast<juce::ComboBox*>(find(browser,"patchSource"));auto* all=dynamic_cast<juce::ToggleButton*>(find(browser,"showAllBankSlots"));auto* model=table->getTableListBoxModel();
  until([&]{browser.pollLibrary();return source->getText()==file.getFileName() && model->getNumRows()==1;},"Browser state lost after restart");
  check(!table->getHeader().isColumnVisible(2) && table->getHeader().getColumnWidth(1)==333 && table->getHeader().getSortColumnId()==7 && !table->getHeader().isSortedForwards() && all->getToggleState(),"Column/sort/slot options lost");
 }
 std::cout<<"PASS: bank/folder distinction, original slot numbers, optional User slots, persistent columns/sort/source/search\n";
}
static void firmwareChecks(const juce::File& root,const juce::File& k,const juce::File& ks){
 juce::MemoryBlock kState;juce::String error;
 for(const auto& rom:{k,ks}){
  {Processor p;check(p.loadFirmware(rom,error),error.toRawUTF8());if(rom==k)p.getStateInformation(kState);}
  {Processor p;until([&]{return p.firmwareReady() && !p.isFirmwareLoading();},"Last firmware did not boot automatically",6000);check(p.osFile==rom && p.isKStation()==(rom==k),"Wrong default firmware restored");check(p.flashFile.getFileName()==juce::String::fromUTF8("Flash флеш.bin") && p.bankFile.getFileName()=="Wave custom.bin","Auto boot changed selected Flash/Wave");}
 }
 {Processor p;p.setStateInformation(kState.getData(),int(kState.getSize()));check(p.firmwareReady() && p.isKStation(),"Host K state lost to default KS boot");}
 {Processor p;check(p.loadFirmware(ks,error),"Prepare missing-ROM restore");auto xml=juce::AudioProcessor::getXmlFromBinary(kState.getData(),int(kState.getSize()));xml->setAttribute("os",root.getChildFile("missing-os.bin").getFullPathName());xml->removeAttribute("osRelative");juce::MemoryBlock state;juce::AudioProcessor::copyXmlToBinary(*xml,state);p.setStateInformation(state.getData(),int(state.getSize()));p.prepareToPlay(48000,256);check(!p.firmwareReady(),"Missing project ROM played the global default engine");}
 check(juce::JSON::parse(root.getChildFile("settings.json"))["os"].toString()==ks.getFullPathName(),"Host restoration overwrote last selected ROM");
 std::cout<<"PASS: K/KS automatic boot without an editor, selected Flash/Wave, host state wins without changing defaults\n";
}
static int runChecks(int argc,char** argv){try{
 auto base=juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",{});auto root=(base.isEmpty()?juce::File::getSpecialLocation(juce::File::tempDirectory):juce::File(base)).getChildFile("persistence-"+juce::Uuid().toString());check(root.createDirectory().wasOk(),"Test directory");
#ifdef _WIN32
 _putenv_s("SILVER_K_DATA",root.getFullPathName().toRawUTF8());
#else
 setenv("SILVER_K_DATA",root.getFullPathName().toRawUTF8(),1);
#endif
 parserChecks(root);rescanChecks(root);preferenceChecks(root);browserChecks(root);if(argc==3)firmwareChecks(root,juce::File(argv[1]),juce::File(argv[2]));return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

static int testArgc=0;static char** testArgv=nullptr;
class PersistenceTestApplication final:public juce::JUCEApplication,private juce::Timer {
public:
 const juce::String getApplicationName()override{return "Silver-K persistence checks";}
 const juce::String getApplicationVersion()override{return "1";}
 bool moreThanOneInstanceAllowed()override{return true;}
 void initialise(const juce::String&)override{
  // A real message loop starts JUCE's timer thread just as a plugin host does.
  // Keep it alive while the synchronous assertions poll pending callbacks.
  startTimerHz(10);juce::Timer::callAfterDelay(1,[this]{juce::MessageManager::callAsync([this]{setApplicationReturnValue(runChecks(testArgc,testArgv));quit();});});
 }
 void shutdown()override{stopTimer();}
private:
 void timerCallback()override{}
};
int main(int argc,char** argv){
 std::cout<<std::unitbuf;std::cerr<<std::unitbuf;testArgc=argc;testArgv=argv;
 juce::JUCEApplicationBase::createInstance=[]()->juce::JUCEApplicationBase*{return new PersistenceTestApplication;};
#if JUCE_WINDOWS
 // JUCE implements only main() on Windows; it reads its own command line.
 // Keep the console arguments above for the optional private-ROM checks.
 return juce::JUCEApplicationBase::main();
#else
 std::vector<const char*> arguments(argv,argv+argc);arguments.push_back(nullptr);return juce::JUCEApplicationBase::main(argc,arguments.data());
#endif
}
