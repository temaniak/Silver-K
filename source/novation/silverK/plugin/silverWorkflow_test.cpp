// Explicit retained LEGACY comparison test; not a semantic NativeControllerExecutor proof.
#include "ksLib/ksBoardComparison_test.h"
#include "silverBrowser.h"
#include "kstationLib/kstationRomLoader.h"
#include "kstationLib/kstationMidiQueue.h"
#include "ksLib/ksRom.h"
#include "silverKCutoffMidi.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <set>
thread_local bool inAudio=false;
thread_local size_t audioAllocations=0;
void* operator new(std::size_t n){if(inAudio)++audioAllocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{std::free(p);}
using namespace silverK;
using namespace pluginLib::patchDB;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::Component* find(juce::Component& c,const juce::String& id){if(c.getComponentID()==id)return &c;for(int n=0;n<c.getNumChildComponents();++n)if(auto* p=find(*c.getChildComponent(n),id))return p;return nullptr;}
static std::vector<uint8_t> program(Model model){
 std::vector<uint8_t> data(model==Model::KStation || model==Model::AStation?142:270,0);
 const uint8_t h[]{0xf0,0,0x20,0x29,1,uint8_t(model==Model::AStation?0x40:model==Model::KStation?0x41:0x42),0x7f,0,0,uint8_t(model==Model::XStation?0x10:0x11),uint8_t(model==Model::XStation?0:model==Model::KS?2:1),0,0};
 std::copy_n(h,13,data.begin());data.back()=0xf7;
 const auto controls=model==Model::KStation || model==Model::AStation?kControls():std::vector<ksPlugin::Control>(std::begin(ksPlugin::controls),std::end(ksPlugin::controls));
 for(const auto& c:controls)if(c.patchOffset>=0){auto& b=data[13+c.patchOffset];b=uint8_t((b&~c.patchMask)|((c.defaultValue<<c.patchShift)&c.patchMask));}
 if(data.size()==270){const std::string name=model==Model::XStation?"Zulu X patch":"Alpha KS patch";std::fill(data.begin()+164,data.begin()+180,' ');std::copy(name.begin(),name.end(),data.begin()+164);}
 return data;
}
static std::vector<std::vector<uint8_t>> drumFrames(unsigned bank){
 std::vector<std::vector<uint8_t>> frames;
 for(unsigned n=0;n<50;++n){auto frame=program(Model::KS);frame[7]=1;frame[8]=1;frame[11]=uint8_t(bank);frame[12]=uint8_t(n);
  std::fill_n(frame.begin()+164,16,' ');const auto name=n==49?std::string("Test Kit FX"):"Drum "+std::to_string(n);std::copy(name.begin(),name.end(),frame.begin()+164);
  frame[13+168]=uint8_t(n==0?36:n==1?60:72);frame[13+173]=0;frame[13+62]=frame[13+65]=0;frame[13+64]=127;frame[13+100]=frame[13+107]=frame[13+110]=0;
  frames.push_back(std::move(frame));
 }return frames;
}
static void drumParserChecks(Library& library){
 const auto frames=drumFrames(5);auto grouped=groupDrumMaps(frames);check(grouped.size()==1 && validPatch(grouped[0]) && drumMap(grouped[0].data(),grouped[0].size()),"Complete drum bank not recognized");
 const auto& map=grouped[0];for(unsigned n=0;n<50;++n)check(std::equal(frames[n].begin(),frames[n].end(),map.begin()+n*270),"Drum map changed original data");
 auto patch=library.createPatch(Data(map.begin(),map.end()),{});check(patch && patch->name=="Drum Map 5 - Test Kit FX" && patch->tags.containsAdded(TagType::Category,"Drum Map"),"Drum map name/type");
 check(convertPatch(map,Model::KStation).data.empty() && convertPatch(frames[0],Model::KStation).data.empty(),"KS drums converted into K-Station");
 auto broken=map;broken[270+12]=0;check(!validPatch(broken),"Duplicate drum slot accepted as complete map");broken=map;broken.back()=0;check(!validPatch(broken),"Damaged drum map accepted");
 auto partial=frames;partial.pop_back();check(groupDrumMaps(partial).size()==49,"Incomplete bank synthesized missing effects");
 auto duplicate=frames;duplicate.push_back(frames[0]);check(groupDrumMaps(duplicate).size()==51,"Duplicate banks silently discarded");
 auto shuffled=frames;std::reverse(shuffled.begin(),shuffled.end());check(groupDrumMaps(shuffled)==grouped,"Drum slot order depends on file order");
 DataList parsed;check(library.parseFileData(parsed,Data(map.begin(),map.end()),"kit.syx") && parsed.size()==1 && std::equal(parsed[0].begin(),parsed[0].end(),map.begin(),map.end()),"Drum map SysEx round trip");
 juce::MidiMessageSequence track;for(const auto& frame:shuffled)track.addEvent(juce::MidiMessage(frame.data(),int(frame.size()),0));juce::MidiFile midi;midi.setTicksPerQuarterNote(96);midi.addTrack(track);juce::MemoryOutputStream stream;check(midi.writeTo(stream),"Drum MIDI fixture");const auto* bytes=static_cast<const uint8_t*>(stream.getData());parsed.clear();check(library.parseFileData(parsed,Data(bytes,bytes+stream.getDataSize()),"kit.mid") && parsed.size()==1,"Drum map MIDI import");
 check(patchType(frames[0].data(),270)=="Drum sound" && patchType(frames[49].data(),270)=="Drum Map FX","Partial drum classification");
 auto x=program(Model::XStation);x[7]=1;x[11]=5;check(!ksDrum(x.data(),x.size()) && ksBank(x.data(),x.size())==0,"X-Station treated as KS Drum Map");
 std::cout<<"PASS: KS Drum Maps preserve 49 sounds/shared FX, SysEx/MIDI round trip, incomplete/duplicate banks, K conversion guard\n";
}
static void drumBrowserChecks(Processor& p,const juce::File& root){
 p.browserState.clear();const auto folder=root.getChildFile(p.isKStation()?"Drums-K":"Drums-KS");check(folder.createDirectory().wasOk(),"Drum browser fixture folder");Data data;
 for(unsigned bank:{1u,2u}){auto frame=program(Model::KS);frame[7]=1;frame[11]=uint8_t(bank);frame[12]=7;frame[13+7]=uint8_t(60+bank);data.insert(data.end(),frame.begin(),frame.end());}
 for(unsigned bank:{5u,6u})for(const auto& frame:drumFrames(bank))data.insert(data.end(),frame.begin(),frame.end());
 auto init=initPatch(Model::KS);
 init[7]=1;
 for(uint8_t bank:{3,4}){
  init[11]=bank;
  data.insert(data.end(),init.begin(),init.end());
 }
 auto blankPerformance=initPatch(Model::KS,true);
 std::copy_n("PERFORMANCE     ",16,blankPerformance.begin()+13+192);
 for(size_t part=0;part<ks::PartCount;++part)
  std::copy_n("PROGRAM NAME    ",16,blankPerformance.begin()+13+part*256+151);
 data.insert(data.end(),blankPerformance.begin(),blankPerformance.end());
 auto partial=drumFrames(7)[0];data.insert(data.end(),partial.begin(),partial.end());const auto file=folder.getChildFile("banks.syx");check(file.replaceWithData(data.data(),data.size()),"Write bank fixtures");p.patchLibrary.addPath(folder);const auto onlyDrums=root.getChildFile(p.isKStation()?"Only-drums-K.syx":"Only-drums-KS.syx");const auto kit=groupDrumMaps(drumFrames(8))[0];check(onlyDrums.replaceWithData(kit.data(),kit.size()),"Write drum-only source");p.patchLibrary.addPath(onlyDrums);
 {
  Browser browser(p);browser.setBounds(0,0,1280,600);auto* source=dynamic_cast<juce::ComboBox*>(find(browser,"patchSource"));auto* bank=dynamic_cast<juce::ComboBox*>(find(browser,"ksBank"));auto* table=dynamic_cast<juce::TableListBox*>(find(browser,"patchTable"));check(source&&bank&&table,"Bank controls missing");auto* rows=table->getTableListBoxModel();bool selected=false;
  auto pump=[&]{browser.pollLibrary();std::this_thread::sleep_for(std::chrono::milliseconds(5));};
  for(int n=0;n<800;++n){pump();if(!selected)for(int i=0;i<source->getNumItems();++i)if(source->getItemText(i)==folder.getFileName()){source->setSelectedId(source->getItemId(i),juce::dontSendNotification);source->onChange();selected=true;break;}if(selected && rows->getNumRows()==(p.isKStation()?2:5))break;}
  check(selected && rows->getNumRows()==(p.isKStation()?2:5),"Model-specific bank rows");check(bank->isVisible()==!p.isKStation() && table->getHeader().isColumnVisible(8)==!p.isKStation(),"KS banks visible in wrong model");
  auto* showInit=dynamic_cast<juce::ToggleButton*>(find(browser,"showInit"));
  check(showInit && !showInit->getToggleState(),"Init slots must start hidden");
  showInit->setToggleState(true,juce::dontSendNotification);showInit->onClick();
  const auto withInit=p.isKStation()?5:8;
  for(int n=0;n<800 && rows->getNumRows()!=withInit;++n)pump();
  check(rows->getNumRows()==withInit,"Show init did not restore Programs in banks 3/4 and the empty Performance");
  showInit->setToggleState(false,juce::dontSendNotification);showInit->onClick();
  if(!p.isKStation()){
   bank->setSelectedId(11,juce::dontSendNotification);bank->onChange();for(int n=0;n<800 && rows->getNumRows()!=3;++n)pump();check(rows->getNumRows()==3,"All Drums filter");
   bank->setSelectedId(6,juce::dontSendNotification);bank->onChange();for(int n=0;n<800 && rows->getNumRows()!=1;++n)pump();check(rows->getNumRows()==1 && rows->getCellTooltip(0,3)=="Drum Map" && rows->getCellTooltip(0,7)=="500-548","KS Drum Map filter/address");
   check(rows->getCellTooltip(0,8)=="Bank 5 / Drums","KS bank column");bank->setSelectedId(3,juce::dontSendNotification);bank->onChange();for(int n=0;n<800 && !(rows->getNumRows()==1 && rows->getCellTooltip(0,7)=="207");++n)pump();check(rows->getNumRows()==1 && rows->getCellTooltip(0,7)=="207","KS address must use source bank/slot, not database row");
   bank->setSelectedId(6,juce::dontSendNotification);bank->onChange();for(int n=0;n<800 && !(rows->getNumRows()==1 && rows->getCellTooltip(0,3)=="Drum Map");++n)pump();
  }else for(int n=0;n<rows->getNumRows();++n)check(rows->getCellTooltip(n,3)=="Program","Drum entries exposed in K-Station");
  for(int n=0;n<50;++n)pump();bool drumSource=false;for(int n=0;n<source->getNumItems();++n)drumSource|=source->getItemText(n)==onlyDrums.getFileName();check(drumSource==!p.isKStation(),"Drum-only bank source visible in wrong model");
  const auto capture=juce::SystemStats::getEnvironmentVariable("SILVER_K_UI_CAPTURE",{});if(capture.isNotEmpty()){
   const auto dir=juce::File(capture);dir.createDirectory();auto out=dir.getChildFile(p.isKStation()?"K-browser.png":"KS-browser.png").createOutputStream();juce::PNGImageFormat png;check(out && png.writeImageToStream(browser.createComponentSnapshot(browser.getLocalBounds()),*out),"Browser preview");
  }
 }
 if(!p.isKStation()){Browser restored(p);auto* bank=dynamic_cast<juce::ComboBox*>(find(restored,"ksBank"));check(bank && bank->getSelectedId()==6,"KS bank preference lost");}
 p.browserState.clear();std::cout<<"PASS: "<<(p.isKStation()?"K":"KS")<<" bank visibility, complete-map rows, original addresses and bank preference\n";
}

static void drumPlaybackChecks(Processor& p,const juce::File& root){
 Library library(root.getChildFile("drum-playback-db"));auto grouped=groupDrumMaps(drumFrames(7));const auto& map=grouped[0];auto patch=library.createPatch(Data(map.begin(),map.end()),{});juce::String error;
 check(p.setKSControllerMode(1,error),error.toRawUTF8());
 p.setNonRealtime(true);juce::AudioBuffer<float> audio(4,256);juce::MidiBuffer midi;midi.ensureSize(1024);
 audioAllocations=0;auto render=[&](int count){for(int n=0;n<count;++n){audio.clear();inAudio=true;p.processBlock(audio,midi);inAudio=false;}p.refreshControls();};
 auto snapshot=[&]{juce::MemoryBlock state;p.getStateInformation(state);return juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));};
 check(p.activatePatch(patch,error),error.toRawUTF8());bool stored=false;
 for(int n=0;n<60&&!stored;++n){render(100);auto xml=snapshot();juce::MemoryBlock flash;check(xml && flash.fromBase64Encoding(xml->getStringAttribute("userFlash")),"Drum Flash snapshot");const auto* bytes=static_cast<const uint8_t*>(flash.getData());const size_t offset=0x20000-0x1fc00+(400+2*50)*256;stored=flash.getSize()>=offset+50*256;for(unsigned i=0;i<50&&stored;++i)stored=bytes[offset+i*256+168]==map[i*270+13+168];}
 check(stored,"Complete Drum Map not installed in instance Flash");render(100);
 auto play=[&](int note){midi.addEvent(juce::MidiMessage::noteOn(1,note,uint8_t(100)),0);double peak=0;unsigned crossings=0;float previous=0;
  for(int n=0;n<200;++n){render(1);if(n<100)continue;for(int i=0;i<256;++i){const auto sample=audio.getSample(0,i);check(std::isfinite(sample),"Nonfinite drum audio");peak=std::max(peak,double(std::abs(sample)));if(previous<0 && sample>=0)++crossings;previous=sample;}}
  midi.addEvent(juce::MidiMessage::noteOff(1,note),0);midi.addEvent(juce::MidiMessage::controllerEvent(1,120,0),0);render(60);check(peak>1e-5 && crossings>5,"Silent drum key");return crossings;
 };
 const auto low=play(36),high=play(37);check(high>low*3 && high<low*5,"Drum keys did not select different stored sounds");
 juce::MemoryBlock project;p.getStateInformation(project);p.setStateInformation(project.getData(),int(project.getSize()));render(200);const auto restoredLow=play(36),restoredHigh=play(37);check(restoredHigh>restoredLow*3 && restoredHigh<restoredLow*5,"Drum Map lost on DAW state restore");
 auto melodic=program(Model::KS);auto melodicPatch=library.createPatch(Data(melodic.begin(),melodic.end()),{});check(p.activatePatch(melodicPatch,error),error.toRawUTF8());render(150);
 check(p.setPerformanceMode(true,error),error.toRawUTF8());render(150);const auto before=snapshot();
 check(p.activatePatchForPart(patch,1,error),error.toRawUTF8());render(200);auto after=snapshot();juce::MemoryBlock state;check(after && state.fromBase64Encoding(after->getStringAttribute("program")),"Read Drum Performance");auto* bytes=static_cast<const uint8_t*>(state.getData());check(state.getSize()==1038 && bytes[13+256+187]==7,"Drum bank reference lost in Performance");
 // Route two parts to one user channel: selection must still affect only Part 2.
 p.selectPart(1);auto* channel=p.uiParameter("part_channel");check(channel!=nullptr,"Part channel control");channel->setValueNotifyingHost(channel->convertTo0to1(0));render(100);
 check(p.activatePatchForPart(patch,1,error),error.toRawUTF8());render(200);after=snapshot();check(state.fromBase64Encoding(after->getStringAttribute("program")),"Shared-channel snapshot");bytes=static_cast<const uint8_t*>(state.getData());check(bytes[13+187]!=7 && bytes[13+256+187]==7,"Drum selection changed another layered part");
 p.selectPart(0);check(p.setPerformanceMode(false,error),error.toRawUTF8());render(150);auto normal=program(Model::KS);auto sound=library.createPatch(Data(normal.begin(),normal.end()),{});check(p.activatePatch(sound,error),error.toRawUTF8());render(150);const auto normalLow=play(36),normalHigh=play(37);check(std::abs(int(normalHigh)-int(normalLow))<int(normalLow)/3,"Normal Program retained Drum Map key mapping");
 std::cout<<"PASS: original KS firmware installs complete map, keys 36/37 select different voices ("<<low<<"/"<<high<<"), DAW map restore, layered Performance part isolation, normal Program return\n";
 check(audioAllocations==0,"Drum Map allocated on the audio callback");
 if(!p.drumEditSupported()){
  std::cout<<"SKIP: this controller does not expose legacy Drum Edit\n";
  return;
 }
 check(p.activatePatch(patch,error),error.toRawUTF8());render(150);p.setDrumEdit(true);check(p.drumEditEnabled(),"Drum Edit cannot enable for a map");
 auto onset=[&]{
  midi.addEvent(juce::MidiMessage::noteOn(1,36,uint8_t(100)),0);
  int first=-1;
  for(int block=0;block<16;++block){
   render(1);
   if(first>=0)continue;
   for(int i=0;i<256;++i)if(std::abs(audio.getSample(0,i))>1e-5f){
    first=block*256+i;break;
   }
  }
  midi.addEvent(juce::MidiMessage::noteOff(1,36),0);
  midi.addEvent(juce::MidiMessage::controllerEvent(1,120,0),0);
  render(60);
  check(first>=0,"Drum audition did not produce audio");
  return first;
 };
 p.setDrumEdit(false);render(24);const auto normalOnset=onset();
 p.setDrumEdit(true);render(24);const auto editOnset=onset();
 std::cout<<"DRUM_EDIT_ONSET normal="<<normalOnset<<" edit="<<editOnset<<" samples at 48 kHz\n";
 check(editOnset/256==normalOnset/256,"Drum Edit delayed audition by an audio block");
 // 64 ms includes the plugin reserve and snapshot publication. A one-second
 // wait hid the serial-store/idle-boundary delay during the original checks.
 auto selectDrum=[&](int note,int channel=1,int velocity=100){midi.addEvent(juce::MidiMessage::noteOn(channel,note,uint8_t(velocity)),0);midi.addEvent(juce::MidiMessage::noteOff(channel,note),128);render(12);};
 selectDrum(36);check(p.drumEditInfo().note==36,"Drum Edit first key");
 auto checkDrumUI=[&](int note){std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());auto* edit=dynamic_cast<juce::TextButton*>(find(*editor,"drumEdit"));auto* display=dynamic_cast<juce::Label*>(find(*editor,"drumEditDisplay"));auto* perf=find(*editor,"performanceSwitch");check(edit && edit->isVisible() && edit->getToggleState() && display && display->isVisible() && display->getText().contains("MIDI "+juce::String(note)),"Drum Edit button/display");check(perf && !perf->getBounds().intersects(edit->getBounds()),"Drum Edit overlaps Performance");const auto capture=juce::SystemStats::getEnvironmentVariable("SILVER_K_UI_CAPTURE",{});if(capture.isNotEmpty()){const auto dir=juce::File(capture);dir.createDirectory();auto out=dir.getChildFile("KS-DrumEdit-"+juce::String(note)+".png").createOutputStream();juce::PNGImageFormat png;check(out && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*out),"Drum Edit screenshot");juce::MemoryBlock wrapperState;p.getStateInformation(wrapperState);check(dir.getChildFile("KS-DrumEdit.state").replaceWithData(wrapperState.getData(),wrapperState.getSize()),"Save Drum Edit wrapper fixture");}};
 const auto reviewSkin=p.skinChoice;
 p.skinChoice="Silver-K";
 checkDrumUI(36);
 p.skinChoice=reviewSkin;
 auto* cutoff=p.uiParameter("cutoff");check(cutoff!=nullptr,"Drum cutoff missing");const auto initialCutoff=int(cutoff->convertFrom0to1(cutoff->getValue()));
 cutoff->setValueNotifyingHost(cutoff->convertTo0to1(31));
 auto* delay=p.uiParameter("delay");check(delay!=nullptr,"Drum FX control missing");delay->setValueNotifyingHost(delay->convertTo0to1(99));
 selectDrum(37);check(p.drumEditInfo().note==37,"Drum Edit second key");check(int(cutoff->convertFrom0to1(cutoff->getValue()))==initialCutoff,"Knobs did not refresh for selected drum");
 p.skinChoice="Silver-K";
 checkDrumUI(37);
 p.skinChoice=reviewSkin;
 check(int(delay->convertFrom0to1(delay->getValue()))==99,"Shared FX lost during Drum Edit switch");
 cutoff->setValueNotifyingHost(cutoff->convertTo0to1(52));selectDrum(36);check(int(cutoff->convertFrom0to1(cutoff->getValue()))==31,"First drum edit lost during immediate switch");
 p.setDrumEdit(false);selectDrum(37);check(p.drumEditInfo().note==36,"Drum Edit off still selects keys");
 p.setDrumEdit(true);selectDrum(35);selectDrum(85);selectDrum(37,2);selectDrum(37,1,0);check(p.drumEditInfo().note==36,"Out-of-range/channel/zero-velocity note selected a drum");
 p.getStateInformation(project);p.setStateInformation(project.getData(),int(project.getSize()));render(150);p.setDrumEdit(true);selectDrum(37);check(p.drumEditInfo().note==37 && int(p.uiParameter("cutoff")->convertFrom0to1(p.uiParameter("cutoff")->getValue()))==52,"Drum edits lost after project restore");
 selectDrum(36);check(int(p.uiParameter("cutoff")->convertFrom0to1(p.uiParameter("cutoff")->getValue()))==31,"First drum lost after restore");
 const auto editedFile=root.getChildFile("edited-map.syx");check(p.saveCurrentPatch(editedFile,error),error.toRawUTF8());juce::MemoryBlock editedBytes;check(editedFile.loadFileAsData(editedBytes),"Read edited map");DataList editedMaps;const auto* edited=static_cast<const uint8_t*>(editedBytes.getData());check(library.parseFileData(editedMaps,Data(edited,edited+editedBytes.getSize()),"edited.syx") && editedMaps.size()==1 && editedMaps[0][13+46]==31 && editedMaps[0][270+13+46]==52 && editedMaps[0][49*270+13+100]==99,"Save current lost drum edits/shared FX");
 check(p.setPerformanceMode(true,error),error.toRawUTF8());render(150);
 p.selectPart(1);
 check(p.drumEditInfo().bank==7,"Performance lost the Drum Map after the project restore");
 auto* partChannel=p.uiParameter("part_channel");check(partChannel!=nullptr,"Part channel missing");
 partChannel->setValueNotifyingHost(partChannel->convertTo0to1(0));render(150);
 p.setDrumEdit(true);selectDrum(37,1);
 check(p.drumEditInfo().note==37,"Performance Drum Edit failed");
 after=snapshot();check(state.fromBase64Encoding(after->getStringAttribute("program")),"Performance Drum Edit snapshot");
 bytes=static_cast<const uint8_t*>(state.getData());
 check(bytes[13+188]==0 && bytes[13+256+188]==0,"Audition changed a loaded Performance program");
 p.setDrumEdit(false);
 using Edit=Processor::DrumGridEdit;
 const auto writeGrid=[&](unsigned part,uint8_t velocity,bool retrigger){
  p.selectPart(part);render(24);
  const auto slot=p.drumEditorSlot();
  check(p.selectDrumRhythm(slot,Processor::UserDrumRhythm,error),error.toRawUTF8());
  for(const Edit edit:std::initializer_list<Edit>{
      {Edit::Kind::SelectPattern,slot,0,0,11},{Edit::Kind::Length,slot,11,0,5},
      {Edit::Kind::Velocity,slot,11,0,velocity},{Edit::Kind::Probability,slot,11,0,79},
      {Edit::Kind::Ratchet,slot,11,0,3},{Edit::Kind::KeyLow,slot,11,0,48},
      {Edit::Kind::KeyHigh,slot,11,0,62},{Edit::Kind::SwitchAfter,slot,0,0,8},
      {Edit::Kind::Retrigger,slot,0,0,uint8_t(retrigger)}})
   check(p.editDrumGrid(edit,error),error.toRawUTF8());
  render(50);
 };
 writeGrid(0,94,true);
 writeGrid(1,71,false);
 p.getStateInformation(project);
 const auto projectXml=juce::AudioProcessor::getXmlFromBinary(project.getData(),int(project.getSize()));
 juce::MemoryBlock savedDrumBytes;
 check(projectXml && savedDrumBytes.fromBase64Encoding(projectXml->getStringAttribute("ksUserDrums")) &&
       savedDrumBytes.getSize()>0 && savedDrumBytes[0]==5,
       "Keep the drum grid readable by the previous VST3");
 p.setStateInformation(project.getData(),int(project.getSize()));
 render(150);
 p.selectPart(0);render(24);
 check(p.drumSequencerInfo().sequences.retrigger,"The project keeps Retrigger on for Part 1");
 p.selectPart(1);render(24);
 check(!p.drumSequencerInfo().sequences.retrigger,"The project keeps Retrigger off for Part 2");
 const auto user=p.userPatchesFolder().getChildFile("Workflow test");
 check(user.createDirectory().wasOk(),"Create a User subfolder");
 const auto layer=user.getChildFile("Saved part.silver-k"),whole=user.getChildFile("Saved performance.silver-k");
 check(p.saveUserPatch(layer,Processor::PatchSaveScope::SelectedSound,error),error.toRawUTF8());
 check(p.saveUserPatch(whole,Processor::PatchSaveScope::Performance,error),error.toRawUTF8());
 const auto readSaved=[&](const juce::File& path){
  DataList decoded;
  check(library.loadFile(decoded,path.getFullPathName().toStdString()) && decoded.size()==1,"Import saved Silver-K preset");
  auto preset=library.createPatch(std::move(decoded.front()),{});
  check(preset && isKSPlus(*preset),"Authored drum grids should be KS+");
  return preset;
 };
 const auto partPatch=readSaved(layer),performancePatch=readSaved(whole);
 check(patchDump(*partPatch).size()==DrumMapSize && patchDump(*performancePatch).size()==1038,"Save part and entire Performance with different scopes");
 const auto exportPath=root.getChildFile("portable.silver-k");
 check(!library.writePatchesToFile(root.getChildFile("blocked.syx"),{performancePatch}),"KS+ leaked to SysEx");
 check(library.writePatchesToFile(exportPath,{performancePatch}) && readSaved(exportPath)->sysex==performancePatch->sysex,
       "Export entire KS+ including maps and sequences");
 {
  Browser browser(p);
  auto* sources=dynamic_cast<juce::ComboBox*>(find(browser,"patchSource"));
  check(sources!=nullptr,"User source selector");
  bool selected=false,foundPart=false,foundPerformance=false,foundTemplates=false;
  for(int attempt=0;attempt<400;++attempt){
   browser.pollLibrary();
   if(!selected)for(int i=0;i<sources->getNumItems();++i)if(sources->getItemText(i)=="User"){
    sources->setSelectedId(sources->getItemId(i),juce::dontSendNotification);
    sources->onChange();selected=true;break;
   }
   const auto view=browser.webSnapshot();
   if(selected && !bool(view["loading"])){
    const auto* rows=view["rows"].getArray();
    juce::StringArray templates;
    if(rows)for(const auto& row:*rows){
     const auto title=row["name"].toString();
     foundPart|=title=="Saved part" && bool(row["ksPlus"]);
     foundPerformance|=title=="Saved performance" && bool(row["ksPlus"]);
     if(title.startsWith("Init "))templates.addIfNotAlreadyThere(title);
    }
    foundTemplates=templates.contains("Init KS") && templates.contains("Init K-Station") &&
                   templates.contains("Init Drum Map") && templates.contains("Init Performance");
    if(foundPart && foundPerformance && foundTemplates)break;
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  check(selected && foundPart && foundPerformance && foundTemplates,"New User presets and templates should appear without Rescan");
 }
 const auto normalDump=initPatch(Model::KS);
 const auto normalFile=encodeNativePatch(normalDump,"Normal test");
 const auto ordinary=library.createPatch(Data(normalFile.begin(),normalFile.end()),{});
 const auto sysEx=root.getChildFile("ordinary-export.syx");
 check(ordinary && !isKSPlus(*ordinary) && library.writePatchesToFile(sysEx,{ordinary}),
       "An ordinary User preset should export as SysEx");
 juce::MemoryBlock sysExBytes;
 check(sysEx.loadFileAsData(sysExBytes) && sysExBytes.getSize()==normalDump.size() &&
       std::equal(normalDump.begin(),normalDump.end(),static_cast<const uint8_t*>(sysExBytes.getData())),
       "Standard SysEx export must contain the hardware dump");
 writeGrid(0,20,false);
 writeGrid(1,22,true);
 check(p.activatePatchForPart(partPatch,1,error),error.toRawUTF8());render(175);
 const auto savedGrid=p.drumSequencerInfo().sequences;
 check(savedGrid.selected && savedGrid.selectedPattern==11 && savedGrid.patterns[11].steps==5 &&
       savedGrid.patterns[11].velocities[0]==71 && savedGrid.patterns[11].probabilities[0]==79 &&
       savedGrid.patterns[11].ratchets[0]==3 && savedGrid.keyRanges[11].high==62 && savedGrid.switchAfter==8 && !savedGrid.retrigger,
       "Part reload lost sequencer data");
 p.selectPart(0);render(24);
 check(p.drumSequencerInfo().sequences.patterns[11].velocities[0]==20 && !p.drumSequencerInfo().sequences.retrigger && p.drumEditInfo().bank==7,
       "Loading a part replaced a different part's sequencer or Drum Map");
 check(p.activatePatch(performancePatch,error),error.toRawUTF8());render(200);
 check(p.performanceMode() && p.drumSequencerInfo().sequences.patterns[11].velocities[0]==94 &&
       p.drumSequencerInfo().sequences.retrigger,"Whole Performance lost Part 1");
 p.selectPart(1);render(24);
 check(p.drumSequencerInfo().sequences.patterns[11].velocities[0]==71 && !p.drumSequencerInfo().sequences.retrigger &&
       p.drumEditInfo().bank==7,"Whole Performance lost Part 2 and its map");
 std::cout<<"PASS: User part and Performance KS+ save/import, portable export, sequence options/part isolation and SysEx guard\n";
 p.selectPart(0);check(p.setPerformanceMode(false,error),error.toRawUTF8());render(150);check(p.activatePatch(sound,error),error.toRawUTF8());render(150);check(p.drumEditInfo().bank==0,"Ordinary program still exposes Drum Edit");
 std::cout<<"PASS: Drum Edit MIDI selection, immediate knob edits retained on both keys, off/range/channel/velocity guards, DAW restore and Performance part isolation\n";
 check(audioAllocations==0,"Drum Edit allocated on audio callback");
}

static void importChecks(Library& library){
 for(auto model:{Model::AStation,Model::KStation,Model::KS,Model::XStation}){
  auto bytes=program(model);check(validPatch(bytes) && modelOf(bytes)==model,"Source model identification");
  auto patch=library.createPatch(Data(bytes.begin(),bytes.end()),"Test bank");check(patch!=nullptr,"Source patch import");
  for(auto target:{Model::KStation,Model::KS}){auto converted=convertPatch(patch->sysex,target);check(validPatch(converted.data) && modelOf(converted.data)==target,"Playable conversion");}
  check(std::equal(bytes.begin(),bytes.end(),patch->sysex.begin()),"Library source bytes changed");
  if(model==Model::AStation || model==Model::KStation){
   Data pair(bytes.begin(),bytes.begin()+13);pair[7]=2;pair[11]=2;pair[12]=8;pair.insert(pair.end(),bytes.begin()+13,bytes.end()-1);pair.insert(pair.end(),bytes.begin()+13,bytes.end()-1);pair.back()=42;pair.push_back(0xf7);
   DataList parsed;check(library.parseFileData(parsed,pair,"pair.syx") && parsed.size()==2,"Program pair expansion");
   auto first=library.createPatch(Data(parsed[0]),{}),second=library.createPatch(Data(parsed[1]),{});
   check(first && second && first->sysex[12]==8 && second->sysex[12]==9 && second->sysex[140]==42,"Program pair address/payload");
   check(first->name==modelName(model).toStdString()+" 208" && second->name==modelName(model).toStdString()+" 209","Bank programs need numbered names");
   pair[12]=9;parsed.clear();check(!library.parseFileData(parsed,pair,"bad.syx"),"Odd pair address rejected");
  }
 }
 auto x=program(Model::XStation);x[13+250]=77;auto native=convertPatch(x,Model::KS);check(native.data[13+250]==77,"Unknown X-Station tail must survive compatible conversion");
 auto a=program(Model::AStation);auto k=convertPatch(a,Model::KStation);check(std::equal(a.begin()+13,a.end()-1,k.data.begin()+13),"A/K exact sound payload");
 a[9]=8;a[10]=7;a[13+43]=52;check(convertPatch(a,Model::KStation).data[13+43]==0,"Legacy A filter drive");a[10]=8;check(convertPatch(a,Model::KStation).data[13+43]==52,"Modern A filter drive");
 for(int glide=0;glide<2;++glide){auto data=program(Model::KStation);data[18]=uint8_t((data[18]&63)|(glide<<6));auto extended=convertPatch(data,Model::KS);check(extended.data[18]==glide,"K/KS glide encoding");check((convertPatch(extended.data,Model::KStation).data[18]>>6)==glide,"Glide round trip");}
 auto invalidBoost=program(Model::KS);invalidBoost[13+125]=127;check(convertPatch(invalidBoost,Model::KStation).data[13+125]==64,"Invalid boost must default without seven-bit wrap");
 auto midi=juce::MidiFile{};juce::MidiMessageSequence track;track.addEvent(juce::MidiMessage(a.data(),int(a.size()),0));track.addEvent(juce::MidiMessage(x.data(),int(x.size()),1));midi.setTicksPerQuarterNote(96);midi.addTrack(track);juce::MemoryOutputStream stream;check(midi.writeTo(stream),"MIDI fixture creation");const auto* p=static_cast<const uint8_t*>(stream.getData());DataList parsed;check(library.parseFileData(parsed,Data(p,p+stream.getDataSize()),"mixed.mid") && parsed.size()==2,"Mixed-model MIDI import");
 std::cout<<"PASS: four models, A/K pairs, MIDI import, exact source preservation and conversion\n";
}
static Data vStationBankFixture(bool includeUserSlots=false){
 const auto sound=program(Model::KStation);
 Data bank(59768,0);auto word=[&](size_t offset,uint32_t v){for(unsigned n=0;n<4;++n)bank[offset+n]=uint8_t(v>>(24-n*8));};
 std::copy_n("CcnK",4,bank.begin());std::copy_n("FBCh",4,bank.begin()+8);std::copy_n("NvS0",4,bank.begin()+16);word(12,1);word(20,1);word(24,400);word(156,59608);bank[160]=1;
 for(unsigned n=0;n<400;++n){
  std::copy_n(sound.begin()+13,128,bank.begin()+161+n*128);
  const auto name=std::string(includeUserSlots && n?"User ":"V preset ")+std::to_string(n);
  std::copy(name.begin(),name.end(),bank.begin()+160+51201+n*21);
 }
 return bank;
}
static void vStationChecks(Library& library){
 auto bank=vStationBankFixture();
 auto verify=[&](const Data& bytes,const DataList& frames){check(frames.size()==400,"V-Station bank expansion");std::set<PatchHash> identities;
  for(unsigned n=0;n<400;++n){auto p=library.createPatch(Data(frames[n]),"V-Station");check(p && validPatch(p->sysex),"V-Station playable K patch");
   check(patchModel(*p)==Model::VStation && p->tags.containsAdded(TagType::CustomA,"V-Station"),"V-Station must retain its source model");
   check(std::equal(p->sysex.begin()+13,p->sysex.end()-1,bytes.begin()+161+n*128),"V-Station sound payload changed");check(p->bank==1+n/100 && p->program==n%100,"V-Station bank slot address");
   const auto* text=bytes.data()+160+51201+n*21;const auto end=std::find(text,text+21,uint8_t(0));const auto name=juce::String::fromUTF8(reinterpret_cast<const char*>(text),int(end-text)).trim();if(name.isNotEmpty())check(p->name==name.toStdString(),"V-Station patch name changed");
   check(validPatch(convertPatch(p->sysex,Model::KS).data),"V-Station conversion to KS");identities.insert(p->hash);
  }
  check(identities.size()==400,"Named V-Station slots collapsed");
 };
 DataList frames;check(library.parseFileData(frames,bank,"test.fxb"),"V-Station FXB parser");verify(bank,frames);
 for(int damage=0;damage<5;++damage){auto invalid=bank;if(damage==0)invalid.pop_back();if(damage==1)invalid[16]='x';if(damage==2)invalid[160]=2;if(damage==3)invalid[161+399*128]=128;if(damage==4)invalid[159]=0;DataList rejected;check(!library.parseFileData(rejected,invalid,"bad.fxb") && rejected.empty(),"Invalid FXB accepted/partially imported");}
 const auto archive=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_VSTATION",{});
 if(archive.isNotEmpty()){
  DataList real;check(library.loadFile(real,archive.toStdString()),"Private V-Station ZIP import");juce::ZipFile zip{juce::File(archive)};bool verified=false;
  for(int n=0;n<zip.getNumEntries();++n)if(zip.getEntry(n)->filename.endsWithIgnoreCase(".fxb")){std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(n));juce::MemoryBlock data;check(stream && stream->readIntoMemoryBlock(data)>0,"Read private FXB");auto* p=static_cast<const uint8_t*>(data.getData());verify(Data(p,p+data.getSize()),real);verified=true;}
  check(verified,"No FXB in private ZIP");
 }
 std::cout<<"PASS: 400 V-Station slots, original names/128-byte payloads, K/KS conversion, invalid FXB rejection\n";
}
static Data fxpFixture(){
 Data file(209,0);auto word=[&](size_t at,uint32_t value){for(unsigned n=0;n<4;++n)file[at+n]=uint8_t(value>>(24-n*8));};
 std::copy_n("CcnK",4,file.begin());std::copy_n("FPCh",4,file.begin()+8);std::copy_n("NvS0",4,file.begin()+16);
 word(4,201);word(12,1);word(20,1);word(24,1);word(56,149);std::copy_n("User 300",8,file.begin()+28);
 auto sound=program(Model::KStation);std::copy_n(sound.begin()+13,128,file.begin()+60);std::copy_n("Named V sound",13,file.begin()+188);return file;
}
static void vStationProgramChecks(Library& library){
 const auto file=fxpFixture();DataList frames;check(library.parseFileData(frames,file,"v.fxp") && frames.size()==1,"V-Station FXP import");
 auto patch=library.createPatch(Data(frames[0]),{});check(patch && patch->name=="Named V sound" && patchModel(*patch)==Model::VStation,"FXP chunk name and source model");
 check(std::equal(patch->sysex.begin()+13,patch->sysex.end()-1,file.begin()+60),"FXP sound bytes changed");
 for(int damage=0;damage<7;++damage){auto invalid=file;if(damage==0)invalid.pop_back();if(damage==1)invalid[16]='X';if(damage==2)invalid[15]=2;if(damage==3)invalid[60]=128;if(damage==4)invalid[59]=148;if(damage==5)invalid[188]=31;if(damage==6)invalid[27]=2;DataList rejected;check(!library.parseFileData(rejected,invalid,"bad.fxp") && rejected.empty(),"Malformed FXP accepted");}
 patch->tags.erase(TagType::Tag,"V-Station");check(patchModel(*patch)==Model::VStation,"User tags must not determine new V-Station type");
 const auto legacyBytes=program(Model::KStation);auto legacy=library.createPatch(Data(legacyBytes.begin(),legacyBytes.end()),{});legacy->tags.add(TagType::Tag,"V-Station");check(patchModel(*legacy)==Model::VStation,"Legacy V-Station bank classification");
 const auto folder=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_VSTATION_FOLDER",{});
 if(folder.isNotEmpty()){
  const auto files=juce::File(folder).findChildFiles(juce::File::findFiles,true,"*.fxp");check(!files.isEmpty(),"No private FXP files");std::set<PatchHash> unique;
  for(const auto& source:files){DataList loaded;check(library.loadFile(loaded,source.getFullPathName().toStdString()) && loaded.size()==1,"Private FXP rejected");auto p=library.createPatch(Data(loaded[0]),{});juce::MemoryBlock raw;check(source.loadFileAsData(raw),"Read private FXP");auto* bytes=static_cast<const uint8_t*>(raw.getData());const auto nameEnd=std::find(bytes+188,bytes+209,uint8_t(0));check(p && patchModel(*p)==Model::VStation && juce::String(p->name)==juce::String::fromUTF8(reinterpret_cast<const char*>(bytes+188),int(nameEnd-(bytes+188))).trim(),"Private FXP name/type");check(std::equal(p->sysex.begin()+13,p->sysex.end()-1,bytes+60),"Private FXP sound payload");unique.insert(p->hash);}
  std::cout<<"PASS: private FXP files="<<files.size()<<", unique named sounds="<<unique.size()<<'\n';
 }
 std::cout<<"PASS: FXP sound/name import, independent V-Station type, legacy bank identity, malformed FXP rejection\n";
}
static void namingChecks(Library& library){
 for(auto model:{Model::KStation,Model::AStation}){
  const auto bytes=program(model);DataList frames;check(library.parseFileData(frames,Data(bytes.begin(),bytes.end()),"Readable patch.syx"),"Named single import");auto named=library.createPatch(Data(frames[0]),{});check(named && named->name=="Readable patch" && std::equal(named->sysex.begin(),named->sysex.end(),bytes.begin(),bytes.end()),"SysEx filename and original bytes");
  Data batch(bytes.begin(),bytes.end());batch.insert(batch.end(),bytes.begin(),bytes.end());frames.clear();check(library.parseFileData(frames,batch,"bank.syx") && frames.size()==2,"Unnamed batch import");
  for(unsigned n=0;n<2;++n){auto patch=library.createPatch(Data(frames[n]),{});check(patch && patch->name==modelName(model).toStdString()+" 00"+std::to_string(n+1) && std::equal(patch->sysex.begin(),patch->sysex.end(),bytes.begin(),bytes.end()),"Sequential names for edit-buffer dumps");}
  auto old=library.createPatch(Data(bytes.begin(),bytes.end()),{});old->name=" 0";old->program=9;check(patchName(*old)==modelName(model)+" 010","Cached zero name fallback");
 }
 std::cout<<"PASS: K/A filename names, sequential unnamed bank programs, cached-zero fallback, unchanged sound bytes\n";
}
static void cacheMigrationChecks(const juce::File& root){
 const auto db=root.getChildFile("old-cache"),file=root.getChildFile("Recovered filename.syx");const auto bytes=program(Model::AStation);check(file.replaceWithData(bytes.data(),bytes.size()),"Cache migration fixture");
 auto wait=[&](Library& library,bool recovered){auto handle=library.search(SearchRequest{});PatchPtr result;for(int n=0;n<400;++n){library.uiProcess();if(auto search=library.getSearch(handle)){std::shared_lock lock(search->resultsMutex);for(const auto& p:search->results)if(!recovered || p->name=="Recovered filename")result=p;}if(result && !library.isLoading())break;std::this_thread::sleep_for(std::chrono::milliseconds(10));}library.cancelSearch(handle);check(bool(result),"Cache migration timed out");return result;};
 {
  Library library(db);library.addPath(file);auto patch=wait(library,false);TypedTags tags;tags.add(TagType::Favourites,"Favourite");check(library.modifyTags({patch},tags),"Favourite fixture");
  for(int n=0;n<30;++n){library.uiProcess();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
  patch->name=" 0";check(db.getChildFile("silver-import-version.txt").replaceWithText("5\n"),"Mark disposable old import cache");
 }
 {
  Library library(db);auto patch=wait(library,true);check(patch->getTags(TagType::Favourites).containsAdded("Favourite"),"Source migration lost favourites");check(db.getChildFile("silver-import-version.txt").loadFileAsString().trim()=="6","Migration completion not remembered");
 }
 std::cout<<"PASS: old zero-name cache refreshed from source, favourites preserved, migration remembered\n";
}
static void queueChecks(){
 KCutoffMidi cutoff;
 const uint8_t first[]{0xb0,105,127},changed[]{0xb0,105,64},otherChannel[]{0xb1,105,64},clock[]{0xf8};
 check(!cutoff.duplicate(first,3),"First cutoff was suppressed");cutoff.accepted(first,3);
 for(int n=0;n<10000;++n){check(cutoff.duplicate(first,3),"Repeated cutoff reached UART");cutoff.accepted(clock,1);}
 check(!cutoff.duplicate(changed,3),"Changed cutoff was suppressed");cutoff.accepted(changed,3);
 check(!cutoff.duplicate(otherChannel,3),"Different MIDI channel was suppressed");
 for(const auto& message:std::vector<std::vector<uint8_t>>{{0x90,60,100},{0x80,60,0},{0xb0,64,127},{0xb0,98,11},{0xb0,6,64},{0xc0,1},{0xf0,0,0xf7},{0xfa},{0xfc}}){
  cutoff.accepted(changed,3);cutoff.accepted(message.data(),message.size());check(!cutoff.duplicate(changed,3),"Cutoff cache survived an intervening message");
  check(!cutoff.duplicate(message.data(),message.size()),"Non-cutoff MIDI was suppressed");
 }
 cutoff.accepted(first,3);cutoff.reset();check(!cutoff.duplicate(first,3),"Rejected MIDI poisoned cutoff cache");
 kstation::MidiSerialQueue<1024,16> queue;
 const uint8_t cc[]{0xb0,72,42};for(int n=0;n<300;++n)check(queue.push(cc,3),"Controller queue");
 const uint8_t rt[]{0xfa,0xf8,0xfc};check(queue.push(rt,3),"Realtime queue");uint8_t b=0;
 for(auto expected:rt)check(queue.pop(b) && b==expected,"Realtime byte delayed by controls");
 for(int n=0;n<900;++n)check(queue.pop(b) && b==cc[n%3],"Realtime damaged channel message order");
 check(!queue.pop(b),"UART queue empty");
 std::cout<<"PASS: realtime UART bytes pass 900 queued controller bytes; channel byte order preserved\n";
}
static void folderChecks(Processor& p,const juce::File& dir){
 const auto original=p.dataFolder;juce::String error;const auto folder=dir.getChildFile("Managed Silver-K");
 check(p.setDataFolder(folder,error),error.toRawUTF8());
 const auto zero=folder.getChildFile("ROMs/ks-wave-zero.bin"),init=folder.getChildFile("ROMs/ks-init-flash.bin");
 check(zero.getSize()==129024 && !init.exists() && p.patchesFolder().isDirectory() && p.skinsFolder().isDirectory(),"Managed folder layout, required zero wave, no synthetic Data");
 juce::MemoryBlock data;check(zero.loadFileAsData(data),"Read placeholder");check(std::all_of(static_cast<const uint8_t*>(data.getData()),static_cast<const uint8_t*>(data.getData())+data.getSize(),[](uint8_t b){return b==0;}),"Placeholder is not zero-filled");
 data[0]=19;check(zero.replaceWithData(data.getData(),data.getSize()),"Replace local Wave ROM");check(p.setDataFolder(folder,error),"Reopen managed folder");juce::MemoryBlock reread;zero.loadFileAsData(reread);check(reread[0]==19,"Existing Wave ROM overwritten");
 {Processor next;check(next.dataFolder==folder,"Main folder preference not remembered");}
 const auto preference=juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",{})).getChildFile("settings.json");
 auto cached=juce::JSON::parse(preference);juce::DynamicObject::Ptr choice=new juce::DynamicObject;choice->setProperty("file","cached-os.bin");choice->setProperty("kStation",true);juce::Array<juce::var> choices;choices.add(juce::var(choice.get()));cached.getDynamicObject()->setProperty("firmwares",choices);check(folder.getChildFile("ROMs/cached-os.bin").replaceWithText("Invalid firmware"),"Write cached file fixture");check(preference.replaceWithText(juce::JSON::toString(cached)),"Write cached index fixture");
 {Processor next;check(next.firmwares.empty() && next.firmwareAfterFolderScan()==juce::File{},"Startup accepted an invalid firmware from the old index");}
 check(p.setDataFolder(folder,error,true) && p.firmwares.empty(),"Explicit folder scan accepted invalid firmware");
 check(p.setDataFolder(original,error),"Restore test folder");
 std::cout<<"PASS: managed ROMs/Patches/Skins, zero Wave ROM, no synthetic KS Data, remembered root, existing files preserved\n";
}
static void userSourceUpgradeChecks(Processor& p,const juce::File& dir) {
 const auto previous=p.dataFolder;
 const auto folder=dir.getChildFile("Existing Silver-K library");
 const auto patches=folder.getChildFile("Patches");
 check(patches.createDirectory().wasOk(),"Create old managed Patches");
 p.patchLibrary.addPath(patches,true);
 DataSource key;
 key.type=SourceType::Folder;
 key.name=patches.getFullPathName().toStdString();
 for(int i=0;i<400 && (!p.patchLibrary.getDataSource(key) || p.patchLibrary.isScanning());++i){
  p.patchLibrary.uiProcess();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
 }
 check(p.patchLibrary.getDataSource(key) && !p.patchLibrary.isScanning(),"Index Patches before User exists");
 const auto user=patches.getChildFile("User/Nested");
 check(user.createDirectory().wasOk(),"Create User after indexing Patches");
 const auto patch=encodeNativePatch(initPatch(Model::KS),"Existing user preset");
 check(user.getChildFile("Existing user preset.silver-k").replaceWithData(patch.data(),patch.size()),
       "Write an existing User preset");
 juce::String error;
 check(p.setDataFolder(folder,error),error.toRawUTF8());
 {
  Browser browser(p);
  auto* source=dynamic_cast<juce::ComboBox*>(find(browser,"patchSource"));
  check(source!=nullptr,"Find Browser source");
  bool selected=false;
  juce::StringArray names;
  for(int attempt=0;attempt<400;++attempt){
   browser.pollLibrary();
   if(!selected)for(int n=0;n<source->getNumItems();++n)if(source->getItemText(n)=="User"){
    source->setSelectedId(source->getItemId(n),juce::dontSendNotification);
    source->onChange();
    selected=true;
    break;
   }
   const auto view=browser.webSnapshot();
   if(selected && !bool(view["loading"]))if(const auto* rows=view["rows"].getArray()){
    names.clear();
    for(const auto& row:*rows)names.add(row["name"].toString());
    if(names.size()==5)break;
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  check(selected && names.size()==5 && names.contains("Existing user preset") &&
        names.contains("Init KS") && names.contains("Init K-Station") &&
        names.contains("Init Drum Map") && names.contains("Init Performance"),
        "Existing Patches tree must discover User and all four Init presets without Rescan");
 }
 check(p.setDataFolder(previous,error),error.toRawUTF8());
 p.browserState.clear();
 std::cout<<"PASS: existing managed Patches discovers User, nested presets and four Init presets without Rescan\n";
}
static void bufferChecks(Processor& p){
 const juce::ScopedValueSetter<juce::String> useClassic(p.skinChoice,"Silver-K");
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 auto* folder=find(*editor,"dataFolder");
 auto* rescan=find(*editor,"rescanRoms");
 auto* guide=dynamic_cast<juce::Label*>(find(*editor,"setupHelp"));
 check(folder && rescan && guide && guide->getText().contains("Patches"),"Setup is missing the managed data controls or folder guide");
 for(const auto* removed:{"loadFirmware","waveRom","factoryData","loadSkin"})check(!find(*editor,removed),"Old separate file picker in Setup");
 auto* select=dynamic_cast<juce::ComboBox*>(find(*editor,"audioBuffer"));check(select && select->getNumItems()==4,"Setup buffer selector");
 for(int profile=0;profile<4;++profile){
  select->setSelectedId(profile+1,juce::dontSendNotification);select->onChange();check(p.bufferProfile()==profile,"Setup buffer selection not applied");
  juce::MemoryBlock saved;p.getStateInformation(saved);auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));check(xml && xml->getIntAttribute("bufferProfile",-1)==profile,"Buffer profile not serialized");
 }
 p.setBufferProfile(0);std::cout<<"PASS: all four Setup buffer choices apply and persist in session state\n";
}
static void browserChecks(Processor& p,const juce::File& dir){
 for(auto model:{Model::AStation,Model::KS,Model::XStation}){auto data=program(model);auto file=dir.getChildFile(modelName(model)+".syx");check(file.replaceWithData(data.data(),data.size()),"Write browser fixture");}
 const auto fxp=fxpFixture();check(dir.getChildFile("V sound.fxp").replaceWithData(fxp.data(),fxp.size()),"Write browser V-Station fixture");
 p.patchLibrary.addPath(dir);Browser browser(p);browser.setBounds(0,0,1280,600);auto pump=[&]{std::this_thread::sleep_for(std::chrono::milliseconds(20));browser.pollLibrary();};
 auto* table=dynamic_cast<juce::TableListBox*>(find(browser,"patchTable"));check(table!=nullptr,"Patch table");auto* model=table->getTableListBoxModel();
 auto* source=dynamic_cast<juce::ComboBox*>(find(browser,"patchSource"));check(source!=nullptr,"Source filter");
 for(int n=0;n<200;++n){pump();bool found=false;for(int i=0;i<source->getNumItems();++i)if(source->getItemText(i)==dir.getFileName()){source->setSelectedId(source->getItemId(i),juce::dontSendNotification);found=true;break;}if(found)break;}
 source->onChange();for(int n=0;n<200 && model->getNumRows()!=4;++n)pump();std::cout<<"Browser rows="<<model->getNumRows()<<" sources="<<source->getNumItems()<<" selected="<<source->getText()<<" revision="<<p.patchLibrary.revision.load()<<std::endl;check(model->getNumRows()==4,"Browser folder scan");
 auto text=[&](int row,int col){return model->getCellTooltip(row,col);};
 for(int column=1;column<=6;++column)for(bool forward:{true,false}){
  model->sortOrderChanged(column,forward);for(int row=1;row<4;++row){const auto order=text(row-1,column).compareNatural(text(row,column));check(forward?order<=0:order>=0,"Column sorting direction");}
 }
 model->sortOrderChanged(1,true);table->selectRow(0);const auto selected=text(0,1),loaded=p.currentPatch;
 if(p.firmwareReady())check(loaded==selected,"Selecting a row must load it");
 const auto clickTime=juce::Time::getCurrentTime()+juce::RelativeTime::seconds(1);
 const juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(),{10,10},juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,table,table,clickTime,{10,10},clickTime,1,false);
 if(p.firmwareReady()){p.currentPatch="Edited sound";model->cellClicked(0,1,click);check(p.currentPatch==selected,"Re-clicking the selected row must load");p.currentPatch="Edited sound";model->returnKeyPressed(0);check(p.currentPatch==selected,"Enter must reload selected patch");}
 model->sortOrderChanged(1,false);check(text(table->getSelectedRow(),1)==selected && p.currentPatch==loaded,"Sort must retain selection without reloading");
 auto* filter=dynamic_cast<juce::ComboBox*>(find(browser,"patchModel"));check(filter!=nullptr,"Model filter");filter->setSelectedId(5,juce::dontSendNotification);filter->onChange();
 for(int n=0;n<200 && !(model->getNumRows()==1 && text(0,2)=="X-Station");++n)pump();
 check(model->getNumRows()==1 && text(0,2)=="X-Station","X-Station filter");
 check(table->getSelectedRow()<0 && p.currentPatch==loaded,"Filtering must not load an unrelated row");table->selectRow(0);
 if(p.firmwareReady())check(p.currentPatch==text(0,1),"X-Station single-selection load");
 filter->setSelectedId(6,juce::dontSendNotification);filter->onChange();for(int n=0;n<200 && !(model->getNumRows()==1 && text(0,2)=="V-Station");++n)pump();check(model->getNumRows()==1 && text(0,2)=="V-Station" && text(0,1)=="Named V sound","V-Station source-type filter");if(p.isKStation())check(text(0,5)=="Native compatible","V-Station must load directly into K sound format");table->selectRow(0);if(p.firmwareReady())check(p.currentPatch=="Named V sound","V-Station selection load");
 std::cout<<"PASS: single-selection load, all six sort columns both directions, selection preservation, X-Station filtering\n";

 const auto action=[&](const char* name,const juce::var& value={},const juce::String& key={}){
  juce::DynamicObject::Ptr message=new juce::DynamicObject;
  message->setProperty("action",name);message->setProperty("value",value);
  message->setProperty("path",key);message->setProperty("id",key);
  browser.webAction(juce::var(message.get()));
 };
 const auto wait=[&](const auto& predicate,const char* failure){
  for(int n=0;n<200;++n){pump();const auto snap=browser.webSnapshot();if(!bool(snap["loading"]) && predicate(snap))return snap;}
  throw std::runtime_error(failure);
 };
 action("facets",juce::Array<juce::var>{"Synth / KS","Instrument / Bass"});
 const auto ks=wait([](const juce::var& snap){return snap["rows"].size()==1 && snap["rows"][0]["model"].toString()=="KS";},"KS instrument facet did not match native category");
 action("tags","Bright Test",ks["rows"][0]["id"].toString());
 action("query","bright bass");
 wait([](const juce::var& snap){
  if(snap["rows"].size()!=1 || snap["rows"][0]["tags"].toString()!="Bright Test")return false;
  for(const auto& facet:*snap["facets"].getArray())if(facet.toString()=="Tag / Bright Test")return true;
  return false;
 },"Text search did not find the combined category and user tag");
 action("presetPathActive",false,dir.getFullPathName());
 wait([](const juce::var& snap){return snap["rows"].size()==0;},"Inactive folder remains in browser results");
 action("rescan");
 wait([](const juce::var& snap){return snap["status"].toString()=="Rescan complete" && snap["rows"].size()==0;},"Rescan reactivated a disabled path");
 action("presetPathActive",true,dir.getFullPathName());
 wait([](const juce::var& snap){return snap["rows"].size()==1 && snap["rows"][0]["tags"].toString()=="Bright Test";},"Reactivating path lost the preset or its tags");
 action("removePresetPath",{},dir.getFullPathName());
 wait([](const juce::var& snap){return snap["rows"].size()==0;},"Removed folder remains in browser results");
 check(dir.getChildFile("KS.syx").existsAsFile(),"Disconnecting a path removed its preset file");
 action("query","");action("clearFilters");
 std::cout<<"PASS: catalog facets, metadata and tag search, disabled path survives Rescan, reconnect and disconnect\n";

 const auto bankFile=dir.getSiblingFile("Browser slots.fxb");
 const auto bank=vStationBankFixture(true);
 check(bankFile.replaceWithData(bank.data(),bank.size()),"Write bank with default User slots");
 p.patchLibrary.addPath(bankFile);
 action("facets",juce::Array<juce::var>{"Synth / V-Station","Type / Bank program"});
 const auto rowCount=[](int count){return [count](const juce::var& snap){return snap["rows"].size()==count;};};
 wait(rowCount(1),"Default User slots should be filtered initially");
 action("allSlots",true);
 wait(rowCount(400),"Show all bank slots should include default User slots");
 action("allSlots",false);
 wait(rowCount(1),"Turning off Show all bank slots should hide the default slots again");
 action("removePresetPath",{},bankFile.getFullPathName());
 action("clearFilters");
 std::cout<<"PASS: Show all bank slots toggles 1 / 400 / 1 visible V-Station presets\n";
}
static void vStationPlayback(Processor& p,const juce::File& archive){
 DataList frames;check(p.patchLibrary.loadFile(frames,archive.getFullPathName().toStdString()) && frames.size()==400,"V-Station playback bank");
 p.patchLibrary.addPath(archive);Browser browser(p);browser.setBounds(0,0,1280,600);
 auto* all=dynamic_cast<juce::ToggleButton*>(find(browser,"showAllBankSlots"));check(all!=nullptr,"All bank slots control");all->setToggleState(true,juce::dontSendNotification);all->onClick();
 auto* source=dynamic_cast<juce::ComboBox*>(find(browser,"patchSource"));auto* table=dynamic_cast<juce::TableListBox*>(find(browser,"patchTable"));check(source && table,"V-Station browser controls");auto* model=table->getTableListBoxModel();
 bool selected=false;for(int attempt=0;attempt<400;++attempt){std::this_thread::sleep_for(std::chrono::milliseconds(20));browser.pollLibrary();if(!selected)for(int n=0;n<source->getNumItems();++n)if(source->getItemText(n)==archive.getFileName()){source->setSelectedId(source->getItemId(n),juce::dontSendNotification);source->onChange();selected=true;break;}if(selected && model->getNumRows()==400)break;}
 std::cout<<"V-Station browser model="<<(p.isKStation()?"K":"KS")<<" rows="<<model->getNumRows()<<" sources="<<source->getNumItems()<<" source="<<source->getText()<<" selected="<<selected<<" revision="<<p.patchLibrary.revision.load()<<std::endl;check(selected && model->getNumRows()==400,"V-Station browser lost bank slots");int bass=-1;for(int n=0;n<400;++n)if(model->getCellTooltip(n,1)=="BAS-Bittersweet")bass=n;check(bass>=0,"Named demo patch absent from browser");table->selectRow(bass);check(p.currentPatch=="BAS-Bittersweet","V-Station single-click load");
 p.setNonRealtime(true);juce::AudioBuffer<float> audio(4,256);juce::MidiBuffer midi;midi.ensureSize(4096);double peak=0;
 auto render=[&]{inAudio=true;p.processBlock(audio,midi);inAudio=false;check(audioAllocations==0,"Allocation while playing imported V-Station program");};
 for(unsigned n=0;n<16;++n){auto patch=p.patchLibrary.createPatch(Data(frames[n]),"Private V-Station bank");juce::String error;check(p.activatePatch(patch,error),"V-Station activation failed");for(int frame=0;frame<40;++frame)render();
  if(n==0){juce::MemoryBlock state;p.getStateInformation(state);auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));check(xml!=nullptr,"V-Station session state");juce::MemoryBlock payload;check(payload.fromBase64Encoding(xml->getStringAttribute(p.isKStation()?"kState":"program")),"Read native V-Station state");
   const auto* bytes=static_cast<const uint8_t*>(payload.getData());if(p.isKStation()){const size_t offset=8+0x40000-0x1fc00;check(payload.getSize()>offset+128,"Native K state length");for(const auto& control:kControls())if(control.patchOffset>=0)check((bytes[offset+control.patchOffset]&control.patchMask)==((control.patchOffset==125?uint8_t(std::max(0,int(patch->sysex[13+125])-6)):patch->sysex[13+control.patchOffset])&control.patchMask),"V-Station field changed by K firmware");}
   else check(payload.getSize()==270 && juce::String::fromUTF8(reinterpret_cast<const char*>(bytes+13+151),16).trim()=="BAS-Bittersweet","V-Station name not sent to KS firmware");
  }
  midi.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),0);double soundPeak=0;
  for(int frame=0;frame<188;++frame){render();for(int i=0;i<256;++i){const auto sample=std::abs(audio.getSample(0,i));check(std::isfinite(sample),"Nonfinite V-Station audio");soundPeak=std::max(soundPeak,double(sample));}}
  check(soundPeak>1e-5,"Imported V-Station demo patch is silent");peak=std::max(peak,soundPeak);midi.addEvent(juce::MidiMessage::noteOff(1,60),0);midi.addEvent(juce::MidiMessage::controllerEvent(1,120,0),0);for(int frame=0;frame<40;++frame)render();
 }
 std::cout<<"PASS: "<<(p.isKStation()?"K":"KS")<<" browser retains 400 V-Station slots, named click load, 16 demo sounds audible, no callback allocations; peak="<<peak<<'\n';
}
class PlayHead final:public juce::AudioPlayHead {
public:
 double bpm=120,ppq=0;bool playing=true;
 juce::Optional<LoopPoints> loop;
 juce::Optional<PositionInfo> getPosition()const override{
  PositionInfo p;
  p.setBpm(bpm);p.setPpqPosition(ppq);p.setIsPlaying(playing);
  p.setLoopPoints(loop);p.setIsLooping(loop.hasValue());
  return p;
 }
};

static std::vector<int> performanceAttacks(const juce::AudioBuffer<float>& audio,int channel,int start=0) {
 const auto peak=audio.getMagnitude(channel,0,audio.getNumSamples());
 std::vector<int> attacks;
 int quiet=512;
 for(int frame=start;frame<audio.getNumSamples();++frame){
  const auto value=audio.getSample(channel,frame);
  check(std::isfinite(value),"Non-finite Performance audio");
  if(std::abs(value)<=peak*.025f){++quiet;continue;}
  if(quiet>=512)attacks.push_back(frame);
  quiet=0;
 }
 return attacks;
}

static void heldPerformanceClockChecks(Processor& p,PlayHead& head) {
 constexpr int rate=48000,block=256,frames=512*block,warmup=128*block;
 const auto missingBefore=p.underruns();
 const auto allocationsBefore=audioAllocations;
 auto* tempo=p.uiParameter("arp_rate");
 check(tempo!=nullptr,"Performance internal tempo");
 tempo->setValueNotifyingHost(tempo->convertTo0to1(30.f));
 head.ppq=0;
 head.playing=false;
 juce::AudioBuffer<float> audio(4,block),capture(2,frames-warmup);
 juce::MidiBuffer midi;
 midi.ensureSize(4096);
 const auto render=[&]{
  audio.clear();
  inAudio=true;
  p.processBlock(audio,midi);
  inAudio=false;
 };
 for(int i=0;i<200;++i)render();
 struct Trial {ClockSource source;double hostBpm,midiBpm,bpm;const char* name;};
 const Trial trials[]{
  {ClockSource::Host,120,60,120,"host"},
  {ClockSource::Internal,180,60,94,"internal"},
  {ClockSource::Midi,180,90,90,"midi"},
  {ClockSource::Host,120,90,120,"host-after-midi"},
 };
 head.playing=true;
 for(const auto& trial:trials){
  p.setClockSource(trial.source);
  head.bpm=trial.hostBpm;
  HostClock incoming;
  for(int offset=0;offset<frames;offset+=block){
   incoming.begin({true,true,true,trial.midiBpm,offset*trial.midiBpm/(60*rate)},rate,block);
   HostClock::Event event{};
   while(incoming.next(event))midi.addEvent(&event.status,1,event.offset);
   if(&trial==trials && offset==1280)
    for(int channel:{1,2})midi.addEvent(juce::MidiMessage::noteOn(channel,60,uint8_t(100)),0);
   render();
   if(offset>=warmup){
    capture.copyFrom(0,offset-warmup,audio,0,0,block);
    capture.copyFrom(1,offset-warmup,audio,2,0,block);
   }
   head.ppq+=block*head.bpm/(60*rate);
  }
  const double expected=rate*60/(4*trial.bpm);
  std::cout<<"PERFORMANCE_CLOCK_SWITCH "<<trial.name<<" expected="<<expected;
  bool bothSound=true;
  for(int bus=0;bus<2;++bus){
   const auto attacks=performanceAttacks(capture,bus);
   std::vector<int> gaps;
   for(size_t i=1;i<attacks.size();++i)gaps.push_back(attacks[i]-attacks[i-1]);
   std::sort(gaps.begin(),gaps.end());
   const auto median=gaps.empty()?0:gaps[gaps.size()/2];
   bothSound&=gaps.size()>=6 && std::abs(median-expected)<=220;
   std::cout<<(bus==0?" drums=":" bass=")<<median;
  }
  std::cout<<'\n';
  if(!bothSound)std::cerr<<"KS status: "<<p.status()<<'\n';
  check(bothSound,"Both held Performance parts must follow the selected clock");
 }
 head.playing=false;
 for(int channel:{1,2})midi.addEvent(juce::MidiMessage::noteOff(channel,60),0);
 render();
 check(p.underruns()==missingBefore && audioAllocations==allocationsBefore,
       "Performance clock handovers lost audio or allocated in the callback");
}

static void performanceLoopChecks(Processor& p,PlayHead& head) {
 constexpr int rate=48000,block=256,passes=3,beats=16,steps=beats*4;
 juce::AudioBuffer<float> audio(4,block);
 juce::MidiBuffer midi;
 midi.ensureSize(4096);
 const auto render=[&]{
  audio.clear();
  inAudio=true;
  p.processBlock(audio,midi);
  inAudio=false;
 };
 // A pattern whose length does not divide the four bars exposes a wrong
 // step at the seam, even if a steady stream of sixteenths would sound fine.
 p.selectPart(0);
 const auto slot=p.drumEditorSlot();
 juce::String error;
 for(uint8_t step:{1,3,4}){
  const auto removed=p.editDrumGrid({Processor::DrumGridEdit::Kind::Remove,slot,0,step,0},error);
  check(removed,error.toRawUTF8());
 }
 head.playing=false;
 for(int i=0;i<80;++i)render();
 juce::MemoryBlock fixture;
 p.getStateInformation(fixture);
 check(fixture.getSize()>0,"Save the drum loop fixture");

 struct Trial {double bpm;bool loopTransport,replayNote;const char* name;};
 bool allAligned=true;
 for(const auto trial:{Trial{120,true,false,"held-block-edge"},Trial{128,true,false,"held-inside-block"},
                       Trial{128,true,true,"clip-note-inside-block"},
                       Trial{123.45,false,true,"midi-clip-fractional-tempo"}}) {
  const double quarter=rate*60/trial.bpm,stepSamples=quarter/4;
  const int loopSamples=juce::roundToInt(beats*quarter);
  const auto passStart=[&](int pass){return juce::roundToInt(pass*beats*quarter);};
  const int length=passStart(passes);
  std::cout<<"PERFORMANCE_DAW_LOOP_CASE "<<trial.name<<" bpm="<<trial.bpm<<" block="<<block
           <<" wrap_offset="<<loopSamples%block<<'\n';
  // Start each independent host scenario with the same sound and MIDI state.
  p.setStateInformation(fixture.getData(),int(fixture.getSize()));
  check(p.firmwareReady() && p.performanceMode(),"Restore the KS Performance loop fixture");
  head.ppq=8;
  head.bpm=trial.bpm;
  for(int i=0;i<80;++i)render();
  const auto missingBefore=p.underruns();
  const auto allocationsBefore=audioAllocations;
  juce::AudioBuffer<float> capture(2,length);
  if(trial.loopTransport)head.loop=juce::AudioPlayHead::LoopPoints{8,8+beats};
  head.playing=true;
  int nextTrigger=0,triggerPass=0;
  for(int offset=0;offset<length;offset+=block){
   head.ppq=8+double(trial.loopTransport?offset%loopSamples:offset)/quarter;
   if(nextTrigger<length && nextTrigger<offset+block){
    if(nextTrigger)midi.addEvent(juce::MidiMessage::noteOff(1,60),nextTrigger-offset);
    midi.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),nextTrigger-offset);
    nextTrigger=trial.replayNote?passStart(++triggerPass):length;
   }
   render();
   const int count=std::min(block,length-offset);
   capture.copyFrom(0,offset,audio,0,0,count);
   capture.copyFrom(1,offset,audio,2,0,count);
  }
  for(int bus=0;bus<(trial.loopTransport?1:2);++bus){
   const auto hits=performanceAttacks(capture,bus);
   auto hit=hits.begin();
   for(int pass=0;pass<passes;++pass) {
    int missing=0,extra=0,worst=0,first=-1;
    for(int step=0;step<steps;++step){
     if(bus==0 && step%5!=0 && step%5!=2)continue;
     const int expected=juce::roundToInt(pass*beats*quarter+step*stepSamples)+p.getLatencySamples();
     while(hit!=hits.end() && *hit<expected-220){++extra;++hit;}
     if(hit==hits.end() || *hit>expected+220){++missing;continue;}
     const int errorSamples=*hit++-expected;
     if(step==0)first=errorSamples;
     worst=std::max(worst,std::abs(errorSamples));
    }
    if(pass==passes-1)extra+=int(hits.end()-hit);
    std::cout<<"PERFORMANCE_DAW_LOOP "<<trial.name<<" part="<<bus+1<<" pass="<<pass+1
             <<" first_delta="<<first<<" max_delta="<<worst<<" missing="<<missing<<" extra="<<extra<<'\n';
    allAligned&=missing==0 && extra==0;
   }
  }
  head.playing=false;
  head.loop.reset();
  midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
  for(int i=0;i<80;++i)render();
  check(p.underruns()==missingBefore && audioAllocations==allocationsBefore,
        "DAW loop capture lost audio or allocated in the callback");
 }
 if(!allAligned)std::cerr<<"KS status: "<<p.status()<<'\n';
 check(allAligned,"Held notes and repeating MIDI clips must follow the grid across four-bar DAW loops");
}

static void performanceClockChecks(Processor& p) {
 constexpr int rate=48000,blockSize=256,quarter=24000,step=quarter/4;
 constexpr int length=256*192;
 juce::String error;
 auto buses=p.getBusesLayout();
 buses.outputBuses.set(1,juce::AudioChannelSet::stereo());
 check(p.setBusesLayout(buses),"Enable Aux output for separated Performance capture");
 p.prepareToPlay(rate,blockSize);
 p.setNonRealtime(true);
 p.setClockSource(ClockSource::Host);
 PlayHead head;
 head.playing=false;
 p.setPlayHead(&head);
 juce::AudioBuffer<float> audio(4,blockSize);
 juce::MidiBuffer midi;
 midi.ensureSize(4096);
 const auto render=[&](int blocks){
  for(int i=0;i<blocks;++i){
   audio.clear();
   inAudio=true;
   p.processBlock(audio,midi);
   inAudio=false;
  }
  p.refreshControls();
 };
 const auto set=[&](const char* id,int value){
  auto* parameter=p.uiParameter(id);
  check(parameter!=nullptr,id);
  parameter->setValueNotifyingHost(parameter->convertTo0to1(float(value)));
 };
 check(p.setPerformanceMode(true,error),error.toRawUTF8());
 render(180);
 const auto map=groupDrumMaps(drumFrames(7)).front();
 auto drum=p.patchLibrary.createPatch(Data(map.begin(),map.end()),{});
 check(p.activatePatchForPart(drum,0,error),error.toRawUTF8());
 render(220);
 const auto bytes=program(Model::KS);
 auto bass=p.patchLibrary.createPatch(Data(bytes.begin(),bytes.end()),{});
 check(p.activatePatchForPart(bass,1,error),error.toRawUTF8());
 render(180);
 for(unsigned part:{0u,1u}){
  p.selectPart(part);
  check(p.setPartActive(part,true,error),error.toRawUTF8());
  render(56);
  set("part_channel",0);
  set("part_output",int(part));
  set("volume",115);
  set("arp_keysync",0);
  set("arp_latch",0);
  if(part==0){
   check(p.drumEditInfo().bank==7,"Load the Drum Map into Part 1");
   const auto slot=p.drumEditorSlot();
   check(p.selectDrumRhythm(slot,Processor::UserDrumRhythm,error),error.toRawUTF8());
   using Edit=Processor::DrumGridEdit;
   check(p.editDrumGrid({Edit::Kind::Length,slot,0,0,5},error),error.toRawUTF8());
   for(uint8_t i=0;i<5;++i)
    check(p.editDrumGrid({Edit::Kind::Velocity,slot,0,i,100},error),error.toRawUTF8());
  }else{
   set("arp_on",1);
   set("arp_sync",3);
   set("arp_gate",30);
   set("arp_rhythm",0);
   set("amp_attack",0);
   set("amp_sustain",127);
   set("amp_release",0);
   set("delay_level",0);
   set("reverb_level",0);
   set("chorus_level",0);
  }
  render(100);
 }

 if(juce::SystemStats::getEnvironmentVariable("SILVER_TEST_DAW_LOOP_ONLY",{}).isNotEmpty()){
  performanceLoopChecks(p,head);
  p.setPlayHead(nullptr);
  return;
 }

 const auto missingBefore=p.underruns();
 const auto allocationsBefore=audioAllocations;
 juce::AudioBuffer<float> capture(2,length);
 bool aligned=true;
 struct Scenario {const char* name; double ppq; int trigger; bool stop; bool separate=false;};
 for(const auto scenario:{Scenario{"start-between-steps",8.,1280,false},
                          Scenario{"seek-with-note-held",20.09375,-1,true},
                          Scenario{"start-mid-beat",8.09375,1280,true},
                          Scenario{"note-on-downbeat",8.,0,true},
                          Scenario{"two-channels-on-downbeat",8.,0,true,true}}){
  if(scenario.separate){
   p.selectPart(1);
   set("part_channel",1);
   render(60);
  }
  head.ppq=scenario.ppq;
  head.playing=true;
  for(int offset=0;offset<length;offset+=blockSize){
   head.ppq=scenario.ppq+double(offset)/quarter;
   if(offset<=scenario.trigger && scenario.trigger<offset+blockSize){
    midi.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),scenario.trigger-offset);
    if(scenario.separate)
     midi.addEvent(juce::MidiMessage::noteOn(2,60,uint8_t(100)),scenario.trigger-offset);
   }
   audio.clear();
   inAudio=true;
   p.processBlock(audio,midi);
   inAudio=false;
   capture.copyFrom(0,offset,audio,0,0,blockSize);
   capture.copyFrom(1,offset,audio,2,0,blockSize);
  }
  const auto expected=int(std::ceil((scenario.ppq+double(std::max(0,scenario.trigger))/quarter)*4)*step-
                          scenario.ppq*quarter)+p.getLatencySamples();
  // Skip the outgoing tails at the seek, leaving the first new step in view.
  const int firstFrame=scenario.trigger<0?p.getLatencySamples()+512:0;
  const std::array hits{performanceAttacks(capture,0,firstFrame),performanceAttacks(capture,1,firstFrame)};
  std::cout<<"PERFORMANCE_CLOCK "<<scenario.name<<" ppq="<<scenario.ppq<<" expected="<<expected<<" drums=";
  for(int hit:hits[0])std::cout<<hit<<',';
  std::cout<<" bass=";
  for(int hit:hits[1])std::cout<<hit<<',';
  std::cout<<'\n';
  for(const auto& layer:hits){
   aligned&=layer.size()>=7;
   for(unsigned i=0;i<layer.size();++i)
    aligned&=std::abs(layer[i]-expected-int(i)*step)<=220;
  }
  for(size_t i=0;i<std::min(hits[0].size(),hits[1].size());++i)
   aligned&=std::abs(hits[0][i]-hits[1][i])<=220;
  if(scenario.stop){
   head.playing=false;
   midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
   if(scenario.separate)midi.addEvent(juce::MidiMessage::noteOff(2,60),0);
   render(70);
  }
 }
 check(p.underruns()==missingBefore && audioAllocations==allocationsBefore,
       "Performance clock capture underruns or allocated on the audio callback");
 check(aligned,"SEQ and Part 2 arpeggiator drifted off the shared DAW grid");
 heldPerformanceClockChecks(p,head);
 p.selectPart(1);
 set("part_channel",0);
 render(60);
 performanceLoopChecks(p,head);
 p.setPlayHead(nullptr);
}
static void levelProbe(Processor& p,const juce::File& file){
 DataList frames;check(p.patchLibrary.loadFile(frames,file.getFullPathName().toStdString()) && frames.size()==1,"Level probe import");auto original=p.patchLibrary.createPatch(Data(frames[0]),{});PlayHead head;p.setPlayHead(&head);p.setNonRealtime(true);juce::AudioBuffer<float> audio(4,256);juce::MidiBuffer midi;midi.ensureSize(4096);
 auto render=[&]{p.processBlock(audio,midi);head.ppq+=256*head.bpm/(60*48000);};
 for(int trial=0;trial<8;++trial){auto patch=p.patchLibrary.createPatch(Data(frames[0]),{});if(!juce::SystemStats::getEnvironmentVariable("SILVER_TEST_RAW_LEVEL",{}).isEmpty()){patch->tags.erase(TagType::CustomA,"V-Station");patch->tags.erase(TagType::Tag,"V-Station");patch->tags.add(TagType::CustomA,"K-Station");}const char* control=trial==2?"unison":trial==3?"filter_drive":trial==4?"eq_level":trial==6?"distortion_compensation":nullptr;
  if(trial==7)patch->sysex[13+125]=58;
  if(control)for(const auto& c:kControls())if(juce::String(c.id)==control)patch->sysex[13+c.patchOffset]=uint8_t((patch->sysex[13+c.patchOffset]&~c.patchMask)|((c.defaultValue<<c.patchShift)&c.patchMask));
  midi.addEvent(juce::MidiMessage::controllerEvent(1,120,0),0);juce::String error;check(p.activatePatch(patch,error),"Level probe activate");auto* volume=p.uiParameter("volume");volume->setValueNotifyingHost(volume->convertTo0to1(127));for(int n=0;n<940;++n)render();
  if(trial==5){auto* param=p.uiParameter("volume");param->setValueNotifyingHost(param->convertTo0to1(64));for(int n=0;n<40;++n)render();}
  const std::vector<int> notes=trial==0?std::vector<int>{60}:std::vector<int>{48,55,60,67};for(int note:notes)midi.addEvent(juce::MidiMessage::noteOn(1,note,uint8_t(127)),0);
  double peak=0,square=0;unsigned over=0,near=0,samples=0;for(int n=0;n<375;++n){render();for(int c=0;c<2;++c)for(int i=0;i<256;++i){auto v=std::abs(audio.getSample(c,i));check(std::isfinite(v),"Level probe finite");peak=std::max(peak,double(v));square+=v*v;++samples;if(v>1)++over;if(v>.999)++near;}}
  std::cout<<"LEVEL model="<<(p.isKStation()?"K":"KS")<<" trial="<<trial<<" name="<<patchName(*original)<<" peak="<<peak<<" rms="<<std::sqrt(square/samples)<<" over="<<over<<" near="<<near<<'\n';for(int note:notes)midi.addEvent(juce::MidiMessage::noteOff(1,note),0);for(int n=0;n<200;++n)render();
 }
 p.setPlayHead(nullptr);
}
static void vStationFolderPlayback(Processor& p,const juce::File& folder){
 const auto files=folder.findChildFiles(juce::File::findFiles,true,"*.fxp");check(!files.isEmpty(),"Private FXP playback folder");p.patchLibrary.addPath(folder);Browser browser(p);browser.setBounds(0,0,1280,600);
 auto* source=dynamic_cast<juce::ComboBox*>(find(browser,"patchSource"));auto* filter=dynamic_cast<juce::ComboBox*>(find(browser,"patchModel"));auto* table=dynamic_cast<juce::TableListBox*>(find(browser,"patchTable"));check(source && filter && table,"FXP browser controls");auto* rows=table->getTableListBoxModel();bool selected=false;
 filter->setSelectedId(6,juce::dontSendNotification);filter->onChange();
 for(int n=0;n<600;++n){browser.pollLibrary();if(!selected)for(int i=0;i<source->getNumItems();++i)if(source->getItemText(i)==folder.getFileName()){source->setSelectedId(source->getItemId(i),juce::dontSendNotification);source->onChange();selected=true;break;}if(selected && rows->getNumRows()==files.size())break;std::this_thread::sleep_for(std::chrono::milliseconds(20));}
 check(selected && rows->getNumRows()==files.size(),"Nested FXP folder scan/filter lost presets");for(int row=0;row<rows->getNumRows();++row)check(rows->getCellTooltip(row,2)=="V-Station","FXP displayed as another synth");table->selectRow(0);check(p.currentPatch==rows->getCellTooltip(0,1),"Named FXP click load");
 auto checkBanks=[&]{for(int n=0;n<source->getNumItems();++n)check(!source->getItemText(n).endsWithIgnoreCase(".fxp"),"Individual FXP leaked into bank selector");check(source->getNumItems()<10,"Bank selector flooded by individual presets");};checkBanks();
 std::vector<DataSourceNodePtr> sources;p.patchLibrary.getDataSources(sources);for(const auto& ds:sources)if(ds->name==folder.getFullPathName().toStdString() && ds->type==SourceType::Folder){p.patchLibrary.refreshDataSource(ds);break;}
 for(int n=0;n<300;++n){browser.pollLibrary();std::this_thread::sleep_for(std::chrono::milliseconds(10));}checkBanks();check(source->getText()==folder.getFileName() && rows->getNumRows()==files.size(),"Rescan lost selected bank or presets");
 PlayHead head;p.setPlayHead(&head);p.setNonRealtime(true);juce::AudioBuffer<float> audio(4,256);juce::MidiBuffer midi;midi.ensureSize(4096);std::set<juce::String> categories;double peak=0;
 auto render=[&]{inAudio=true;p.processBlock(audio,midi);inAudio=false;check(audioAllocations==0,"FXP playback allocated in callback");head.ppq+=256*head.bpm/(60*48000);for(int c=0;c<2;++c)for(int i=0;i<256;++i){const auto value=audio.getSample(c,i);check(std::isfinite(value),"Non-finite FXP playback");peak=std::max(peak,double(std::abs(value)));}};
 for(const auto& file:files){DataList frames;check(p.patchLibrary.loadFile(frames,file.getFullPathName().toStdString()),"FXP playback parse");auto patch=p.patchLibrary.createPatch(Data(frames[0]),{});const auto category=juce::String(patch->name).upToFirstOccurrenceOf(" ",false,false);if(!categories.insert(category).second)continue;juce::String error;check(p.activatePatch(patch,error),"FXP activate");for(int n=0;n<40;++n)render();midi.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),0);for(int n=0;n<188;++n)render();midi.addEvent(juce::MidiMessage::noteOff(1,60),0);midi.addEvent(juce::MidiMessage::controllerEvent(1,120,0),0);for(int n=0;n<40;++n)render();}
 p.setPlayHead(nullptr);check(categories.size()>=6 && peak>1e-4,"FXP category sound coverage");std::cout<<"PASS: "<<(p.isKStation()?"K":"KS")<<" recursive FXP rows="<<rows->getNumRows()<<", V-Station filter/click, categories played="<<categories.size()<<", finite sound/no allocations, peak="<<peak<<'\n';
}
// Optional private-ROM regression for filter-dependent arp phase. Measure every
// attack against the sample grid: mean spacing alone hides accumulated drift.
static void filterClock(Processor& p,const juce::File& dir){
 PlayHead head;head.bpm=100;p.setPlayHead(&head);p.setNonRealtime(true);
 const juce::ScopeGuard clearHead{[&]{p.setPlayHead(nullptr);}};
 constexpr int rate=48000,block=256,steps=12,period=28800;
 juce::AudioBuffer<float> audio(4,block);juce::MidiBuffer midi;midi.ensureSize(4096);
 auto set=[&](const char* id,int value){auto* parameter=p.uiParameter(id);check(parameter!=nullptr,id);parameter->setValueNotifyingHost(parameter->convertTo0to1(float(value)));};
 auto render=[&]{inAudio=true;p.processBlock(audio,midi);inAudio=false;check(audioAllocations==0,"Filter/clock audio callback allocated");head.ppq+=block*head.bpm/(60*rate);};
 // A known dry program avoids threshold movement from tails, modulation or
 // changing pulse width. Keep incoming host notes identical in each case.
 auto message=program(p.isKStation()?Model::KStation:Model::KS);
 auto patch=p.patchLibrary.createPatch(Data(message.begin(),message.end()),{});juce::String error;
 check(p.activatePatch(patch,error),error.toRawUTF8());for(int n=0;n<100;++n)render();
 for(auto item:{std::pair<const char*,int>{"arp_on",1},{"arp_sync",9},{"arp_gate",32},{"arp_keysync",1},{"arp_latch",0},{"arp_rate",36},{"amp_release",0},{"amp_attack",0},{"amp_sustain",127},{"delay_level",0},{"reverb_level",0},{"chorus_level",0},{"volume",127},{"osc1_wave",2},{"osc1_level",127},{"osc2_level",0},{"osc3_level",0},{"noise_level",0},{"ring_level",0},{"filter_env",64},{"filter_keytrack",0},{"resonance",0}})set(item.first,item.second);
 bool passed=true;
 for(auto source:{ClockSource::Internal,ClockSource::Host}){
  p.setClockSource(source);head.ppq=0;for(int n=0;n<1200;++n)render();
  for(int key:{36,60,84}){
   std::vector<int> reference;
   for(int variant=0;variant<3;++variant){
    const int cutoff=variant==1?127:64;const bool sweep=variant==2;set("cutoff",cutoff);
    const int channel=1+p.midiChannel();midi.addEvent(juce::MidiMessage::controllerEvent(channel,120,0),0);
    for(int n=0;n<100;++n)render();
    const auto snapshot=dir.getChildFile("filter-clock-current.syx");check(p.saveCurrentPatch(snapshot,error),error.toRawUTF8());juce::MemoryBlock saved;check(snapshot.loadFileAsData(saved) && saved.getSize()>59 && static_cast<const uint8_t*>(saved.getData())[59]==cutoff,"Cutoff did not reach the engine snapshot");
    const auto missedBefore=p.underruns();
    std::vector<float> samples(steps*period);midi.addEvent(juce::MidiMessage::noteOn(channel,key,uint8_t(100)),0);
    for(int n=0;n<int(samples.size());n+=block){
     if(sweep)set("cutoff",64+(n/block)%64);
     render();std::copy_n(audio.getReadPointer(0),std::min(block,int(samples.size())-n),samples.begin()+n);
    }
    check(p.underruns()==missedBefore,"Missing audio frames invalidate filter/clock measurement");
    midi.addEvent(juce::MidiMessage::noteOff(channel,key),0);midi.addEvent(juce::MidiMessage::controllerEvent(channel,120,0),0);for(int n=0;n<100;++n)render();
    std::vector<int> attacks;
    // Search each expected beat after a long silent gate. The threshold is
    // relative to that beat's own peak, with a fixed -120 dBFS noise floor.
    // This records audio onset, not a claim about the controller's note time.
    for(int beat=1;beat<steps;++beat){
     const int begin=beat*period-rate/5,end=beat*period+rate/5;
     float peak=0;for(int n=begin;n<end;++n)peak=std::max(peak,std::abs(samples[n]));check(peak>1e-5f,"Filter/clock arp silent");
     const float threshold=std::max(1e-6f,peak*.001f);int onset=begin;while(onset<end && std::abs(samples[onset])<=threshold)++onset;
     check(onset>begin && onset<end,"Arp onset missing or outside the grid window");attacks.push_back(onset);
    }
    double minPhase=1e9,maxPhase=-1e9,maxReference=0;
    for(size_t n=0;n<attacks.size();++n){const double phase=attacks[n]-(n+1)*period;minPhase=std::min(minPhase,phase);maxPhase=std::max(maxPhase,phase);if(!reference.empty())maxReference=std::max(maxReference,double(std::abs(attacks[n]-reference[n])));}
    if(variant==0)reference=attacks;
    const double drift=attacks.back()-attacks.front()-(steps-2)*period;
    const bool ok=std::abs(drift)<=rate*.002 && maxPhase-minPhase<=rate*.004 && maxReference<=rate*.004;
    passed=passed && ok;
    std::cout<<"FILTER_CLOCK model="<<(p.isKStation()?"K":"KS")<<" source="<<(source==ClockSource::Internal?"internal":"host")<<" key="<<key<<" cutoff="<<cutoff<<" sweep="<<sweep<<" drift_ms="<<1000*drift/rate<<" phase_span_ms="<<1000*(maxPhase-minPhase)/rate<<" vs_cutoff64_ms="<<1000*maxReference/rate<<" result="<<(ok?"PASS":"FAIL")<<'\n'<<std::flush;
    const auto name=juce::String(p.isKStation()?"K":"KS")+"-filter-"+(source==ClockSource::Internal?"internal":"host")+"-"+juce::String(key)+"-"+juce::String(cutoff)+(sweep?"-sweep":"")+".wav";
    auto stream=dir.getChildFile(name).createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),rate,1,24,{},0));const float* channels[]{samples.data()};check(writer && writer->writeFromFloatArrays(channels,1,int(samples.size())),"Filter/clock audio capture");
   }
  }
 }
 set("arp_on",0);check(passed,"Filter/clock phase regression; inspect FILTER_CLOCK measurements");
}
// Long external-preset probe for comparison with a tester's DAW captures.
// Preserve the patch's envelopes, oscillator phase, arp division and keysync.
static void filterPatchClock(Processor& p,const juce::File& file,const juce::File& dir){
 const bool ccFlood=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FILTER_CC_FLOOD",{}).isNotEmpty();
 const int key=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FILTER_NOTE","24").getIntValue();check(key>=0 && key<=127,"Filter probe MIDI note outside 0..127");
 const auto source=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FILTER_SOURCE","internal");check(source=="host" || source=="internal","Filter probe clock must be host or internal");
 const int selectedCase=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FILTER_CASE","-1").getIntValue();check(selectedCase>=-1 && selectedCase<8,"Filter probe case outside 0..7");
 const bool realtime=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FILTER_REALTIME",{}).isNotEmpty();
 PlayHead head;head.bpm=100;p.setPlayHead(&head);p.setNonRealtime(true);const juce::ScopeGuard clearHead{[&]{p.setPlayHead(nullptr);p.setNonRealtime(true);}};
 DataList frames;check(p.patchLibrary.loadFile(frames,file.getFullPathName().toStdString()) && frames.size()==1,"Filter probe requires one program dump");
 auto patch=p.patchLibrary.createPatch(Data(frames[0]),{});check(patch!=nullptr,"Invalid filter probe patch");juce::String error;
 constexpr int rate=48000,block=256,half=921600,length=1958400; // 8 and 17 bars, 100 BPM, 4/4.
 juce::AudioBuffer<float> audio(4,block);juce::MidiBuffer midi;midi.ensureSize(4096);
 auto set=[&](const char* id,int value){auto* q=p.uiParameter(id);check(q!=nullptr,id);q->setValueNotifyingHost(q->convertTo0to1(float(value)));};
 auto render=[&]{inAudio=true;p.processBlock(audio,midi);inAudio=false;head.ppq+=block/28800.;};
 p.setClockSource(source=="host"?ClockSource::Host:ClockSource::Internal);
 bool passed=true;
 for(bool arp:{false,true})for(int variant=0;variant<4;++variant){
  if(selectedCase>=0 && selectedCase!=(arp?4:0)+variant)continue;
  check(p.activatePatch(patch,error),error.toRawUTF8());for(int n=0;n<200;++n)render();
  set("arp_rate",36);set("arp_on",arp?1:0);set("cutoff",variant==0?64:127);set("filter_keytrack",variant<2?0:127);
  for(int n=0;n<1200;++n)render();
  const auto name=juce::String(p.isKStation()?"K":"KS")+"-tester-"+source+"-"+juce::String(key)+(arp?"-arp":"-held")+"-v"+juce::String(variant)+(realtime?"-realtime":"-offline");
  check(p.saveCurrentPatch(dir.getChildFile(name+"-before.syx"),error),error.toRawUTF8());
  const auto misses=p.underruns();const auto allocationBefore=audioAllocations;const int channel=1+p.midiChannel();
  struct MissingBlock{int offset;uint64_t count;int64_t finishedLateUs;};std::vector<MissingBlock> dropoutBlocks;dropoutBlocks.reserve(length/block);uint64_t previousMissing=misses;
  juce::AudioBuffer<float> capture(2,length);head.ppq=0;midi.addEvent(juce::MidiMessage::noteOn(channel,key,uint8_t(100)),0);
  p.setNonRealtime(!realtime);const auto start=std::chrono::steady_clock::now();
  for(int n=0;n<length;n+=block){
   if(variant==3 && n>=half){const double phase=std::min(1.,double(n-half)/half);set("cutoff",int(std::round(127*std::abs(2*phase-1))));}
   if(ccFlood){const int value=variant==0?64:variant==3 && n>=half?int(std::round(127*std::abs(2*std::min(1.,double(n-half)/half)-1))):127;midi.addEvent(juce::MidiMessage::controllerEvent(channel,105,value),0);}
   render();const auto currentMissing=p.underruns();if(currentMissing>previousMissing){const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();dropoutBlocks.push_back({n,currentMissing-previousMissing,elapsed-int64_t(n+block)*1000000/rate});}previousMissing=currentMissing;
   for(int c=0;c<2;++c)capture.copyFrom(c,n,audio,c,0,block);
   if(realtime)std::this_thread::sleep_until(start+std::chrono::microseconds(int64_t(n+block)*1000000/rate));
  }
  p.setNonRealtime(true);const auto missing=p.underruns()-misses;const auto allocated=audioAllocations-allocationBefore;
  midi.addEvent(juce::MidiMessage::noteOff(channel,key),0);midi.addEvent(juce::MidiMessage::controllerEvent(channel,120,0),0);for(int n=0;n<200;++n)render();
  check(p.saveCurrentPatch(dir.getChildFile(name+"-after.syx"),error),error.toRawUTF8());
  const float peak=capture.getMagnitude(0,0,length);bool finite=true;
  // Right contains the sound plus a quarter-note click; left is untouched.
  for(int n=0;n<length;++n){finite=finite && std::isfinite(capture.getSample(0,n)) && std::isfinite(capture.getSample(1,n));const int tick=n%28800;if(tick<480)capture.addSample(1,n,float(.2*std::exp(-tick/80.)*std::sin(tick*2*juce::MathConstants<double>::pi*3000/rate)));}
  auto stream=dir.getChildFile(name+".wav").createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),rate,2,24,{},0));check(writer && writer->writeFromAudioSampleBuffer(capture,0,length),"Tester filter capture");
  juce::XmlElement meta("FilterClockProbe");meta.setAttribute("operatingSystem",juce::SystemStats::getOperatingSystemName());meta.setAttribute("sourceFile",file.getFullPathName());meta.setAttribute("sourceModel",modelName(modelOf(patch->sysex)));meta.setAttribute("model",p.isKStation()?"K":"KS");meta.setAttribute("clockSource",source);meta.setAttribute("ksControllerMode",p.ksControllerMode());meta.setAttribute("reportedLatencySamples",p.getLatencySamples());meta.setAttribute("repeatedCutoffCC",ccFlood);meta.setAttribute("note",key);meta.setAttribute("arpOn",arp);meta.setAttribute("cutoff",variant==0?64:127);meta.setAttribute("keytrack",variant<2?0:127);meta.setAttribute("sweepAfterEightBars",variant==3);meta.setAttribute("bpm",100);meta.setAttribute("sampleRate",rate);meta.setAttribute("blockSize",block);meta.setAttribute("samples",length);meta.setAttribute("realtime",realtime);meta.setAttribute("missingFrames",juce::String(missing));meta.setAttribute("callbackAllocations",juce::String(allocated));meta.setAttribute("peak",double(peak));
  for(const auto& lost:dropoutBlocks){auto* entry=meta.createNewChildElement("MissingBlock");entry->setAttribute("sampleOffset",lost.offset);entry->setAttribute("count",juce::String(lost.count));entry->setAttribute("callbackFinishedLateUs",juce::String(lost.finishedLateUs));}check(meta.writeTo(dir.getChildFile(name+".xml")),"Tester dropout metadata");
  const bool ok=missing==0 && allocated==0 && finite && peak>1e-6f;passed=passed && ok;
  std::cout<<"FILTER_PATCH "<<name<<" missing="<<missing<<" callback_allocations="<<allocated<<" peak="<<peak<<" result="<<(ok?"PASS":"FAIL")<<'\n'<<std::flush;
 }
 check(passed,"Tester filter probe lost frames, allocated, or produced invalid/silent audio; inspect captures");
}
static void arpStress(Processor& p,const juce::File& dir,int block){
 const auto sourceName=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FILTER_SOURCE","internal");
 check(sourceName=="internal" || sourceName=="host" || sourceName=="midi","Arp stress clock must be internal, host or midi");
 const auto source=sourceName=="host"?ClockSource::Host:sourceName=="midi"?ClockSource::Midi:ClockSource::Internal;
 const bool realtime=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FILTER_REALTIME",{}).isNotEmpty();
 const auto selectedTraffic=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_ARP_TRAFFIC",{});
 constexpr std::array trafficCases{"quiet","repeat","cutoff-cc","cutoff-parameter","mixed-cc","mixed-parameter","keytrack-parameter"};
 check(selectedTraffic.isEmpty() || std::any_of(trafficCases.begin(),trafficCases.end(),
       [&](const char* name){return selectedTraffic==name;}),"Unknown arp stress traffic pattern");
 const int controlBlocks=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_ARP_CONTROL_BLOCKS","1").getIntValue();
 check(controlBlocks>0 && controlBlocks<=64,"Arp stress control cadence must be 1..64 blocks");
 const bool stagger=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_ARP_STAGGER",{})=="1";
 check(!stagger || controlBlocks>=4,"Staggered arp stress needs at least four control blocks");
 constexpr int rate=48000, period=7200, length=48*period, quiet=rate/50;
 PlayHead head;head.bpm=100;p.setPlayHead(&head);p.setNonRealtime(true);p.setClockSource(source);
 const juce::ScopeGuard clearHead{[&]{p.setPlayHead(nullptr);p.setNonRealtime(true);}};
 HostClock incoming;
 juce::AudioBuffer<float> audio(4,block);juce::MidiBuffer midi;midi.ensureSize(4096);
 auto set=[&](const char* id,int value){
  auto* parameter=p.uiParameter(id);check(parameter!=nullptr,id);
  parameter->setValueNotifyingHost(parameter->convertTo0to1(float(value)));
 };
 auto render=[&]{
  if(source==ClockSource::Midi){
   incoming.begin({true,true,true,head.bpm,head.ppq},rate,block);
   HostClock::Event tick{};while(incoming.next(tick))midi.addEvent(&tick.status,1,tick.offset);
  }
  inAudio=true;p.processBlock(audio,midi);inAudio=false;
  head.ppq+=block/28800.;
 };
 const auto message=program(p.isKStation()?Model::KStation:Model::KS);
 auto patch=p.patchLibrary.createPatch(Data(message.begin(),message.end()),{});juce::String error;
 check(p.activatePatch(patch,error),error.toRawUTF8());
 for(int n=0;n<100;++n)render();
 for(auto item:{std::pair<const char*,int>{"arp_on",1},{"arp_sync",3},{"arp_gate",24},
      {"arp_keysync",1},{"arp_latch",0},{"arp_rate",36},{"amp_release",0},{"amp_attack",0},
      {"amp_sustain",127},{"delay_level",0},{"reverb_level",0},{"chorus_level",0},{"volume",127},
      {"osc1_wave",2},{"osc1_level",127},{"osc2_level",0},{"osc3_level",0},{"noise_level",0},
      {"ring_level",0},{"filter_env",64},{"filter_keytrack",0},{"resonance",0},{"cutoff",112}})
  set(item.first,item.second);
 const int channel=1+p.midiChannel();
 auto cc=[&](int number,int value,int offset){midi.addEvent(juce::MidiMessage::controllerEvent(channel,number,value),offset);};
 // Send setup edits in one block in either arm. Measure the guard only after
 // the dry patch, phase and clock have settled.
 cc(120,0,0);
 for(int n=0;n<750;++n)render();
 const auto backend=juce::JSON::parse(p.performanceReport()).getProperty("controller_backend",{}).toString();
 check(backend.isNotEmpty() && backend!="not-applicable","Arp stress needs the active controller identity");
 std::vector<int> reference;
 bool healthy=true;
 for(const auto* name:trafficCases){
  const juce::String traffic(name);
  if(selectedTraffic.isEmpty() && traffic=="keytrack-parameter")continue;
  if(selectedTraffic.isNotEmpty() && traffic!="quiet" && traffic!=selectedTraffic)continue;
  set("cutoff",112);set("resonance",0);set("lfo1_rate",68);set("filter_keytrack",0);
  cc(120,0,0);
  for(int n=0;n<100;++n)render();
  check(p.setPerformanceDiagnostics(1),"Could not begin arp stress diagnostics");
  const auto missingBefore=p.underruns();
  const auto allocBefore=audioAllocations;
  std::vector<int> attacks;
  attacks.reserve(64);
  midi.addEvent(juce::MidiMessage::noteOn(channel,60,uint8_t(100)),0);
  p.setNonRealtime(!realtime);const auto start=std::chrono::steady_clock::now();
  int below=quiet;
  bool finite=true;
  uint64_t audioHash=14695981039346656037ull;
  int lastCutoff=-1,lastResonance=-1,lastRate=-1,lastKeytrack=-1;
  for(int n=0;n<length;n+=block){
   const int i=n/block;
   const int phase=i%controlBlocks;
   const int cutoff=96+i%32;
   if(phase==0){
    if(traffic=="repeat")cc(105,112,block/4);
    if(traffic=="cutoff-cc" || traffic=="mixed-cc")cc(105,cutoff,block/4);
    if(traffic=="cutoff-parameter" || traffic=="mixed-parameter"){lastCutoff=cutoff;set("cutoff",lastCutoff);}
    if(traffic=="keytrack-parameter"){lastKeytrack=i%32;set("filter_keytrack",lastKeytrack);}
   }
   if(traffic=="mixed-cc" || traffic=="mixed-parameter"){
    if(phase==(stagger?1:0)){
     if(traffic=="mixed-cc")cc(106,i%32,block*3/8);else{lastResonance=i%32;set("resonance",lastResonance);}
    }
    if(phase==(stagger?2:0)){
     if(traffic=="mixed-cc")cc(80,64+i%32,block/2);else{lastRate=64+i%32;set("lfo1_rate",lastRate);}
    }
    if(traffic=="mixed-parameter" && phase==(stagger?3:0)){lastKeytrack=i%32;set("filter_keytrack",lastKeytrack);}
   }
   render();
   const auto* signal=audio.getReadPointer(0);
   for(int sample=0;sample<block;++sample){
    const float value=signal[sample];finite=finite && std::isfinite(value);
    uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));audioHash=(audioHash^bits)*1099511628211ull;
    if(std::abs(value)<=0.0001f){++below;continue;}
    if(below>=quiet)attacks.push_back(n+sample);
    below=0;
   }
   if(realtime)std::this_thread::sleep_until(start+std::chrono::microseconds(int64_t(n+block)*1000000/rate));
  }
  const auto missing=p.underruns()-missingBefore;
  const auto allocated=audioAllocations-allocBefore;
  p.setPerformanceDiagnostics(0);
  const auto report=p.performanceReport();
  const auto metrics=juce::JSON::parse(report);
  const auto overflows=int64_t(metrics.getProperty("midi_overflows",0))
                      -int64_t(metrics.getProperty("session",{}).getProperty("midi_overflows_at_start",0));
  const auto stem=(p.isKStation()?"K-":"KS-")+sourceName+"-"+traffic;
  check(dir.getChildFile(stem+".json").replaceWithText(report),
        "Arp stress diagnostics output");
  p.setNonRealtime(true);midi.addEvent(juce::MidiMessage::noteOff(channel,60),0);for(int n=0;n<100;++n)render();
  int finalValues=-1;
  if(lastCutoff>=0 || lastKeytrack>=0){
   const auto file=dir.getChildFile(stem+"-after.syx");
   check(p.saveCurrentPatch(file,error),error.toRawUTF8());
   juce::MemoryBlock snapshot;
   check(file.loadFileAsData(snapshot) && snapshot.getSize()>13+72,"Read final arp stress patch");
   const auto* programBytes=static_cast<const uint8_t*>(snapshot.getData())+13;
   finalValues=1;
   for(const auto& [offset,wanted]:{std::pair<int,int>{46,lastCutoff},{44,lastResonance},{72,lastRate},{47,lastKeytrack}})
    if(wanted>=0 && programBytes[offset]!=wanted)finalValues=0;
  }
  int minInterval=length,maxInterval=0,maxDelta=0,drift=0;
  for(size_t n=1;n<attacks.size();++n){
   const int interval=attacks[n]-attacks[n-1];minInterval=std::min(minInterval,interval);maxInterval=std::max(maxInterval,interval);
   if(n<reference.size())maxDelta=std::max(maxDelta,std::abs(interval-(reference[n]-reference[n-1])));
  }
  if(attacks.size()>1)drift=attacks.back()-attacks.front()-int(attacks.size()-1)*period;
  if(traffic=="quiet")reference=attacks;
  const bool valid=finite && !missing && !allocated && !overflows;
  const bool grid=attacks.size()>=47 && attacks.size()<=49 && minInterval>period/2 && maxInterval<period*3/2;
  healthy=healthy && valid;
  std::cout<<"ARP_STRESS backend="<<backend<<" clock="<<sourceName<<" traffic="<<traffic
    <<" realtime="<<realtime<<" block="<<block<<" control_blocks="<<controlBlocks<<" stagger="<<stagger
    <<" attacks="<<attacks.size()<<" interval_min="<<(attacks.size()>1?minInterval:0)
    <<" interval_max="<<maxInterval<<" vs_quiet_interval_delta="<<maxDelta<<" drift="<<drift
    <<" missing_frames="<<missing<<" midi_overflows="<<overflows<<" callback_allocations="<<allocated
    <<" valid="<<valid<<" grid="<<grid<<" final_values="<<finalValues<<" audio_hash="<<audioHash<<" frames=";
  for(int attack:attacks)std::cout<<attack<<',';
  std::cout<<'\n';
 }
 check(healthy,"Arp stress capture lost frames/MIDI, allocated, or produced non-finite output");
}
static void audioClock(Processor& p,const juce::File& dir){
 PlayHead head;p.setPlayHead(&head);p.setNonRealtime(true);juce::AudioBuffer<float> audio(4,256);juce::MidiBuffer midi;midi.ensureSize(4096);
 HostClock incoming;double incomingBpm=0;int64_t incomingSamples=0;bool noteHeld=false;
 auto set=[&](const char* id,int value){auto* parameter=p.uiParameter(id);check(parameter!=nullptr,id);parameter->setValueNotifyingHost(parameter->convertTo0to1(float(value)));};
 for(auto item:{std::pair<const char*,int>{"arp_on",1},{"arp_sync",9},{"arp_gate",32},{"arp_keysync",1},{"arp_latch",0},{"arp_rate",30},{"amp_release",0},{"amp_attack",0},{"amp_sustain",127},{"delay_level",0},{"reverb_level",0},{"chorus_level",0},{"volume",127}})set(item.first,item.second);
 auto render=[&]{
  if(incomingBpm>0){incoming.begin({true,true,true,incomingBpm,incomingSamples*incomingBpm/(60*48000)},48000,256);HostClock::Event event{};while(incoming.next(event))midi.addEvent(&event.status,1,event.offset);incomingSamples+=256;}
  inAudio=true;p.processBlock(audio,midi);inAudio=false;check(audioAllocations==0,"Heap allocation while processing clock");head.ppq+=256*head.bpm/(60*48000);
 };
 for(int n=0;n<250;++n)render();
 struct Scenario{ClockSource source;double hostBpm,midiBpm,tempo;const char* name;};
 for(const auto scenario:{Scenario{ClockSource::Host,90,0,90,"host-90"},
                         {ClockSource::Host,180,0,180,"host-180"},
                         {ClockSource::Internal,180,60,94,"internal-host180-midi60"},
                         {ClockSource::Internal,90,180,94,"internal-host90-midi180"},
                         {ClockSource::Midi,180,90,90,"midi90-host180"},
                         {ClockSource::Midi,90,180,180,"midi180-host90"},
                         {ClockSource::Host,90,180,90,"host90-after-midi"}}){
  const double tempo=scenario.tempo;p.setClockSource(scenario.source);incomingBpm=scenario.midiBpm;incoming.reset();incomingSamples=0;
  head.bpm=scenario.hostBpm;head.ppq=0;for(int n=0;n<2400;++n)render();if(!noteHeld){midi.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),0);noteHeld=true;}
  const int blocks=750;std::vector<float> envelope;envelope.reserve(blocks);juce::AudioBuffer<float> capture(2,blocks*256);
  for(int n=0;n<blocks;++n){if(n%2==0)set("osc1_pulse_width",(n*7)%100+14);if(n==375){juce::MemoryBlock saved;p.getStateInformation(saved);check(saved.getSize()>0,"Live arp state read");}render();envelope.push_back(audio.getRMSLevel(0,0,256));capture.copyFrom(0,n*256,audio,0,0,256);capture.copyFrom(1,n*256,audio,1,0,256);}
  const auto peak=*std::max_element(envelope.begin(),envelope.end());check(peak>1e-5,"Clock arp silent");std::vector<int> attacks;
  // A held note can already be sounding at the measurement window's boundary;
  // that boundary is not a new attack.
  bool sounding=envelope[189]>peak*.12;
  for(int n=190;n<blocks;++n){const bool on=envelope[n]>peak*.12;if(on && !sounding)attacks.push_back(n*256);sounding=on;}
  std::cout<<(p.isKStation()?"K":"KS")<<" clock="<<scenario.name<<" expected="<<tempo<<" attacks="<<attacks.size()<<" gaps:";for(size_t n=1;n<attacks.size();++n)std::cout<<' '<<attacks[n]-attacks[n-1];std::cout<<'\n';
  const auto file=dir.getChildFile(juce::String(p.isKStation()?"K":"KS")+"-clock-"+scenario.name+".wav");auto stream=file.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,2,24,{},0));check(writer && writer->writeFromAudioSampleBuffer(capture,0,capture.getNumSamples()),"Clock audio capture");
  check(attacks.size()>=3,"Arp attacks not detected");double average=0;for(size_t n=1;n<attacks.size();++n)average+=attacks[n]-attacks[n-1];average/=attacks.size()-1;
  check(std::abs(average-48000*60/tempo)<512,"Arp does not follow selected clock source");
  for(size_t n=1;n<attacks.size();++n)check(std::abs((attacks[n]-attacks[n-1])-average)<768,"Pulse edits disturb arp timing");
 }
 midi.addEvent(juce::MidiMessage::noteOff(1,60),0);p.setPlayHead(nullptr);set("arp_on",0);for(int n=0;n<100;++n)p.processBlock(audio,midi);
 juce::MemoryBlock saved;p.getStateInformation(saved);check(dir.getParentDirectory().getChildFile(p.isKStation()?"K-ClockProbe.state":"KS-ClockProbe.state").replaceWithData(saved.getData(),saved.getSize()),"Save wrapper clock fixture");
 std::cout<<"PASS: Host/Internal/MIDI Clock isolation and live handovers with conflicting tempos, Pulse edits, state reads and no callback allocations\n";
}
static std::vector<uint8_t> read(const juce::File& f){juce::MemoryBlock b;check(f.loadFileAsData(b),"Read private fixture");const auto* p=static_cast<const uint8_t*>(b.getData());return {p,p+b.getSize()};}
static void nativeImports(Library& library,const juce::File& os,const juce::File& wave,const juce::File& flash,const juce::File& folder){
 ks::Rom rom(os.getFullPathName().toStdString());check(rom.isValid(),"Reference KS firmware");
 auto board=ks::test::makeLegacyBoard(rom.data(),read(wave),read(flash));check(board->boot(),"Reference KS boot");
 const auto counter=[&]{return (unsigned(board->read(0xffffe6))<<8)|board->read(0xffffe7);};
 check(board->read(0xffffe0)==6,"KS clock timer prescaler");
 auto before=counter();auto cycles=ks::test::comparisonCpu(*board).cycles;board->advance(4096);
 check(counter()==((before+((ks::test::comparisonCpu(*board).cycles>>8)-(cycles>>8)))&65535),"TPU1 counter does not follow CPU/256");
 const auto start=board->read(0xffffc0);board->write(0xffffc0,uint8_t(start&~2));before=counter();board->advance(4096);check(counter()==before,"Stopped TPU1 counter advanced");board->write(0xffffc0,start);
 board->write(0xffffe6,255);board->write(0xffffe7,255);board->advance(4096);check(counter()<64 && (board->read(0xffffe5)&4),"TPU1 overflow/wrap");
 std::cout<<"PASS: original KS timer setup, CPU/256 count, stop and 16-bit overflow\n";
 for(uint8_t value:{1,0}){const uint8_t cc[]{0xb0,70,value};check(board->enqueueMidi(cc,3),"Glide reference CC");for(int n=0;n<2000;++n)board->renderSample();check(board->state().parts[0][5]==value,"KS glide field must be bit zero");}
 std::cout<<"PASS: original KS CC70 uses program byte 5 bit zero\n";
 const auto snapshot=ks::stateMessage(board->state());uint8_t byte=0;while(board->popMidiOut(byte)){}
 const uint8_t request[]{0xf0,0,0x20,0x29,1,0x42,0x7f,0x40,0,0x11,2,0,0,0xf7};check(board->enqueueMidi(request,sizeof(request)),"Reference dump request");
 std::vector<uint8_t> reply;bool complete=false;
 for(int n=0;n<16384 && !complete;++n){board->renderSample();while(board->popMidiOut(byte)){if(byte==0xf0)reply.clear();if(byte==0xf0 || !reply.empty())reply.push_back(byte);if(byte==0xf7 && reply.size()==270){complete=true;break;}}}
 check(complete,"Reference KS UART dump");for(unsigned n=0;n<209;++n)if(n<179 || n>=187)check(reply[13+n]==snapshot[13+n],"Read-only snapshot differs from original UART serialization");
 std::cout<<"PASS: read-only Single snapshot matches original UART dump (live part metadata uses its addressed mirrors)\n";
 unsigned count=0;
 for(const auto& file:folder.findChildFiles(juce::File::findFiles,true,"*.syx")){
  DataList frames;library.loadFile(frames,file.getFullPathName().toStdString());
  for(const auto& record:frames){auto imported=library.createPatch(Data(record),{});if(!imported)continue;const auto& frame=imported->sysex;const auto source=modelOf(frame);if(source!=Model::AStation && source!=Model::XStation)continue;
   auto message=frame;message[6]=0x7f;message[7]=0;message[8]=message[11]=message[12]=0;
   check(board->enqueueMidi(message.data(),message.size()),"Reference import enqueue");for(int n=0;n<10000;++n)board->renderSample();
   const auto state=board->state();const auto converted=convertPatch(frame,Model::KS);
   const unsigned length=source==Model::AStation?151:179;
   for(unsigned n=0;n<length;++n)if(state.parts[0][n]!=converted.data[13+n]){std::cerr<<file.getFileName()<<" source="<<modelName(source)<<" byte="<<n<<" original="<<int(state.parts[0][n])<<" converted="<<int(converted.data[13+n])<<'\n';throw std::runtime_error("Conversion differs from original KS importer");}
   ++count;
  }
 }
 check(count>0,"No external A/X reference programs");std::cout<<"PASS: "<<count<<" A/X program conversions match original KS firmware synth bytes\n";
}
int main(int argc,char** argv){std::cout<<std::unitbuf;std::cerr<<std::unitbuf;try{
 juce::ScopedJuceInitialiser_GUI gui;const auto root=juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName())).getChildFile("workflow-"+juce::Uuid().toString());check(root.createDirectory().wasOk(),"Test folder");
 queueChecks();{Library library(root.getChildFile("parse-db"));importChecks(library);drumParserChecks(library);vStationChecks(library);vStationProgramChecks(library);namingChecks(library);}cacheMigrationChecks(root);
 if(argc>=6 && !juce::SystemStats::getEnvironmentVariable("SILVER_TEST_IMPORTS_ONLY",{}).isEmpty()){Library library(root.getChildFile("reference-db"));nativeImports(library,juce::File(argv[2]),juce::File(argv[4]),juce::File(argv[3]),juce::File(argv[5]));return 0;}
 const bool browserOnly=!juce::SystemStats::getEnvironmentVariable("SILVER_TEST_BROWSER_ONLY",{}).isEmpty();
 Processor p;folderChecks(p,root);bufferChecks(p);p.prepareToPlay(48000,256);
 if(argc>=4 && juce::SystemStats::getEnvironmentVariable("SILVER_TEST_PERFORMANCE_CLOCK_ONLY",{}).isNotEmpty()){
  juce::String error;
  check(p.setKSControllerMode(1,error),error.toRawUTF8());
  p.flashFile=juce::File(argv[2]);
  p.bankFile=juce::File(argv[3]);
  check(p.loadFirmware(juce::File(argv[1]),error),error.toRawUTF8());
  performanceClockChecks(p);
  return 0;
 }
 if(!juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FILTER_CLOCK_ONLY",{}).isEmpty()){
  check(argc>=5,"Filter clock probe requires K OS, KS OS, KS Data and Wave ROM paths");
  const auto testerPatch=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FILTER_PATCH",{});
  const bool stress=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_ARP_STRESS",{}).isNotEmpty();
  const int stressBlock=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_ARP_BLOCK","256").getIntValue();
  if(stress){
   check(stressBlock==64 || stressBlock==128 || stressBlock==256 || stressBlock==512,"Arp stress block must be 64, 128, 256 or 512");
   p.prepareToPlay(48000,stressBlock);
  }
  const auto controller=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FILTER_KS_CONTROLLER","native");
  check(controller=="native" || controller=="hybrid" || controller=="h8","Filter clock controller must be native, hybrid or h8");
  for(int model:{1,2}){
   if(!juce::SystemStats::getEnvironmentVariable(model==1?"SILVER_TEST_SKIP_K":"SILVER_TEST_SKIP_KS",{}).isEmpty())continue;
   juce::String error;
   if(model==2)check(p.setKSControllerMode(controller=="native"?1:controller=="hybrid"?2:0,error),error.toRawUTF8());
   p.flashFile=juce::File(argv[3]);p.bankFile=juce::File(argv[4]);check(p.loadFirmware(juce::File(argv[model]),error),error.toRawUTF8());
   if(stress)arpStress(p,root,stressBlock);
   else if(testerPatch.isEmpty())filterClock(p,root);
   else filterPatchClock(p,juce::File(testerPatch),root);
  }return 0;
 }
 if(argc>=2 && !juce::SystemStats::getEnvironmentVariable("SILVER_TEST_DRUMS_ONLY",{}).isEmpty()){
  juce::String error;const auto waveFile=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_DRUM_WAVES",{});if(waveFile.isNotEmpty())p.bankFile=juce::File(waveFile);const auto flashFile=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_DRUM_FLASH",{});if(flashFile.isNotEmpty())check(p.setFlashRom(juce::File(flashFile),error),error.toRawUTF8());check(p.setKSControllerMode(1,error),error.toRawUTF8());check(p.loadFirmware(juce::File(argv[1]),error),error.toRawUTF8());drumBrowserChecks(p,root);drumPlaybackChecks(p,root);
  if(argc>=3){check(p.loadFirmware(juce::File(argv[2]),error),error.toRawUTF8());drumBrowserChecks(p,root);}return 0;
 }
 const auto levelFile=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_LEVEL",{});
 if(argc>=5 && levelFile.isNotEmpty()){for(int model:{1,2}){p.flashFile=juce::File(argv[3]);p.bankFile=juce::File(argv[4]);juce::String error;check(p.loadFirmware(juce::File(argv[model]),error),error.toRawUTF8());levelProbe(p,juce::File(levelFile));}return 0;}
 if(argc>=5 && !juce::SystemStats::getEnvironmentVariable("SILVER_TEST_FXP_ONLY",{}).isEmpty()){
  const auto folder=juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_TEST_VSTATION_FOLDER",{}));
  for(int model:{1,2}){p.flashFile=juce::File(argv[3]);p.bankFile=juce::File(argv[4]);juce::String error;check(p.loadFirmware(juce::File(argv[model]),error),error.toRawUTF8());vStationFolderPlayback(p,folder);}return 0;
 }
 if(argc>=5 && !juce::SystemStats::getEnvironmentVariable("SILVER_TEST_VSTATION_ONLY",{}).isEmpty()){
  const auto archive=juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_TEST_VSTATION",{}));
  for(int model:{1,2}){p.flashFile=juce::File(argv[3]);p.bankFile=juce::File(argv[4]);juce::String error;check(p.loadFirmware(juce::File(argv[model]),error),error.toRawUTF8());vStationPlayback(p,archive);}
  return 0;
 }
 if(argc>=5 && juce::SystemStats::getEnvironmentVariable("SILVER_TEST_SKIP_K",{}).isEmpty()){p.flashFile=juce::File(argv[3]);p.bankFile=juce::File(argv[4]);juce::String error;check(p.loadFirmware(juce::File(argv[1]),error),error.toRawUTF8());if(!browserOnly)audioClock(p,root);}
 userSourceUpgradeChecks(p,root);
 auto banks=root.getChildFile("browser-banks-"+juce::Uuid().toString());banks.createDirectory();browserChecks(p,banks);drumBrowserChecks(p,root);
 if(browserOnly)return 0;
 if(p.firmwareReady()){
  // Console Browser actions enqueue work while this fixture has no host
  // callback. Advance audio before requesting a firmware handoff.
  juce::AudioBuffer<float> audio(4,256);
  juce::MidiBuffer midi;
  for(int block=0;block<160;++block)p.processBlock(audio,midi);
 }
 if(argc>=5 && juce::SystemStats::getEnvironmentVariable("SILVER_TEST_SKIP_KS",{}).isEmpty()){p.flashFile=juce::File(argv[3]);p.bankFile=juce::File(argv[4]);juce::String error;check(p.loadFirmware(juce::File(argv[2]),error),error.toRawUTF8());audioClock(p,root);drumBrowserChecks(p,root);}
 if(argc>=6){Library library(root.getChildFile("external-db"));unsigned counts[5]{};for(const auto& file:juce::File(argv[5]).findChildFiles(juce::File::findFiles,true,"*.syx")){DataList data;library.loadFile(data,file.getFullPathName().toStdString());for(auto& message:data)if(auto patch=library.createPatch(std::move(message),file.getFileNameWithoutExtension().toStdString())){++counts[int(modelOf(patch->sysex))];for(auto target:{Model::KStation,Model::KS})check(validPatch(convertPatch(patch->sysex,target).data),"External bank conversion");}}
  for(int m=1;m<=4;++m)std::cout<<modelName(Model(m))<<" external programs="<<counts[m]<<'\n';check(counts[int(Model::AStation)] && counts[int(Model::XStation)],"External A/X banks missing");
  nativeImports(library,juce::File(argv[2]),juce::File(argv[4]),juce::File(argv[3]),juce::File(argv[5]));
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
