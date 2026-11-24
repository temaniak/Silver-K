#include "silverProcessor.h"
#include "../../controller/command/controllerArchive.h"
#include <iostream>
#include <stdexcept>
#include "ksLib/ksRom.h"
#include <fstream>
#include <thread>
#include <cstdlib>
#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
static LONG WINAPI reportCrash(EXCEPTION_POINTERS* exception){
 std::cerr<<"Windows exception 0x"<<std::hex<<exception->ExceptionRecord->ExceptionCode<<" at "<<exception->ExceptionRecord->ExceptionAddress<<std::dec<<std::endl;
 const auto process=GetCurrentProcess();SymSetOptions(SYMOPT_LOAD_LINES|SYMOPT_UNDNAME);SymInitialize(process,nullptr,TRUE);
 CONTEXT context=*exception->ContextRecord;STACKFRAME64 frame{};frame.AddrPC.Offset=context.Rip;frame.AddrStack.Offset=context.Rsp;frame.AddrFrame.Offset=context.Rbp;frame.AddrPC.Mode=frame.AddrStack.Mode=frame.AddrFrame.Mode=AddrModeFlat;
 for(int n=0;n<32 && frame.AddrPC.Offset;++n){
  alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=MAX_SYM_NAME;DWORD64 displacement=0;
  if(SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol))std::cerr<<symbol->Name;else std::cerr<<"0x"<<std::hex<<frame.AddrPC.Offset<<std::dec;
  IMAGEHLP_LINE64 line{};line.SizeOfStruct=sizeof(line);DWORD lineOffset=0;if(SymGetLineFromAddr64(process,frame.AddrPC.Offset,&lineOffset,&line))std::cerr<<" at "<<line.FileName<<":"<<line.LineNumber;std::cerr<<std::endl;
  if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,GetCurrentThread(),&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
 }
 return EXCEPTION_EXECUTE_HANDLER;
}
#endif
thread_local bool trackAudioNew=false;
thread_local size_t audioNewCount=0;
void* operator new(std::size_t n){if(trackAudioNew)++audioNewCount;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{std::free(p);}
#include "jucePluginLib/patchdb/search.h"
static std::vector<uint8_t> readBytes(const char* path){std::ifstream in(path,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
using namespace silverK;
static void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
static std::vector<uint8_t> fixture(){std::vector<uint8_t> p(270,0);const uint8_t h[]{0xf0,0,0x20,0x29,1,0x42,0x7f,0,0,0x11,2,0,0};std::copy_n(h,13,p.begin());p.back()=0xf7;for(const auto& c:ksPlugin::controls)if(c.patchOffset>=0){auto& byte=p[13+c.patchOffset];byte=uint8_t((byte&~c.patchMask)|((c.defaultValue<<c.patchShift)&c.patchMask));}return p;}
static pluginLib::patchDB::PatchPtr awaitPatch(Library& library){using namespace pluginLib::patchDB;auto handle=library.search(SearchRequest{});for(int n=0;n<500;++n){library.uiProcess();if(auto result=library.getSearch(handle)){std::shared_lock lock(result->resultsMutex);if(!result->results.empty())return *result->results.begin();}std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Library scan timeout");}
static void audioBlock(Processor& p,juce::AudioBuffer<float>& b,juce::MidiBuffer& m){trackAudioNew=true;p.processBlock(b,m);trackAudioNew=false;check(audioNewCount==0,"C++ heap allocation in audio callback");}
static void loadAsync(Processor& p,const juce::File& file,juce::String& error){
 const auto start=std::chrono::steady_clock::now();check(p.beginFirmwareLoad(file),"Start background firmware load");
 check(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<0.2,"Firmware start blocked the caller");
 check(!p.beginFirmwareLoad(file),"Duplicate background load accepted");
 juce::AudioBuffer<float> audio(4,256);juce::MidiBuffer midi;unsigned polls=0;
 while(!p.finishFirmwareLoad(error)){
  check(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<120,"Background firmware timeout");
  const auto tick=std::chrono::steady_clock::now();p.status();p.currentName();p.refreshControls();audio.clear();audioBlock(p,audio,midi);
  check(std::chrono::duration<double>(std::chrono::steady_clock::now()-tick).count()<0.2,"UI/audio blocked during firmware boot");
  ++polls;std::this_thread::sleep_for(std::chrono::milliseconds(10));
 }
 check(!p.isFirmwareLoading(),"Loading flag was not cleared");
 std::cout<<"Background load completed in "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" s; responsive polls="<<polls<<"\n";
}
static juce::Component* find(juce::Component& c,const juce::String& id){if(c.getComponentID()==id)return &c;for(int n=0;n<c.getNumChildComponents();++n)if(auto* result=find(*c.getChildComponent(n),id))return result;return nullptr;}
static juce::File testDirectory(){return juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName()));}
static void screenshot(Processor& processor,const char* name){auto editor=std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());auto file=juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName())).getChildFile(name);file.getParentDirectory().createDirectory();file.deleteFile();auto stream=file.createOutputStream();juce::PNGImageFormat png;check(stream && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*stream),"Screenshot");}
static std::vector<uint8_t> savedProgram(Processor& p){juce::MemoryBlock saved;p.getStateInformation(saved);auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));check(xml!=nullptr,"State XML");juce::MemoryBlock dump;check(dump.fromBase64Encoding(xml->getStringAttribute("program")),"Program state");const auto* b=static_cast<const uint8_t*>(dump.getData());return {b,b+dump.getSize()};}
static novation::controller::command::ControllerArchive savedKArchive(Processor& p){
 juce::MemoryBlock state,archive;
 p.getStateInformation(state);
 const auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));
 check(xml && archive.fromBase64Encoding(xml->getStringAttribute("kCommandState")),"K-Station archive");
 const auto* first=static_cast<const uint8_t*>(archive.getData());
 auto decoded=novation::controller::command::decodeControllerArchive({first,first+archive.getSize()});
 check(bool(decoded),"K-Station current program");
 return std::move(*decoded);
}
static std::vector<uint8_t> savedKProgram(Processor& p){return savedKArchive(p).activeProgram;}
static void checkKeptProgram(const juce::File& k,const juce::File& ks,const juce::File& data,
                             const juce::File& wave,const juce::File& preset){
 auto instance=std::make_unique<Processor>();
 auto& p=*instance;
 p.prepareToPlay(48000,256);
 p.setNonRealtime(true);
 juce::String error;
 check(p.setFlashRom(data,error),error.toRawUTF8());
 check(p.setWaveRom(wave,error),error.toRawUTF8());
 p.setFirmwareSwitch(Processor::FirmwareSwitch::KeepPatch);
 const auto boot=[&](const juce::File& file,bool wantK){
  loadAsync(p,file,error);
  check(error.isEmpty(),error.toRawUTF8());
  check(p.firmwareReady() && p.isKStation()==wantK,"Requested model did not load");
 };
 juce::MemoryBlock dump;
 check(preset.loadFileAsData(dump),"Read the test Program SysEx");
 const auto* raw=static_cast<const uint8_t*>(dump.getData());
 check(validPatch(raw,dump.getSize()) && dump.getSize()==270,"Expected a KS or X-Station Program");
 auto patch=std::make_shared<pluginLib::patchDB::Patch>();
 patch->sysex.assign(raw,raw+dump.getSize());
 patch->name=juce::String::fromUTF8(reinterpret_cast<const char*>(raw+13+151),16).trim().toStdString();
 std::cout<<modelName(modelOf(patch->sysex))<<" Program: "<<patch->name<<'\n';
 juce::AudioBuffer<float> audio(4,256);
 juce::MidiBuffer midi;
 const auto settle=[&]{
  for(int i=0;i<160;++i){audio.clear();audioBlock(p,audio,midi);}
 };
 const auto render=[&](const juce::String& stage){
  settle();
  juce::AudioBuffer<float> capture(2,256*384);
  for(int i=0;i<384;++i){
   const auto channel=p.editorChannel()+1;
   if(i==0)midi.addEvent(juce::MidiMessage::noteOn(channel,60,uint8_t(100)),0);
   if(i==288)midi.addEvent(juce::MidiMessage::noteOff(channel,60),0);
   audio.clear();
   audioBlock(p,audio,midi);
   for(int ch=0;ch<2;++ch)capture.copyFrom(ch,256*i,audio,ch,0,256);
  }
  check(p.underruns()==0,"Offline output underrun");
  const auto rms=std::max(capture.getRMSLevel(0,24000,48000),capture.getRMSLevel(1,24000,48000));
  std::cout<<stage<<" held RMS="<<rms<<" peak="<<capture.getMagnitude(0,capture.getNumSamples())<<'\n';
  auto file=testDirectory().getChildFile(stage+".wav");
  file.deleteFile();
  auto stream=file.createOutputStream();
  juce::WavAudioFormat waveFormat;
  std::unique_ptr<juce::AudioFormatWriter> writer(waveFormat.createWriterFor(stream.release(),48000,2,24,{},0));
  check(writer && writer->writeFromAudioSampleBuffer(capture,0,capture.getNumSamples()),"Write the note capture");
  return rms;
 };
 boot(ks,false);
 check(p.activatePatch(patch,error),error.toRawUTF8());
 const auto originalRms=render("KS-original");
 const auto original=savedProgram(p);
 boot(k,true);
 const auto kRms=render("K-converted");
 const auto converted=savedKProgram(p);
 const auto direct=convertPatch(patch->sysex,Model::KStation);
 check(direct.data.size()==142 &&
       std::equal(direct.data.begin()+13+37,direct.data.begin()+13+42,converted.begin()+13+37),
       "Direct import and firmware switching should use the same mixer conversion");
 boot(ks,false);
 const auto returnedRms=render("KS-returned");
 const auto returned=savedProgram(p);
 std::cout<<"K oscillator levels: "<<int(converted[13+37])<<' '
          <<int(converted[13+38])<<' '<<int(converted[13+39])<<'\n';
 check(originalRms>1e-4,"The original preset did not sustain a note");
 check(returned==original && p.currentName()==juce::String(patch->name),"Switching back to KS lost original Program data or name");
 check(returnedRms>originalRms*.5f,"Switching back to KS lost sustained sound");
 check(kRms>1e-4,"The converted K-Station Program did not sustain a note");

 boot(k,true);
 juce::MemoryBlock state;
 p.getStateInformation(state);
 p.setStateInformation(state.getData(),int(state.getSize()));
 check(p.firmwareReady() && p.isKStation(),"Restore the converted K-Station instance");
 boot(ks,false);
 check(savedProgram(p)==original,"Saved project lost the original after switching back to KS");

 boot(k,true);
 const auto cutoff=uint8_t((savedKProgram(p)[13+46]+1)&127);
 auto* parameter=p.uiParameter("cutoff");
 parameter->setValueNotifyingHost(parameter->convertTo0to1(cutoff));
 settle();
 boot(ks,false);
 check(savedProgram(p)[13+46]==cutoff,"Keep discarded a deliberate K-Station edit");

 check(p.activatePatch(patch,error),error.toRawUTF8());
 settle();
 boot(k,true);
 auto replacement=std::make_shared<pluginLib::patchDB::Patch>();
 replacement->name="Replacement";
 const auto savedProgramBytes=savedKProgram(p);
 replacement->sysex.assign(savedProgramBytes.begin(),savedProgramBytes.end());
 check(p.activatePatch(replacement,error),error.toRawUTF8());
 settle();
 boot(ks,false);
 check(p.currentName()=="Replacement" && savedProgram(p)!=original,"New K-Station selection resurrected the original KS patch");
 std::cout<<"PASS: original Program round trip, saved-project restore, intentional edit and preset replacement\n";
}
static void checkFirmwareSwitches(const juce::File& k,const juce::File& ks,
                                  const juce::File& data,const juce::File& wave){
 auto modulated=fixture();
 auto* mixer=modulated.data()+13;
 std::fill_n(mixer+37,5,0);
 const auto mixerLevels=[](const Conversion& conversion){
  return std::array<uint8_t,5>{conversion.data[50],conversion.data[51],conversion.data[52],
                              conversion.data[53],conversion.data[54]};
 };
 check(mixerLevels(convertPatch(modulated,Model::KStation))==std::array<uint8_t,5>{},
       "An intentionally silent mixer must stay silent");
 modulated[9]=0x10;modulated[10]=0; // X-Station transport.
 mixer[133]=1;mixer[141]=104;mixer[142]=24;mixer[143]=80;
 check(mixerLevels(convertPatch(modulated,Model::KStation))==std::array<uint8_t,5>{20,10,8,0,0},
       "X-Station mixer fallback for unipolar, bipolar and envelope sources");
 mixer[39]=91;
 check(mixerLevels(convertPatch(modulated,Model::KStation))==std::array<uint8_t,5>{0,0,91,0,0},
       "An audible authored mixer must stay unchanged");
 auto waveFixture=fixture();
 for(unsigned waveform=0;waveform<32;++waveform){
  std::fill_n(waveFixture.begin()+13+128,3,uint8_t(waveform));
  const auto converted=convertPatch(waveFixture,Model::KStation);
  const auto expected=waveform<4?waveform:1u;
  check(converted.data.size()==142 && (converted.data[13+5]&63)==expected*21,
        "KS waveform conversion on all three K-Station oscillators");
 }
 std::cout<<"PASS: all 32 KS waveforms checked on each K-Station oscillator\n";
 auto instance=std::make_unique<Processor>();
 auto& p=*instance;
 juce::String error;
 auto boot=[&](Processor& processor,const juce::File& file,bool wantK){
  loadAsync(processor,file,error);
  check(error.isEmpty(),error.toRawUTF8());
  check(processor.firmwareReady() && processor.isKStation()==wantK,"Requested model did not load");
 };
 auto render=[](Processor& processor){
  juce::AudioBuffer<float> audio(4,256);
  juce::MidiBuffer midi;
  for(int i=0;i<125;++i){
   audio.clear();
   audioBlock(processor,audio,midi);
  }
  processor.refreshControls();
  check(processor.underruns()==0,"Firmware switch dropped offline output");
 };
 const auto select=[&](Processor& processor,std::vector<uint8_t> bytes,const char* name){
  auto patch=std::make_shared<pluginLib::patchDB::Patch>();
  patch->sysex.assign(bytes.begin(),bytes.end());patch->name=name;
  check(processor.activatePatch(patch,error),error.toRawUTF8());
 };
 const auto rename=[](uint8_t* patch,const juce::String& name){
  std::fill_n(patch+151,16,' ');
  std::copy_n(name.toRawUTF8(),std::min(16,name.length()),patch+151);
 };
 const auto setKExpression=[&]{
  const auto channel=uint8_t(0xb0|p.editorChannel());
  check(p.sendShortMidi(channel,7,127) && p.sendShortMidi(channel,1,28) &&
        p.sendShortMidi(channel,2,31),"Queue K-Station expression CCs");
  render(p);
  const std::vector<uint8_t> expression{channel,7,127,channel,1,28,channel,2,31};
  check(savedKArchive(p).pendingControl==expression,"K-Station archive did not retain the applied CC values");
  return expression;
 };
 p.prepareToPlay(48000,256);p.setNonRealtime(true);
 check(p.setFlashRom(data,error),error.toRawUTF8());
 check(p.setWaveRom(wave,error),error.toRawUTF8());
 p.setFirmwareSwitch(Processor::FirmwareSwitch::KeepPatch);
 boot(p,k,true);
 auto kPatch=savedKProgram(p);
 kPatch[13+5]=uint8_t((kPatch[13+5]&64)|56); // Sine, Saw, Pulse.
 kPatch[13+46]=91;
 kPatch[13+88]&=~4;
 select(p,kPatch,"K transfer");
 render(p);
 auto* cutoff=p.uiParameter("cutoff");
 cutoff->setValueNotifyingHost(cutoff->convertTo0to1(83));
 render(p);
 check(savedKProgram(p)[13+46]==83,"K-Station cutoff edit was not applied");
 setKExpression();
 std::cout<<"Switching with K-Station volume 127, modulation wheel 28 and breath 31\n";
 boot(p,ks,false);
 auto transferred=savedProgram(p);
 check(!p.performanceMode() && transferred[13+46]==83 &&
       transferred[13+128]==0 && transferred[13+129]==2 && transferred[13+130]==3 &&
       p.currentName()=="K transfer","Keep current patch, K-Station to KS");
 std::cout<<"PASS: Keep K-Station -> KS, live edit, name, Sine/Saw/Pulse\n";

 ks::State scene;
 scene.performance=true;scene.activeMask=13;
 for(unsigned i=0;i<ks::PartCount;++i){
  auto& part=scene.parts[i];
  std::copy_n(transferred.begin()+13,256,part.begin());
  rename(part.data(),"Switch part "+juce::String(i+1));
  part[46]=uint8_t(22+11*i);
  part[179]=uint8_t(59+9*i);
  part[180]=uint8_t(i&1);part[181]=uint8_t(i);
  part[182]=uint8_t(32+i);part[183]=uint8_t(103-i);
  part[185]=uint8_t(64+i);
  part[187]=1;part[188]=uint8_t(i);
 }
 auto& drumPart=scene.parts[2];
 drumPart[87]=7;drumPart[88]|=4;
 drumPart[128]=4;drumPart[129]=17;drumPart[130]=31;
 drumPart[140]=2;
 auto& name=scene.parts[0];
 name[189]=1;name[190]=scene.activeMask;
 std::copy_n("Switch Test Perf",16,name.begin()+192);
 select(p,ks::stateMessage(scene),"Switch Test Perf");
 render(p);
 p.selectPart(2);
 const auto performance=savedProgram(p);
 check(performance.size()==1038 && p.performanceMode() && p.activeParts()==13 &&
       p.currentName().startsWith("Switch Test Perf"),"Prepare KS Performance");
 boot(p,k,true);
 transferred=savedKProgram(p);
 check((transferred[13+5]&63)==21 && transferred[13+46]==44 &&
       transferred[13+87]==0 && !(transferred[13]&32) && p.currentName()=="Switch part 3",
       "Keep selected KS Performance part, Triangle, LP12 and Arp fallback");
 std::cout<<"PASS: Keep KS Performance part 3 -> K-Station, extra waves -> Triangle, defaults applied\n";
 boot(p,ks,false);
 check(p.performanceMode() && p.currentPart()==2 && savedProgram(p)==performance,
       "Keep round trip must restore all KS Performance parts and selected part");
 boot(p,k,true);

 kPatch[13+46]=72;
 kPatch[13+5]=uint8_t((kPatch[13+5]&64)|39);
 select(p,kPatch,"K remembered");
 render(p);
 const auto rememberedK=savedKProgram(p);
 const auto rememberedExpression=setKExpression();
 p.setFirmwareSwitch(Processor::FirmwareSwitch::RecallLast);
 const auto expectPerformance=[&](Processor& processor){
  check(processor.performanceMode() && processor.currentPart()==2 && processor.activeParts()==13,
        "Performance mode, active parts and selected part restored");
  const auto recalled=savedProgram(processor);
  check(recalled==performance,"All four Performance parts round-trip exactly");
 };
 boot(p,ks,false);
 expectPerformance(p);
 juce::AudioBuffer<float> audio(4,256);
 juce::MidiBuffer midi;
 float silentPeak=0;
 for(int i=0;i<70;++i){
  audio.clear();
  audioBlock(p,audio,midi);
  silentPeak=std::max(silentPeak,audio.getMagnitude(0,audio.getNumSamples()));
 }
 std::cout<<"Restored Performance before Note On: peak="<<silentPeak<<"\n";
 check(silentPeak<1e-5,"Remembered drum Performance started without a key");
 boot(p,k,true);
 render(p);
 const auto recalledK=savedKArchive(p);
 check(recalledK.activeProgram==rememberedK && recalledK.pendingControl==rememberedExpression &&
       p.currentName()=="K remembered","Recall K-Station Program and applied expression");
 std::cout<<"PASS: Recall K-Station -> KS Performance and KS -> K-Station, exact programs\n";
 juce::MemoryBlock hostState;
 p.getStateInformation(hostState);
 auto restored=std::make_unique<Processor>();
 check(restored->firmwareSwitch()==Processor::FirmwareSwitch::RecallLast,"Firmware switch preference did not persist");
 restored->prepareToPlay(48000,256);
 restored->setNonRealtime(true);
 restored->setStateInformation(hostState.getData(),int(hostState.getSize()));
 check(restored->firmwareSwitch()==Processor::FirmwareSwitch::RecallLast &&
       savedKProgram(*restored)==rememberedK,"Saved K-Station and switch option");
 boot(*restored,ks,false);
 expectPerformance(*restored);
 check(restored->setPerformanceMode(false,error),error.toRawUTF8());
 render(*restored);
 auto single=savedProgram(*restored);
 check(single.size()==270,"Expected Single after leaving Performance");
 single[13+46]=69;
 rename(single.data()+13,"KS remembered");
 select(*restored,single,"KS remembered");
 render(*restored);
 const auto rememberedKS=savedProgram(*restored);
 restored->getStateInformation(hostState);
 p.setStateInformation(hostState.getData(),int(hostState.getSize()));
 boot(p,k,true);
 check(savedKProgram(p)==rememberedK,"Inactive K-Station persisted with KS state");
 boot(p,ks,false);
 check(!p.performanceMode() && savedProgram(p)==rememberedKS && p.currentName()=="KS remembered",
       "Most recent KS Single recalled");
 std::cout<<"PASS: inactive firmware state survives host save/restore in both directions; KS Single replaces last Performance\n";

 p.setFirmwareSwitch(Processor::FirmwareSwitch::KeepPatch);
 single[13+46]=38;
 rename(single.data()+13,"Last queued");
 select(p,single,"Last queued");
 boot(p,k,true);
 check(savedKProgram(p)[13+46]==38 && p.currentName()=="Last queued","Keep mode lost a just-selected preset");
 check(!p.load(ks,testDirectory().getChildFile("missing-wave.bin"),data,error) &&
       p.firmwareReady() && p.isKStation() && p.currentName()=="Last queued","Failed firmware load changed the live sound");
 std::cout<<"PASS: asynchronous switch retains a queued preset and preserves the live sound on load failure\n";

 auto firstVisit=std::make_unique<Processor>();
 firstVisit->setNonRealtime(true);
 firstVisit->setFirmwareSwitch(Processor::FirmwareSwitch::RecallLast);
 boot(*firstVisit,k,true);
 select(*firstVisit,kPatch,"First K-Station");
 boot(*firstVisit,ks,false);
 check(!firstVisit->performanceMode() && firstVisit->currentName()!="First K-Station",
       "Recall mode must use the firmware default on the first visit");
 boot(*firstVisit,k,true);
 check(savedKProgram(*firstVisit)==rememberedK && firstVisit->currentName()=="First K-Station",
       "First firmware visit was not remembered");
 std::cout<<"PASS: first visit uses the target default and records its previous model\n";
}
static void checkLayerEdits(Processor& p,juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){
 auto set=[&](unsigned part,const char* id,int value){auto* param=p.parameters.getParameter(part?"part"+juce::String(part+1)+"_"+id:juce::String(id));check(param!=nullptr,"Part parameter");param->setValueNotifyingHost(param->convertTo0to1(float(value)));};
 auto settle=[&]{for(int n=0;n<120;++n)audioBlock(p,audio,midi);p.refreshControls();};
 for(unsigned part=0;part<4;++part){set(part,"part_channel",0);set(part,"volume",70);}
 settle();auto before=savedProgram(p);check(before.size()==1038,"Layer Performance dump");
 for(unsigned part=0;part<4;++part)check(before[13+part*256+181]==0,"Shared channel assignment");
 midi.addEvent(juce::MidiMessage::noteOn(1,69,uint8_t(100)),0);settle();check(audio.getMagnitude(0,0,256)>1e-5,"Layered held note");
 set(1,"volume",10);set(1,"part_transpose",76);set(1,"part_fine",70);settle();
 auto after=savedProgram(p);
 for(unsigned part=0;part<4;++part){const auto at=13+part*256;check(after[at+181]==0,"Editor edit changed the user's MIDI channel");check(after[at+179]==(part==1?10:70),"Layer edit changed a neighbour's volume");check(after[at+184]==(part==1?76:64),"Layer edit changed a neighbour's transpose");check(after[at+185]==(part==1?70:64),"Layer edit changed a neighbour's fine tuning");}
 check(audio.getMagnitude(0,0,256)>1e-5,"Part edit stopped held notes");
 midi.addEvent(juce::MidiMessage::controllerEvent(1,7,45),0);settle();after=savedProgram(p);
 for(unsigned part=0;part<4;++part)check(after[13+part*256+179]==45,"External CC must still reach every layered part");
 midi.addEvent(juce::MidiMessage::controllerEvent(1,98,89),0);settle();set(1,"part_fine",71);settle();
 midi.addEvent(juce::MidiMessage::controllerEvent(1,6,64),0);settle();after=savedProgram(p);
 for(unsigned part=0;part<4;++part){check(after[13+part*256+184]==64,"Editor NRPN corrupted the external sender's selector");check(after[13+part*256+185]==(part==1?71:64),"External Data Entry changed the wrong parameter");}
 midi.addEvent(juce::MidiMessage::noteOff(1,69),0);settle();check(audio.getMagnitude(0,0,256)<1e-5,"Layered Note Off left a stuck voice");
 set(1,"voice_level",30);settle();after=savedProgram(p);check(after[13+256+125]==30 && after[13+125]==0,"Program boost is not per part");
 for(unsigned part:{0u,2u,3u})set(part,"volume",0);settle();
 midi.addEvent(juce::MidiMessage::noteOn(1,69,uint8_t(100)),0);settle();check(audio.getMagnitude(0,0,256)>1e-5,"Maximum valid boost muted the voice");
 for(int n=0;n<256;++n)check(std::isfinite(audio.getSample(0,n)),"Nonfinite audio after maximum valid boost");
 set(1,"voice_level",0);settle();check(audio.getMagnitude(0,0,256)>1e-5,"Returning boost to zero muted the held voice");
 midi.addEvent(juce::MidiMessage::noteOff(1,69),0);settle();for(unsigned part:{0u,2u,3u})set(part,"volume",45);settle();p.selectPart(1);
 auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());auto* perf=dynamic_cast<juce::TextButton*>(find(*editor,"pagePerf"));check(perf!=nullptr,"Perf tab");perf->onClick();
 for(const char* id:{"performanceMode","partActive"}){auto* toggle=dynamic_cast<juce::ToggleButton*>(find(*editor,id));check(toggle && toggle->findColour(juce::ToggleButton::textColourId)==juce::Colour(0xff171b20),"Performance toggle has unreadable text");}
 auto* panel=find(*editor,"performancePanel");check(panel && panel->isVisible() && find(*panel,"performance:part2:volume:slider"),"Perf part volume is missing");
 auto file=testDirectory().getChildFile("Silver-K-Performance.png");file.deleteFile();auto stream=file.createOutputStream();juce::PNGImageFormat png;check(stream && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*stream),"Performance screenshot");
 std::cout<<"PASS: same-channel layer edits preserve neighbour tuning/volume and held notes, external CC/Note Off, per-part boost, readable Perf controls\n";
}
int main(int argc,char** argv){std::cout<<std::unitbuf;std::cerr<<std::unitbuf;
#if defined(_WIN32)
 SetUnhandledExceptionFilter(reportCrash);
#endif
 try{std::cout<<"Initialize GUI"<<std::endl;juce::ScopedJuceInitialiser_GUI gui;
 if(argc>=2 && juce::String(argv[1])=="--keep-patch-round-trip"){
  check(argc==7,"Pass K firmware, KS firmware, KS Data, Wave ROM and a Program SysEx");
  checkKeptProgram(juce::File(argv[2]),juce::File(argv[3]),juce::File(argv[4]),juce::File(argv[5]),juce::File(argv[6]));
  return 0;
 }
 if(argc>=2 && juce::String(argv[1])=="--firmware-switch"){
  check(argc==6,"Pass K firmware, KS firmware, KS Data and Wave ROM");
  checkFirmwareSwitches(juce::File(argv[2]),juce::File(argv[3]),juce::File(argv[4]),juce::File(argv[5]));
  return 0;
 }
 std::cout<<"KS board storage: "<<sizeof(ks::Board)<<" bytes (heap in firmware checks)"<<std::endl;std::cout<<"Check patch conversion"<<std::endl;
 auto ks=fixture();ks[13+128]=27;ks[13+129]=4;ks[13+130]=31;ks[13+140]=2;ks[13+87]=7;auto k=convertPatch(ks,Model::KStation);check(k.data.size()==142,"KS->K size");check((k.data[18]&63)==21,"Extra waves must become Triangle");check((k.data[13]&32)==0,"Filter must become LP12");check(k.data[13+87]==0,"Unsupported arp must default");check(k.changes.size()>=5,"Replacement report");auto round=convertPatch(k.data,Model::KS);check(round.data.size()==270,"K->KS size");check(round.data[13+128]==1,"Converted Triangle must remain Triangle");check(validPatch(round.data),"Converted dump valid");auto native=convertPatch(ks,Model::KS);check(native.data==ks,"Native payload exact");ks[21]=255;check(convertPatch(ks,Model::KStation).data.empty(),"Malformed dump rejected");
std::cout<<"Conversion passed; create file fixtures"<<std::endl;const auto temp=testDirectory().getChildFile("silver-library-"+juce::Uuid().toString());check(temp.createDirectory().wasOk(),"Create test directory");const auto folder=temp.getChildFile(juce::String::fromUTF8("banks-банк"));folder.createDirectory();const auto file=folder.getChildFile(juce::String::fromUTF8("check-тест.syx"));auto test=fixture();check(file.replaceWithData(test.data(),test.size()),"Write test fixture");const auto badRom=temp.getChildFile(juce::String::fromUTF8("прошивка.bin"));std::vector<uint8_t> emptyImage(ks::Rom::ImageSize,0);check(badRom.replaceWithData(emptyImage.data(),emptyImage.size()),"Write synthetic ROM-reader fixture");ks::Rom readUnicode(badRom.getFullPathName().toStdString());check(readUnicode.error()=="Unrecognized or damaged firmware; use KS OS 2.1.02.","UTF-8 ROM path must reach hash validation");
 std::cout<<"Scan UTF-8 patch folder"<<std::endl;{Library db(temp.getChildFile("db"));db.addPath(folder);auto patch=awaitPatch(db);check(modelOf(patch->sysex)==Model::KS,"Library source model");pluginLib::patchDB::TypedTags tags;tags.add(pluginLib::patchDB::TagType::Tag,"Bass");tags.add(pluginLib::patchDB::TagType::Favourites,"Favourite");check(db.modifyTags({patch},tags),"Modify tags");}
 std::cout<<"Reload tags"<<std::endl;{Library db(temp.getChildFile("db"));auto patch=awaitPatch(db);check(patch->getTags(pluginLib::patchDB::TagType::Tag).containsAdded("Bass"),"Tags persisted");check(!patch->getTags(pluginLib::patchDB::TagType::Favourites).empty(),"Favourite persisted");}
 std::cout<<"Folder scan, source model, tags and favourite persistence passed\n";
 std::cout<<"Create processor and editor"<<std::endl;Processor p;{Processor peer;check(&p.patchLibrary==&peer.patchLibrary,"Instances must share the library");}check(p.getName()=="Silver-K","Product name");check(p.spec("delay_level") && p.uiParameter("delay_level"),"KS skin delay binding");check(p.parameters.getParameter("k_cutoff")!=nullptr,"K automation");check(p.parameters.getParameter("part4_part_output")!=nullptr,"KS part4 automation");
 p.skinChoice="Silver-K"; // This executable checks the original skin's components and dimensions.
 auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());
 const auto* panel=find(*editor,"mainPanel");
 check(panel && panel->getWidth()==1280 && panel->getHeight()==1000,"Native skin canvas dimensions");
 if(auto* button=dynamic_cast<juce::TextButton*>(find(*editor,"pageMain")))button->onClick();
 check(find(*editor,"control:delay")!=nullptr,"KS delay control present");
 {juce::String error;loadAsync(p,badRom,error);check(!error.isEmpty() && !p.firmwareReady(),"Failed async load did not report error");loadAsync(p,badRom,error);check(!error.isEmpty(),"Async retry lost error");}
 if(argc>=5){juce::String error;p.flashFile=juce::File(argv[3]);p.bankFile=juce::File(argv[4]);p.prepareToPlay(48000,256);p.setNonRealtime(true);check(p.loadFirmware(juce::File(argv[1]),error),error.toRawUTF8());check(p.isKStation(),"K model detected");screenshot(p,"Silver-K-KStation.png");juce::AudioBuffer<float> audio(4,256);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,69,uint8_t(100)),0);float peak=0;for(int n=0;n<250;++n){audioBlock(p,audio,midi);peak=std::max(peak,audio.getMagnitude(0,0,256));for(int ch=2;ch<4;++ch)check(audio.getMagnitude(ch,0,256)==0,"K Aux silent");}check(peak>1e-5,"K audio");auto* cutoff=p.parameters.getParameter("k_cutoff");cutoff->setValueNotifyingHost(cutoff->convertTo0to1(0));double low=0,high=0;for(int n=0;n<120;++n){audioBlock(p,audio,midi);if(n>60)low+=audio.getRMSLevel(0,0,256);}cutoff->setValueNotifyingHost(cutoff->convertTo0to1(127));for(int n=0;n<120;++n){audioBlock(p,audio,midi);if(n>60)high+=audio.getRMSLevel(0,0,256);}check(high>low*2,"K held-note cutoff must react live");juce::MemoryBlock saved;p.getStateInformation(saved);juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName())).getChildFile("K.state").replaceWithData(saved.getData(),saved.getSize());Processor restore;restore.prepareToPlay(48000,256);restore.setStateInformation(saved.getData(),int(saved.getSize()));check(restore.firmwareReady() && restore.isKStation(),"K state restore");restore.prepareToPlay(48000,256);check(restore.firmwareReady(),"K state remains valid on second prepare");restore.setNonRealtime(true);midi.addEvent(juce::MidiMessage::noteOn(1,69,uint8_t(100)),0);float restoredPeak=0;for(int n=0;n<200;++n){audioBlock(restore,audio,midi);restoredPeak=std::max(restoredPeak,audio.getMagnitude(0,0,256));}check(restoredPeak>1e-5,"K restored audio after second prepare");
 if(argc>=6){auto original=readBytes(argv[5]);ks::Rom os(argv[2]);auto referenceBoard=std::make_unique<ks::Board>(os.data(),readBytes(argv[4]),readBytes(argv[3]));auto& board=*referenceBoard;check(board.boot(),"KS reference boot");for(int phase=0;phase<16;++phase){auto patch=original;patch[13+80]=uint8_t((patch[13+80]&7)|(phase<<3));board.enqueueMidi(patch.data(),patch.size());for(int n=0;n<16000;++n)board.renderSample();auto native=board.state().parts[0];auto converted=convertPatch(patch,Model::KS);for(unsigned n=0;n<151;++n)if(native[n]!=converted.data[13+n]){std::cerr<<"K->KS mismatch phase="<<phase<<" byte="<<n<<" native="<<unsigned(native[n])<<" converted="<<unsigned(converted.data[13+n])<<"\n";throw std::runtime_error("K->KS differs from original importer");}}std::cout<<"Original KS importer and Silver-K conversion agree at every synth byte for all 16 keysync values\n";}
 p.setNonRealtime(false);loadAsync(p,juce::File(argv[2]),error);check(error.isEmpty(),error.toRawUTF8());p.setNonRealtime(true);check(!p.isKStation(),"KS model detected");
 midi.addEvent(juce::MidiMessage::noteOn(1,69,uint8_t(100)),0);float ksPeak=0;for(int n=0;n<200;++n){audioBlock(p,audio,midi);ksPeak=std::max(ksPeak,audio.getMagnitude(0,0,256));}check(ksPeak>1e-5,"KS init flash must produce audio");midi.addEvent(juce::MidiMessage::noteOff(1,69),0);for(int n=0;n<200;++n)audioBlock(p,audio,midi);std::cout<<"KS init flash audio peak="<<ksPeak<<"\n";
 check(p.setPerformanceMode(true,error),error.toRawUTF8());for(int n=0;n<120;++n)audioBlock(p,audio,midi);p.refreshControls();check(p.performanceMode(),"KS Performance enabled");checkLayerEdits(p,audio,midi);screenshot(p,"Silver-K-KS.png");p.selectPart(3);check(p.currentPart()==3,"Part4 selected");
 for(const auto& item:std::initializer_list<std::pair<const char*,int>>{{"part3_part_channel",8},{"part3_part_output",1},{"part4_part_output",1}}){auto* v=p.parameters.getParameter(item.first);v->setValueNotifyingHost(v->convertTo0to1(float(item.second)));}
 for(int n=0;n<120;++n)audioBlock(p,audio,midi);p.getStateInformation(saved);juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName())).getChildFile("KS.state").replaceWithData(saved.getData(),saved.getSize());restore.setStateInformation(saved.getData(),int(saved.getSize()));check(restore.firmwareReady() && restore.performanceMode() && restore.currentPart()==3,"KS Performance state restore");auto restored=savedProgram(restore);check(restored[13+181]==0 && restored[13+256+181]==0 && restored[13+512+181]==8 && restored[13+768+181]==0,"Performance restore leaked private channel assignments");check(restored[13+256+185]==71,"Performance restore lost per-part tuning");std::cout<<"K/KS firmware switch, audio, K silent Aux and both state round trips passed; K peak="<<peak<<"\n";
 }
 std::cout<<"Silver-K conversion/editor/parameter checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
