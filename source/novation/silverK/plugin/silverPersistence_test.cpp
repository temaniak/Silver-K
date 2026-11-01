#include "silverBrowser.h"
#include "silverEditor.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <cstdlib>
#include <cstring>
using namespace silverK;
using namespace pluginLib::patchDB;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void word(Data& data,size_t offset,uint32_t value){for(unsigned i=0;i<4;++i)data[offset+i]=uint8_t(value>>(24-8*i));}
static Data preset(const std::string& name,uint8_t sound=1){
 Data bytes(209,0);std::copy_n("CcnK",4,bytes.begin());std::copy_n("FPCh",4,bytes.begin()+8);std::copy_n("NvS0",4,bytes.begin()+16);
 word(bytes,4,201);word(bytes,12,1);word(bytes,24,1);word(bytes,56,149);bytes[60]=sound;
 std::copy_n(name.begin(),std::min(size_t(20),name.size()),bytes.begin()+188);return bytes;
}
static Data bank(const std::vector<std::string>& names,uint8_t version=1){
 const size_t count=names.size(),prefix=version==2?3:1,chunk=prefix+count*149+(version==2?6:7);Data data(160+chunk,0);
 std::copy_n("CcnK",4,data.begin());std::copy_n("FBCh",4,data.begin()+8);std::copy_n("NvS0",4,data.begin()+16);
 word(data,4,uint32_t(data.size()-8));word(data,12,1);word(data,24,uint32_t(count));word(data,156,uint32_t(chunk));data[160]=version;
 if(version==2){data[161]=1;data[162]=76;}
 for(size_t n=0;n<count;++n){data[160+prefix+n*128]=uint8_t(n%127+1);std::copy_n(names[n].begin(),std::min(size_t(20),names[n].size()),data.begin()+160+prefix+count*128+n*21);}return data;
}
static void write(const juce::File& file,const Data& data){check(file.getParentDirectory().createDirectory().wasOk(),"Create fixture folder");check(file.replaceWithData(data.data(),data.size()),"Write fixture");}
static void dataMidi(const juce::File& file,uint8_t program,const char* name){
 Data bytes(270,0);constexpr uint8_t header[]{0xf0,0,0x20,0x29,1,0x42,0x21,1,0,0,0,1};std::copy(std::begin(header),std::end(header),bytes.begin());bytes[12]=program;bytes.back()=0xf7;
 for(const auto& c:ksPlugin::controls)if(c.patchOffset>=0)bytes[13+c.patchOffset]=uint8_t((bytes[13+c.patchOffset]&~c.patchMask)|((c.defaultValue<<c.patchShift)&c.patchMask));
 std::copy_n(name,std::min(size_t(16),std::strlen(name)),bytes.begin()+13+151);
 juce::MidiMessageSequence sequence;sequence.addEvent(juce::MidiMessage(bytes.data(),int(bytes.size()),0));juce::MidiFile midi;midi.setTicksPerQuarterNote(96);midi.addTrack(sequence);
 check(file.getParentDirectory().createDirectory().wasOk(),"MIDI fixture folder");file.deleteFile();auto output=file.createOutputStream();check(output && midi.writeTo(*output),"Write KS Data MIDI fixture");
}
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
static void vStationCompatibilityChecks(const juce::File& root){
 Library library(root.getChildFile("vstation-compatibility"));std::vector<std::string> names;
 for(unsigned n=0;n<400;++n)names.push_back("Compatibility "+std::to_string(n));
 auto verifyBank=[&](const Data& input,unsigned version){
  DataList parsed;check(library.parseFileData(parsed,input,"compatibility.fxb") && parsed.size()==400,"Compatible FXB rejected");
  const size_t soundBase=version==2?163:161,nameBase=soundBase+400*128;std::set<PatchHash> hashes;
  for(unsigned n=0;n<400;++n){auto p=library.createPatch(Data(parsed[n]),{});check(p && patchModel(*p)==Model::VStation,"FXB source identity");
   check(p->name==names[n] && p->bank==1+n/100 && p->program==n%100,"FXB names/order/address");
   check(std::equal(p->sysex.begin()+13,p->sysex.end()-1,input.begin()+soundBase+n*128),"FXB changed sound bytes");
   check(std::equal(parsed[n].end()-21,parsed[n].end(),input.begin()+nameBase+n*21),"FXB changed raw name bytes");hashes.insert(p->hash);
  }check(hashes.size()==400,"FXB slots collapsed");
 };
 for(unsigned version:{1u,2u}){
  const auto original=bank(names,uint8_t(version));verifyBank(original,version);
  auto stale=original;word(stale,4,uint32_t(stale.size()-108));verifyBank(stale,version);
  auto footer=original;footer.insert(footer.end(),112,'#');verifyBank(footer,version);
  auto reject=[&](const Data& invalid){DataList parsed{Data{42}};check(!library.parseFileData(parsed,invalid,"invalid.fxb") && parsed==DataList{Data{42}},"Invalid FXB accepted/partially appended");};
  for(auto size:{size_t(156),size_t(160),original.size()-8,original.size()-1}){auto invalid=original;invalid.resize(size);reject(invalid);}
  auto invalid=original;invalid[160]=3;reject(invalid);
  invalid=original;word(invalid,24,401);reject(invalid);word(invalid,24,0);reject(invalid);
  invalid=original;word(invalid,156,uint32_t(original.size()-161));reject(invalid);
  invalid=original;invalid[(version==2?163:161)+399*128]=128;reject(invalid);
  invalid=original;invalid[(version==2?163:161)+400*128+399*21]=31;reject(invalid);
  invalid=original;invalid[16]='X';reject(invalid);
 }
 auto one=preset("MIDI name from chunk");for(unsigned n=0;n<128;++n)one[60+n]=uint8_t((n*7)%128);
 DataList expected;check(library.parseFileData(expected,one,"reference.fxp"),"MIDI reference FXP");
 Data frame{0xf0,0,0x20,0x29,1,0x41,0x7f,1,0,8,9,0,0};frame.insert(frame.end(),one.begin()+60,one.end());frame.push_back(0xf7);
 auto verifySingle=[&](const Data& input,const char* filename){DataList parsed;check(library.parseFileData(parsed,input,filename) && parsed==expected,"V-Station MIDI/SysEx differs from identical FXP");auto p=library.createPatch(Data(parsed[0]),{});check(p && p->name=="MIDI name from chunk" && patchModel(*p)==Model::VStation,"MIDI name/source identity");};
 for(uint8_t command:{uint8_t(0),uint8_t(1)}){
  frame[7]=command;verifySingle(frame,"different-filename.syx");
  for(uint8_t length:{uint8_t(0x0d),uint8_t(0x22)}){
   // Some exporters declare 141 bytes, omitting the 21-byte name, instead of 162.
   Data midi{'M','T','h','d',0,0,0,6,0,0,0,1,3,0xc0,'M','T','r','k',0,0,0,0,0,0xf0,0x81,length};
   midi.insert(midi.end(),frame.begin()+1,frame.end());midi.insert(midi.end(),{0,0xff,0x2f,0});word(midi,18,uint32_t(midi.size()-22));verifySingle(midi,"different-filename.mid");
  }
 }
 auto rejectSingle=[&](const Data& invalid){DataList parsed{Data{42}};check(!library.parseFileData(parsed,invalid,"invalid.syx") && parsed==DataList{Data{42}},"Invalid extended SysEx accepted/partially appended");};
 auto invalid=frame;invalid.pop_back();rejectSingle(invalid);
 invalid=frame;invalid[7]=2;rejectSingle(invalid);invalid=frame;invalid[5]=0x40;rejectSingle(invalid);
 invalid=frame;invalid[13+127]=128;rejectSingle(invalid);invalid=frame;invalid[141]=31;rejectSingle(invalid);
 const auto user=preset("User 123");std::copy(user.begin()+188,user.end(),frame.begin()+141);DataList parsed;
 check(library.parseFileData(parsed,frame,"user.syx") && !emptyVStationSlot(*library.createPatch(Data(parsed[0]),{})),"Individual MIDI hidden as bank filler");
 std::cout<<"PASS: v1/v2 FXB, stale size/footer, all 400 names/sounds/slots, extended MIDI/SysEx and malformed-input rejection\n";
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
  auto parent=sourceFor(library,folder,SourceType::Folder);library.removePath(parent);idle(library);library.addPath(folder,true);idle(library);check(patches(library).empty(),"Automatic folder reconnection defeated Remove source");
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
static void managedSourceChecks(const juce::File& root){
 const auto folder=root.getChildFile("Managed Patches"),child=folder.getChildFile("Removed child"),external=root.getChildFile("External bank"),database=root.getChildFile("managed-removal-db");
 write(child.getChildFile("one.fxp"),preset("Managed"));write(external.getChildFile("two.fxp"),preset("External",2));
 {
  Library library(database);library.addPath(folder,true);library.addPath(external);idle(library);
  library.removePath(sourceFor(library,child,SourceType::Folder));library.removePath(sourceFor(library,external,SourceType::Folder));idle(library);
  check(patches(library).empty(),"Removed sources kept cached patches");check(child.isDirectory() && external.isDirectory(),"Remove source deleted user files");
  library.addPath(folder,true);idle(library);check(patches(library).empty(),"Automatic Patches reconnect restored a removed folder");
 }
 {
  Library library(database);library.addPath(folder,true);idle(library);check(patches(library).empty(),"Removed source returned from saved cache");
  library.rescan();idle(library);check(patches(library).size()==1 && sourceFor(library,child,SourceType::Folder),"Explicit Rescan did not restore managed child");
  check(!sourceFor(library,external,SourceType::Folder),"Rescan reconnected a removed external source");
  library.removePath(sourceFor(library,folder,SourceType::Folder));idle(library);check(patches(library).empty(),"Removing Patches left independently connected descendants");
 }
 {
  Library library(database);library.addPath(folder,true);idle(library);check(patches(library).empty(),"Removed root returned after restart");
  library.rescan();idle(library);check(patches(library).size()==1,"Rescan could not reconnect managed root");
 }
 std::cout<<"PASS: removed folders and patches leave live/disk cache, survive restart, return only on explicit managed Rescan; user files retained\n";
}
static void requiredDataChecks(const juce::File& root){
 Processor p;juce::String error;check(p.skinsFolder().isDirectory(),"Skins folder missing");
 check(p.bankFile.getSize()==129024 && !p.dataFolder.getChildFile("ROMs/ks-init-flash.bin").exists(),"Zero Wave ROM / no automatic Silver Init");
 check(p.flashFile==juce::File{} && !p.firmwareReady(),"Unconfigured KS started without required Data");
 const auto init=juce::File(__FILE__).getParentDirectory().getChildFile("../../../../assets/ks-init-flash.bin");
 check(init.existsAsFile() && !p.setFlashRom(init,error) && error.contains("Silver Init"),"Legacy synthetic Flash accepted as KS All Data");
 const auto first=root.getChildFile("External data/First.mid"),second=root.getChildFile("External data/Second.mid"),conflict=root.getChildFile("External data/Conflict.mid");
 dataMidi(first,0,"First");dataMidi(second,1,"Second");dataMidi(conflict,0,"Conflict");
 check(p.setFlashRoms({first,second},error),"Multiple KS Data MIDI files rejected");
 const auto chosen=p.ksDataFiles();check(chosen.size()==2,"KS Data list lost files");for(const auto& file:chosen)check(file.isAChildOf(p.dataFolder.getChildFile("ROMs")),"Required Data stayed outside ROMs");
 check(!p.setFlashRoms({first,conflict},error) && p.ksDataFiles()==chosen,"Conflicting MIDI files changed selected data");
 juce::MemoryBlock state;p.getStateInformation(state);Processor restored;restored.setStateInformation(state.getData(),int(state.getSize()));check(restored.ksDataFiles()==chosen,"MIDI set lost in host state");
 Processor restarted;check(restarted.ksDataFiles()==chosen,"MIDI set lost in settings");
 std::cout<<"PASS: required zero Wave ROM and Skins, no synthetic startup, Silver Init rejected, managed multi-MIDI data, conflicts and state/settings persistence\n";
}
static void preferenceChecks(const juce::File& root){
 const auto settings=root.getChildFile("settings.json");juce::String error;
 {
  Processor stale;Processor p;
  const auto flash=root.getChildFile(juce::String::fromUTF8("Flash флеш.mid")),wave=root.getChildFile("Wave custom.bin");check(p.flashFile.copyFileTo(flash) && p.bankFile.copyFileTo(wave),"Copy synthetic assets");
  check(p.setFlashRom(flash,error) && p.setWaveRom(wave,error),"Select external assets");
  p.editorScale.store(.92);p.skinChoice="Classic";p.setEditorChannel(5);p.setBufferProfile(3);check(p.setKSTwoCoreProcessing(true,error),"Set two-core preference");p.savePreferences();
  stale.skinChoice="Silver-K";stale.savePreferences();
  auto saved=juce::JSON::parse(settings);check(double(saved["editorScale"])==.92 && int(saved["bufferProfile"])==3 && saved["skinChoice"].toString()=="Silver-K" && bool(saved["ksTwoCoreProcessing"]),"Stale instance overwrote unrelated settings");
  check(p.setDataFolder(p.dataFolder,error),"Reopen folder");saved=juce::JSON::parse(settings);check(saved["flash"].toString()==p.flashFile.getFullPathName() && p.flashFile.isAChildOf(p.dataFolder.getChildFile("ROMs")),"ROM catalog save discarded managed assets");
 }
 {
  Processor p;check(p.editorScale.load()==.92 && p.skinChoice=="Silver-K" && p.bufferProfile()==3 && p.editorChannel()==5,"Global options lost on restart");
  check(p.ksTwoCoreProcessing(),"Two-core preference lost on restart");
  check(p.flashFile.getFileName()==juce::String::fromUTF8("Flash флеш.mid") && p.bankFile.getFileName()=="Wave custom.bin" && p.flashFile.isAChildOf(p.dataFolder.getChildFile("ROMs")),"Flash/Wave choices lost on restart");
  auto project=std::make_unique<Processor>();juce::MemoryBlock state;p.getStateInformation(state);auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));check(bool(xml),"Read host state");xml->setAttribute("editorScale",.6);xml->setAttribute("bufferProfile",0);xml->setAttribute("skinChoice","Classic");xml->setAttribute("dataFolder",root.getFullPathName());juce::AudioProcessor::copyXmlToBinary(*xml,state);project->setStateInformation(state.getData(),int(state.getSize()));
  check(project->ksTwoCoreProcessing(),"Two-core choice lost in host state");
  xml->removeAttribute("ksTwoCoreProcessing");juce::AudioProcessor::copyXmlToBinary(*xml,state);project->setStateInformation(state.getData(),int(state.getSize()));check(!project->ksTwoCoreProcessing(),"Legacy project did not retain sequential execution");
  const auto selectedWave=p.bankFile,collision=p.dataFolder.getChildFile(selectedWave.getFileName());check(selectedWave.copyFileTo(collision),"Wave relocation fixture");check(project->setWaveRom(selectedWave,error),"Explicit asset choice inside project");project.reset();
  {Processor next;check(next.bankFile==selectedWave,"Project-relative asset path resolved against global folder");}
  auto saved=juce::JSON::parse(settings);check(double(saved["editorScale"])==.92 && int(saved["bufferProfile"])==3 && saved["skinChoice"].toString()=="Silver-K" && bool(saved["ksTwoCoreProcessing"]),"Opening a project changed new-instance defaults");check(saved["dataFolder"].toString()!=root.getFullPathName(),"Project changed global data folder");
  check(p.setKSTwoCoreProcessing(false,error),"Restore sequential default");
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
static void duplicateBrowserChecks(const juce::File& root){
 juce::File folder;
 {
  Processor p;p.browserState.clear();folder=p.patchesFolder().getChildFile("ZIP duplicate regression");
  const auto first=folder.getChildFile("Extracted/One.fxp"),second=folder.getChildFile("Extracted/Two.fxp"),archive=folder.getChildFile("Sounds.zip");
  write(first,preset("Duplicate Fixture A",1));write(second,preset("Duplicate Fixture B",2));write(folder.getChildFile("Different.fxp"),preset("Duplicate Fixture A",9));
  juce::ZipFile::Builder builder;builder.addFile(first,9,"nested/One.fxp");builder.addFile(second,9,"nested/Two.fxp");
  archive.deleteFile();{auto output=archive.createOutputStream();check(output && builder.writeToStream(*output,nullptr),"Write duplicate ZIP fixture");}
  p.patchLibrary.addPath(folder);idle(p.patchLibrary);Browser browser(p);
  auto* table=dynamic_cast<juce::TableListBox*>(find(browser,"patchTable"));auto* source=dynamic_cast<juce::ComboBox*>(find(browser,"patchSource"));auto* search=dynamic_cast<juce::TextEditor*>(find(browser,"patchSearch"));auto* remove=dynamic_cast<juce::TextButton*>(find(browser,"removePatchSource"));check(table && source && search && remove,"Duplicate browser controls");
  auto select=[&](const juce::File& selected){until([&]{browser.pollLibrary();for(int i=0;i<source->getNumItems();++i)if(source->getItemText(i)==selected.getFileName()){source->setSelectedId(source->getItemId(i),juce::dontSendNotification);source->onChange();return true;}return false;} ,"Find duplicate collection");};
  select(folder);until([&]{browser.pollLibrary();return table->getTableListBoxModel()->getNumRows()==3;},"ZIP and extracted patches displayed twice / distinct sounds hidden");
  for(int row=0;row<3;++row)check(!table->getTableListBoxModel()->getCellTooltip(row,6).endsWithIgnoreCase(".zip"),"Extracted copy was not preferred");
  select(archive);until([&]{browser.pollLibrary();return table->getTableListBoxModel()->getNumRows()==2;},"Explicit ZIP selection lost its patches");
  select(folder);search->setText("Duplicate Fixture");search->onTextChange();remove->onClick();
  until([&]{browser.pollLibrary();return table->getTableListBoxModel()->getNumRows()==0;},"Remove source left ZIP/extracted rows in the browser");
  check(first.existsAsFile() && archive.existsAsFile(),"Browser Remove source deleted original files");
 }
 {
  Processor p;Browser browser(p);auto* table=dynamic_cast<juce::TableListBox*>(find(browser,"patchTable"));auto* search=dynamic_cast<juce::TextEditor*>(find(browser,"patchSearch"));auto* rescan=dynamic_cast<juce::TextButton*>(find(browser,"rescanPatches"));
  search->setText("Duplicate Fixture");search->onTextChange();idle(p.patchLibrary);
  for(int i=0;i<20;++i){browser.pollLibrary();juce::Timer::callPendingTimersSynchronously();std::this_thread::sleep_for(std::chrono::milliseconds(5));}
  check(table->getTableListBoxModel()->getNumRows()==0,"Removed browser rows returned from cache after reopening");
  rescan->onClick();until([&]{browser.pollLibrary();return rescan->isEnabled() && table->getTableListBoxModel()->getNumRows()==3;},"Explicit UI Rescan did not restore one copy of each managed patch");
 }
 std::cout<<"PASS: ZIP/extracted content deduplication, same-name different sounds retained, source removal/reopen/explicit UI Rescan without deleting files or cache manually\n";
}
static void firmwareChecks(const juce::File& root,const juce::File& k,const juce::File& ks){
 Processor assets;const auto selectedFlash=assets.flashFile,selectedWave=assets.bankFile;
 juce::MemoryBlock kState,ksState;juce::String error;
 for(const auto& rom:{k,ks}){
  {Processor p;check(p.loadFirmware(rom,error),error.toRawUTF8());p.getStateInformation(rom==k?kState:ksState);}
  {Processor p;until([&]{return p.firmwareReady() && !p.isFirmwareLoading();},"Last firmware did not boot automatically",6000);check(p.osFile.getFileName()==rom.getFileName() && p.osFile.isAChildOf(p.dataFolder.getChildFile("ROMs")) && p.isKStation()==(rom==k),"Wrong default firmware restored");check(p.flashFile==selectedFlash && p.bankFile==selectedWave,"Auto boot changed selected Flash/Wave");}
 }
 {Processor p;p.setStateInformation(kState.getData(),int(kState.getSize()));check(p.firmwareReady() && p.isKStation(),"Host K state lost to default KS boot");}
 {Processor p;const auto zero=p.dataFolder.getChildFile("ROMs/ks-wave-zero.bin");if(selectedWave==zero)check(zero.deleteFile(),"Remove test zero Wave ROM");p.setStateInformation(ksState.getData(),int(ksState.getSize()));check(p.firmwareReady() && !p.isKStation() && zero.getSize()==129024,"KS host state failed to recreate required zero Wave ROM");}
 {Processor p;auto xml=juce::AudioProcessor::getXmlFromBinary(ksState.getData(),int(ksState.getSize()));xml->setAttribute("flash",root.getChildFile("missing-data.mid").getFullPathName());xml->removeAttribute("flashRelative");xml->removeAttribute("ksDataFiles");juce::MemoryBlock state;juce::AudioProcessor::copyXmlToBinary(*xml,state);p.setStateInformation(state.getData(),int(state.getSize()));check(!p.firmwareReady(),"KS state booted from embedded Flash without required external Data");}
 {Processor p;check(p.loadFirmware(ks,error),"Prepare missing-ROM restore");auto xml=juce::AudioProcessor::getXmlFromBinary(kState.getData(),int(kState.getSize()));xml->setAttribute("os",root.getChildFile("missing-os.bin").getFullPathName());xml->removeAttribute("osRelative");juce::MemoryBlock state;juce::AudioProcessor::copyXmlToBinary(*xml,state);p.setStateInformation(state.getData(),int(state.getSize()));p.prepareToPlay(48000,256);check(!p.firmwareReady(),"Missing project ROM played the global default engine");}
 check(juce::JSON::parse(root.getChildFile("settings.json"))["os"].toString()==assets.dataFolder.getChildFile("ROMs").getChildFile(ks.getFileName()).getFullPathName(),"Host restoration overwrote last selected ROM");
 std::cout<<"PASS: K/KS automatic boot, selected Data/Wave, host state precedence, zero Wave recreation, missing Data blocks restore without changing defaults\n";
}
static int runChecks(int argc,char** argv){try{
 auto base=juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",{});auto root=(base.isEmpty()?juce::File::getSpecialLocation(juce::File::tempDirectory):juce::File(base)).getChildFile("persistence-"+juce::Uuid().toString());check(root.createDirectory().wasOk(),"Test directory");
#ifdef _WIN32
 _putenv_s("SILVER_K_DATA",root.getFullPathName().toRawUTF8());
#else
 setenv("SILVER_K_DATA",root.getFullPathName().toRawUTF8(),1);
#endif
 parserChecks(root);vStationCompatibilityChecks(root);rescanChecks(root);managedSourceChecks(root);requiredDataChecks(root);preferenceChecks(root);browserChecks(root);duplicateBrowserChecks(root);if(argc>=3){if(argc>=5){Processor assets;juce::String error;check(assets.setFlashRom(juce::File(argv[3]),error) && assets.setWaveRom(juce::File(argv[4]),error),error.toRawUTF8());}firmwareChecks(root,juce::File(argv[1]),juce::File(argv[2]));}return 0;
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
