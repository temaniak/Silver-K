#include "silverProcessor.h"
#include "silverEditor.h"
#include "ksLib/ksRom.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace silverK;
namespace {
void check(bool v,const juce::String& why){if(!v)throw std::runtime_error(why.toStdString());}
juce::Component* find(juce::Component& c,const juce::String& id){if(c.getComponentID()==id)return &c;for(auto* child:c.getChildren())if(auto* found=find(*child,id))return found;return nullptr;}
juce::var report(Processor& p){return juce::JSON::parse(p.performanceReport());}
juce::String backendName(const char* name){
 juce::String expected=name;
 // Live switching exercises the explicit legacy comparison backend.
 // A fresh saved Native project restores the separate semantic owner.
 if(expected=="native-cpp-runtime"){
  const bool transport=juce::SystemStats::getEnvironmentVariable("SILVER_KS_DSP_TRANSPORT",{})=="events";
  const bool jobs=juce::SystemStats::getEnvironmentVariable("SILVER_KS_CONTROL_JOBS",{})=="1";
  if(jobs)expected=transport?"legacy-native-events-control-comparison":"legacy-native-control-jobs-comparison";
  else if(transport)expected="legacy-native-events-transport-comparison";
  else expected="legacy-native-runtime-comparison";
 }
 return expected;
}
void backend(Processor& p,const char* name){check(report(p)["controller_backend"].toString()==backendName(name),"Wrong active controller backend");}
}
int main(int argc,char** argv){try{
 juce::ScopedJuceInitialiser_GUI gui;
 const auto base=juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",{});
 if(base.isEmpty()&&argc!=2){std::cout<<"SKIP: isolated SILVER_K_DATA and private KS OS required\n";return 77;}
 check(base.isNotEmpty(),"Set SILVER_K_DATA to an isolated test directory");
 const auto settings=juce::File(base).getChildFile("settings.json");
 {
  auto referenceOwner=std::make_unique<Processor>(),candidateOwner=std::make_unique<Processor>();
  auto& reference=*referenceOwner;auto& candidate=*candidateOwner;juce::String error;
  check(reference.setKSControllerMode(0,error)&&candidate.setKSControllerMode(0,error),error);
  check(!candidate.setKSControllerMode(99,error)&&candidate.ksControllerMode()==0,"Invalid selection changed controller");
  check(candidate.setKSControllerMode(1,error),error);
  check(int(juce::JSON::parse(settings)["ksControllerMode"])==1,"New-instance preference missing");
  {auto fresh=std::make_unique<Processor>();check(fresh->ksControllerMode()==1,"New instance lost controller choice");}
  check(candidate.setKSControllerMode(0,error),error);
  if(argc!=2){std::cout<<"PASS: controller preference/invalid-choice checks; SKIP private live-switch check (supply KS OS)\n";return 77;}
  for(auto* p:{&reference,&candidate}){
   p->prepareToPlay(48000,256);p->setNonRealtime(true);check(p->loadFirmware(juce::File(argv[1]),error),error);backend(*p,"original-h8");
  }
  const auto revision=candidate.firmwareLoadRevision();const auto latency=candidate.getLatencySamples();
  std::unique_ptr<juce::AudioProcessorEditor> editor(candidate.createEditor());
  auto* setup=dynamic_cast<juce::TextButton*>(find(*editor,"pageSetup"));check(setup!=nullptr,"Missing Setup");setup->onClick();
  auto* choice=dynamic_cast<juce::ComboBox*>(find(*editor,"ksController"));check(choice&&choice->isVisible()&&choice->isEnabled()&&choice->getNumItems()==2,"Missing H8/C++ selector");
  check(candidate.setPerformanceDiagnostics(1),"Could not start H8 measurement");
  float peak=0;uint64_t samples=0;
  for(int block=0;block<60;++block){
   if(block==4 || block==10 || block==15 || block==24 || block==30){
    const bool native=block!=10&&block!=24;choice->setSelectedId(native?2:1,juce::dontSendNotification);choice->onChange();
    check(candidate.ksControllerMode()==int(native),"UI switch did not apply");backend(candidate,native?"native-cpp-runtime":"original-h8");backend(reference,"original-h8");
    check(candidate.firmwareLoadRevision()==revision&&candidate.getLatencySamples()==latency,"Controller switch rebuilt firmware or changed latency");
    if(block==4){auto r=report(candidate);check(!bool(r["recording"])&&r["stop_reason"].toString()=="controller_changed"&&r["session"]["controller_backend"].toString()=="original-h8","Mixed controller diagnostic session");}
    if(block==15){check(candidate.setPerformanceDiagnostics(1),"Could not start native measurement");check(report(candidate)["session"]["controller_backend"].toString()==backendName("native-cpp-runtime"),"Native diagnostic session mislabeled");}
   }
   juce::MidiBuffer midi;
   if(block==0)midi.addEvent(juce::MidiMessage::noteOn(1,60,juce::uint8(100)),0);
   if(block==6)midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0);
   if(block==8)midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
   if(block==12)midi.addEvent(juce::MidiMessage::noteOn(1,67,juce::uint8(90)),0);
   if(block==18)midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);
   if(block==28)midi.addEvent(juce::MidiMessage::noteOff(1,67),0);
   juce::AudioBuffer<float> a(4,256),b(4,256);a.clear();b.clear();juce::MidiBuffer copy(midi);
   reference.processBlock(a,midi);candidate.processBlock(b,copy);
   for(int c=0;c<4;++c)for(int i=0;i<256;++i){const auto x=a.getSample(c,i),y=b.getSample(c,i);check(std::isfinite(y)&&x==y,"Live controller switch changed audio");peak=std::max(peak,std::abs(y));++samples;}
  }
  check(peak>1e-5f&&candidate.underruns()==0&&reference.underruns()==0,"Silent or underrunning comparison");
  juce::MemoryBlock saved;candidate.getStateInformation(saved);
  auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));check(xml&&xml->getIntAttribute("ksControllerMode",-1)==1,"Project lost native choice");
  for(int mode:{0,1}){
   auto copy=juce::XmlElement(*xml);copy.setAttribute("ksControllerMode",mode);juce::MemoryBlock state;
   juce::AudioProcessor::copyXmlToBinary(copy,state);const auto file=juce::File(base).getChildFile(mode?"native.state":"h8.state");
   check(!file.exists()&&file.replaceWithData(state.getData(),state.getSize()),"Could not save private wrapper-check state");
  }
  check(candidate.setKSControllerMode(0,error),error);
  candidate.setStateInformation(saved.getData(),int(saved.getSize()));check(candidate.firmwareReady()&&candidate.ksControllerMode()==1,"Project did not restore native controller");backend(candidate,"native-cpp-semantic");
  check(int(juce::JSON::parse(settings)["ksControllerMode"])==0,"Project restore overwrote new-instance preference");
  xml->removeAttribute("ksControllerMode");juce::AudioProcessor::copyXmlToBinary(*xml,saved);
  candidate.setStateInformation(saved.getData(),int(saved.getSize()));check(candidate.firmwareReady()&&candidate.ksControllerMode()==0,"Old project did not retain original H8 default");backend(candidate,"original-h8");
  // Change only an inline LCD text byte in a private in-memory ROM copy:
  // H8 still boots; the strict native fingerprint must reject the handoff.
  ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid private ROM for guard test");auto image=rom.data();image[0x2a0c]^=1;
  juce::MemoryBlock init;check(candidate.dataFolder.getChildFile("ROMs/ks-init-flash.bin").loadFileAsData(init),"Missing synthetic test Flash");
  const auto* data=static_cast<const uint8_t*>(init.getData());
  auto guarded=std::make_unique<ks::Device>(image,std::vector<uint8_t>(ks::Board::WaveSize,0),std::vector<uint8_t>(data,data+init.getSize()),48000,256,std::vector<uint8_t>{},nullptr,ks::ControllerMode::H8);
  check(guarded->valid(),"H8 guard fixture failed to boot");std::string rejected;
  check(!guarded->setControllerMode(ks::ControllerMode::Native,rejected)&&!rejected.empty()&&guarded->valid()&&guarded->controllerMode()==ks::ControllerMode::H8,"Rejected native request did not preserve original H8");
  std::cout<<"PASS: "<<samples<<" exact float samples across five live UI switches, held notes/sustain; no reboot/latency change/underruns; instance isolation, diagnostic segmentation, preferences, native and legacy project restore\n";
 }
 check(int(juce::JSON::parse(settings)["ksControllerMode"])==0,"Stale instance overwrote selected controller preference");
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
