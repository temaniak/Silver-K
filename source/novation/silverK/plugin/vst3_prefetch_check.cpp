#include <juce_audio_utils/juce_audio_utils.h>
#define JUCE_VST3HEADERS_INCLUDE_HEADERS_ONLY 1
#include <juce_audio_processors/format_types/juce_VST3Headers.h>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Steinberg;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}

// ProcessData owns this list for one synchronous SDK call; the plugin does not
// retain it. No event storage is allocated while rendering.
struct Events final:Vst::IEventList {
 Vst::Event value{};
 bool present=false;
 tresult PLUGIN_API queryInterface(const TUID,void** out) override {*out=nullptr;return kNoInterface;}
 uint32 PLUGIN_API addRef() override {return 1;}
 uint32 PLUGIN_API release() override {return 1;}
 int32 PLUGIN_API getEventCount() override {return present?1:0;}
 tresult PLUGIN_API getEvent(int32 index,Vst::Event& event) override {
  if(index || !present)return kResultFalse;
  event=value;return kResultOk;
 }
 tresult PLUGIN_API addEvent(Vst::Event&) override {return kNotImplemented;}
};
struct Visitor final:juce::ExtensionsVisitor {
 Vst::IComponent* component=nullptr;
 Vst::IAudioProcessor* audio=nullptr;
 void visitVST3Client(const VST3Client& client) override {
  component=client.getIComponentPtr();
  component->queryInterface(Vst::IAudioProcessor_iid,reinterpret_cast<void**>(&audio));
 }
 ~Visitor(){if(audio)audio->release();}
};
}

// Private processor state must select an audible generated oscillator program.
// Run once with mode 1 (kPrefetch) and once with 2 (kOffline), then compare PCM.
// This intentionally calls the actual SDK at full speed without host pacing.
int main(int argc,char** argv){try{
 require(argc==5,"Usage: silver_vst3_prefetch_check bundle processor.state mode[1|2] output.f32");
 const int mode=std::stoi(argv[3]);
 require(mode==Vst::kPrefetch || mode==Vst::kOffline,"Use prefetch or offline mode");
 juce::ScopedJuceInitialiser_GUI gui;
 juce::VST3PluginFormat format;
 juce::OwnedArray<juce::PluginDescription> descriptions;
 format.findAllTypesForFile(descriptions,argv[1]);
 require(descriptions.size()==1,"Plugin scan");
 juce::String error;
 auto plugin=format.createInstanceFromDescription(*descriptions[0],48000,256,error);
 require(bool(plugin),"Plugin create");

 juce::MemoryBlock state;
 require(juce::File(argv[2]).loadFileAsData(state),"State read");
 juce::XmlElement wrapper("VST3PluginState");
 wrapper.createNewChildElement("IComponent")->addTextElement(state.toBase64Encoding());
 juce::MemoryBlock wrapped;
 juce::AudioProcessor::copyXmlToBinary(wrapper,wrapped);
 plugin->setStateInformation(wrapped.getData(),int(wrapped.getSize()));
 plugin->prepareToPlay(48000,256);

 Visitor visitor;
 plugin->getExtensions(visitor);
 require(visitor.audio && visitor.component,"SDK visitor");
 visitor.audio->setProcessing(false);
 visitor.component->setActive(false);
 Vst::ProcessSetup setup{};
 setup.processMode=mode;setup.symbolicSampleSize=Vst::kSample32;
 setup.maxSamplesPerBlock=256;setup.sampleRate=48000;
 require(visitor.audio->setupProcessing(setup)==kResultOk,"SDK setup");
 require(visitor.component->setActive(true)==kResultOk,"SDK active");
 require(visitor.audio->setProcessing(true)==kResultOk,"SDK processing");

 std::array<std::array<float,256>,2> audio{};
 float* pointers[]{audio[0].data(),audio[1].data()};
 Vst::AudioBusBuffers output{};
 output.numChannels=2;output.channelBuffers32=pointers;
 Vst::ProcessContext context{};
 context.sampleRate=48000;context.tempo=100;
 context.state=Vst::ProcessContext::kPlaying|Vst::ProcessContext::kTempoValid|Vst::ProcessContext::kProjectTimeMusicValid;
 Events events;
 Vst::ProcessData data{};
 data.processMode=mode;data.symbolicSampleSize=Vst::kSample32;
 data.numSamples=256;data.numOutputs=1;data.outputs=&output;
 data.inputEvents=&events;data.processContext=&context;
 std::ofstream out(argv[4],std::ios::binary);
 require(bool(out),"PCM output");
 double peak=0;
 uint64_t hash=1469598103934665603ull;
 for(unsigned n=0;n<192000;n+=256){
  events.present=n==0 || n==144128;
  if(events.present){
   events.value={};
   if(n==0){
    events.value.type=Vst::Event::kNoteOnEvent;
    events.value.noteOn.channel=0;events.value.noteOn.pitch=24;
    events.value.noteOn.velocity=1;events.value.noteOn.noteId=-1;
   }else{
    events.value.type=Vst::Event::kNoteOffEvent;
    events.value.noteOff.channel=0;events.value.noteOff.pitch=24;
    events.value.noteOff.noteId=-1;
   }
  }
  context.projectTimeSamples=n;context.projectTimeMusic=n/28800.;
  require(visitor.audio->process(data)==kResultOk,"SDK process");
  for(unsigned i=0;i<256;++i)for(unsigned channel=0;channel<2;++channel){
   const float sample=audio[channel][i];
   require(std::isfinite(sample),"Finite audio");
   peak=std::max(peak,double(std::abs(sample)));
   // File I/O is outside the plugin's process call, in this offline test host.
   out.write(reinterpret_cast<const char*>(&sample),sizeof(sample));
   const auto* bytes=reinterpret_cast<const uint8_t*>(&sample);
   for(unsigned j=0;j<sizeof(sample);++j){hash^=bytes[j];hash*=1099511628211ull;}
  }
 }
 require(bool(out),"PCM write");
 visitor.audio->setProcessing(false);
 visitor.component->setActive(false);
 plugin->getStateInformation(wrapped);
 const auto xml=juce::AudioProcessor::getXmlFromBinary(wrapped.getData(),int(wrapped.getSize()));
 require(xml && xml->getChildByName("IComponent"),"Saved wrapper");
 state.fromBase64Encoding(xml->getChildByName("IComponent")->getAllSubText());
 const auto inner=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));
 require(inner && inner->hasTagName("SilverParameters") && inner->hasAttribute("underruns"),"Saved processor");
 const auto missed=inner->getStringAttribute("underruns").getLargeIntValue();
 std::cout<<"mode="<<mode<<" hash="<<hash<<" peak="<<peak<<" missed="<<missed<<std::endl;
 require(peak>1e-5 && missed==0,"Audio or underruns");
 return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<std::endl;return 1;}}
