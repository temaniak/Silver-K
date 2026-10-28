#include "silverProcessor.h"
#include "silverEditor.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace silverK;
namespace {
void check(bool value,const juce::String& why){if(!value)throw std::runtime_error(why.toStdString());}
juce::Component* find(juce::Component& c,const juce::String& id){if(c.getComponentID()==id)return &c;for(auto* child:c.getChildren())if(auto* found=find(*child,id))return found;return nullptr;}
juce::var report(Processor& p){return juce::JSON::parse(p.performanceReport());}
}
int main(int argc,char** argv){try{
 juce::ScopedJuceInitialiser_GUI gui;
 const auto base=juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName());
 const auto root=juce::File(base).getChildFile("silver-dsp-workers-"+juce::Uuid().toString());
 check(root.createDirectory().wasOk(),"Create isolated test data");
#ifdef _WIN32
 _putenv_s("SILVER_K_DATA",root.getFullPathName().toRawUTF8());_putenv_s("SILVER_KS_DSP_THREADS","1");
#else
 setenv("SILVER_K_DATA",root.getFullPathName().toRawUTF8(),1);setenv("SILVER_KS_DSP_THREADS","1",1);
#endif
 const auto settings=root.getChildFile("settings.json");
 auto referenceOwner=std::make_unique<Processor>(),candidateOwner=std::make_unique<Processor>();
 auto& reference=*referenceOwner;auto& candidate=*candidateOwner;juce::String error;
 check(!candidate.ksTwoCoreProcessing(),"Two-core processing enabled by default");
 check(candidate.setKSTwoCoreProcessing(true,error),error);
 {Processor fresh;check(fresh.ksTwoCoreProcessing(),"New instance lost two-core preference");}
 check(candidate.setKSTwoCoreProcessing(false,error),error);
 std::unique_ptr<juce::AudioProcessorEditor> editor(candidate.createEditor());
 auto* setup=dynamic_cast<juce::TextButton*>(find(*editor,"pageSetup"));check(setup!=nullptr,"Missing Setup");setup->onClick();
 auto* toggle=dynamic_cast<juce::ToggleButton*>(find(*editor,"ksTwoCoreProcessing"));
 auto* notice=dynamic_cast<juce::Label*>(find(*editor,"ksTwoCoreNotice"));
 check(toggle && toggle->isVisible() && toggle->isEnabled() && toggle->getButtonText().containsIgnoreCase("experimental"),"Missing experimental checkbox");
 check(notice && notice->isVisible() && notice->getText().contains("not guaranteed"),"Missing visible performance notice");
 const auto capture=juce::SystemStats::getEnvironmentVariable("SILVER_DSP_CAPTURE",{});
 if(capture.isNotEmpty()){auto file=juce::File(capture);check(file.getParentDirectory().createDirectory().wasOk(),"Create capture folder");auto out=file.createOutputStream();check(out && juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*out),"Capture Setup");}
 if(argc<3){std::cout<<"PASS: default, preference, Setup checkbox and warning; audio not exercised (supply KS firmware and data MIDI)\n";return 0;}
 for(auto* p:{&reference,&candidate}){
  check(p->setKSControllerMode(1,error),error);check(p->setFlashRom(juce::File(argv[2]),error),error);
  if(argc>=4)check(p->setWaveRom(juce::File(argv[3]),error),error);
  p->prepareToPlay(48000,256);p->setNonRealtime(true);check(p->loadFirmware(juce::File(argv[1]),error),error);
 }
 const auto revision=candidate.firmwareLoadRevision();const int latency=candidate.getLatencySamples();
 check(candidate.setPerformanceDiagnostics(1),"Start sequential diagnostics");
 float peak=0;uint64_t samples=0;
 for(int block=0;block<96;++block){
  if(block==4 || block==12 || block==20 || block==32 || block==40 || block==56){
   const bool enabled=block==4 || block==20 || block==40;
   toggle->setToggleState(enabled,juce::dontSendNotification);toggle->onClick();
   check(candidate.ksTwoCoreProcessing()==enabled,"Checkbox did not apply");
   check(report(candidate)["dsp_execution_backend"].toString()==(enabled?"two-workers-slice-barrier":"sequential"),"Worker did not acknowledge execution mode");
   check(report(reference)["dsp_execution_backend"].toString()=="sequential","Instance changed another engine");
   check(candidate.firmwareLoadRevision()==revision && candidate.getLatencySamples()==latency,"DSP switch rebooted firmware or changed latency");
   if(block==4){auto r=report(candidate);check(!bool(r["recording"]) && r["stop_reason"].toString()=="dsp_execution_changed" && r["session"]["dsp_execution_backend"].toString()=="sequential","Mixed diagnostic modes");}
   if(block==20){check(candidate.setPerformanceDiagnostics(1),"Start parallel diagnostics");check(report(candidate)["session"]["dsp_execution_backend"].toString()=="two-workers-slice-barrier","Wrong diagnostic session backend");}
  }
  juce::MidiBuffer midi;
  if(block==0)midi.addEvent(juce::MidiMessage::noteOn(1,60,juce::uint8(100)),0);
  if(block==6)midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0);
  if(block==8)midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
  if(block==16)midi.addEvent(juce::MidiMessage::noteOn(1,67,juce::uint8(90)),0);
  if(block==28)midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);
  if(block==48)midi.addEvent(juce::MidiMessage::noteOff(1,67),0);
  juce::AudioBuffer<float> a(4,256),b(4,256);a.clear();b.clear();juce::MidiBuffer copy(midi);
  reference.processBlock(a,midi);candidate.processBlock(b,copy);
  for(int c=0;c<4;++c)for(int n=0;n<256;++n){const float x=a.getSample(c,n),y=b.getSample(c,n);check(std::isfinite(y) && x==y,"Switch changed audio");peak=std::max(peak,std::abs(y));++samples;}
 }
 check(peak>1e-5f && reference.underruns()==0 && candidate.underruns()==0,"Silent or underrunning comparison");
 check(candidate.setKSTwoCoreProcessing(true,error),error);juce::MemoryBlock saved;candidate.getStateInformation(saved);
 check(candidate.setKSTwoCoreProcessing(false,error),error);candidate.setStateInformation(saved.getData(),int(saved.getSize()));
 check(candidate.firmwareReady() && candidate.ksTwoCoreProcessing() && report(candidate)["dsp_execution_backend"].toString()=="two-workers-slice-barrier","Project did not restore parallel mode");
 check(!bool(juce::JSON::parse(settings)["ksTwoCoreProcessing"]),"Project changed new-instance default");
 auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));check(bool(xml),"Read saved project");
 xml->removeAttribute("ksTwoCoreProcessing");juce::AudioProcessor::copyXmlToBinary(*xml,saved);candidate.setStateInformation(saved.getData(),int(saved.getSize()));
 check(candidate.firmwareReady() && !candidate.ksTwoCoreProcessing() && report(candidate)["dsp_execution_backend"].toString()=="sequential","Legacy project did not restore sequential mode");
 std::cout<<"PASS: "<<samples<<" exact float samples across six live UI switches; held notes/sustain, mode acknowledgement, instance isolation, diagnostics, preferences and project/legacy restore\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
