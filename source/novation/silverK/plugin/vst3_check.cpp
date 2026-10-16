#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <fstream>
#if JUCE_MAC
#include "dsp56kBase/threadtools.h"
#endif
#include "wrapperClock_test.h"
int main(int argc,char** argv){if(argc!=5 && argc!=6)return 2;const bool live=argc==6 && juce::String(argv[5])=="live";const bool multi=argc==6 && juce::String(argv[5])=="multi";const int channels=multi?4:2;juce::ScopedJuceInitialiser_GUI gui;juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> descriptions;format.findAllTypesForFile(descriptions,argv[1]);if(descriptions.size()!=1)return 3;juce::String error;auto plugin=format.createInstanceFromDescription(*descriptions[0],48000,256,error);if(!plugin){std::cerr<<error<<std::endl;return 4;}
 {std::unique_ptr<juce::AudioProcessorEditor> e(plugin->createEditorIfNeeded());if(!e)return 5;}
 bool kModel=false;juce::MemoryBlock state;if(!juce::File(argv[2]).loadFileAsData(state))return 6;if(auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));xml && xml->hasTagName("SilverParameters")){kModel=xml->getBoolAttribute("kModel");juce::XmlElement wrapper("VST3PluginState");wrapper.createNewChildElement("IComponent")->addTextElement(state.toBase64Encoding());if(argc==6 && !live && !multi){
  const auto wave=std::stoi(argv[5]);juce::MemoryBlock program;program.fromBase64Encoding(xml->getStringAttribute("program"));
  if(program.getSize()!=270)return 10;auto* b=static_cast<uint8_t*>(program.getData());b[13+128]=uint8_t(wave);for(int offset:{38,39,40,41,100,107,110,65})b[13+offset]=0;
  xml->setAttribute("program",program.toBase64Encoding());
  for(auto* child:xml->getChildIterator()){const auto id=child->getStringAttribute("id");if(id=="osc1_wave")child->setAttribute("value",wave);if(id=="osc2_level" || id=="osc3_level" || id=="delay" || id=="reverb" || id=="chorus" || id=="release")child->setAttribute("value",0);}
  juce::AudioProcessor::copyXmlToBinary(*xml,state);wrapper.getChildByName("IComponent")->deleteAllChildElements();wrapper.getChildByName("IComponent")->addTextElement(state.toBase64Encoding());
 }
 juce::MemoryBlock wrapped;juce::AudioProcessor::copyXmlToBinary(wrapper,wrapped);plugin->setStateInformation(wrapped.getData(),int(wrapped.getSize()));}else plugin->setStateInformation(state.getData(),int(state.getSize()));if(multi){auto layout=plugin->getBusesLayout();if(layout.outputBuses.size()!=2)return 15;layout.outputBuses.set(1,juce::AudioChannelSet::stereo());if(!plugin->setBusesLayout(layout))return 16;}plugin->prepareToPlay(48000,256);const bool realtime=juce::String(argv[4])=="realtime";plugin->setNonRealtime(!realtime);{std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorIfNeeded());if(editor){juce::PNGImageFormat png;auto file=juce::File(juce::String(argv[3])+".png");file.deleteFile();auto stream=file.createOutputStream();if(stream)png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*stream);}}
 if(plugin->getParameters()[1]->getNumSteps()!=32)return 9;
 if(plugin->getParameters().size()<16)return 9;
 if(argc==6 && !live && !multi){auto wave=std::stoi(argv[5]);plugin->getParameters()[1]->setValueNotifyingHost(float(wave)/31);for(int n:{5,6,13,14,15})plugin->getParameters()[n]->setValueNotifyingHost(0);plugin->getParameters()[12]->setValueNotifyingHost(0);}

 WrapperClock clock;if(clock.enabled())plugin->setPlayHead(&clock);
 const bool hostAudioPriority=juce::SystemStats::getEnvironmentVariable("SILVER_TEST_HOST_AUDIO_PRIORITY","0")=="1";
 if(hostAudioPriority){
#if JUCE_MAC
  if(!realtime || !dsp56k::ThreadTools::setCurrentThreadPriority(dsp56k::ThreadPriority::Highest)
     || !dsp56k::ThreadTools::setCurrentThreadRealtimeParameters(48000,256))return 20;
#else
  return 20; // Optional audio-priority host model is currently macOS-only.
#endif
 }
 std::cout<<"host_audio_priority="<<hostAudioPriority<<std::endl;
 juce::AudioBuffer<float> a(channels,256),capture(channels,192000);std::array<double,4> channelPeaks{};juce::MidiBuffer m;m.ensureSize(4096);double peak=0,maxms=0,pre=0,note=0,tail=0;int onset=-1;
 if(clock.enabled()){WrapperClock::setupParameters(*plugin,kModel);const auto warmStart=std::chrono::steady_clock::now();for(int n=0;n<(kModel?3000:6000);++n){plugin->processBlock(a,m);m.clear();clock.advance();if(realtime)std::this_thread::sleep_until(warmStart+std::chrono::microseconds(int64_t((n+1)*256*1000000.0/48000)));}}
 const int64_t warmUnderruns=clock.enabled()?WrapperClock::underruns(*plugin):0;
 auto begin=std::chrono::steady_clock::now();
 for(int pos=0;pos<192000;pos+=256){int n=std::min(256,192000-pos);a.setSize(channels,n,false,false,true);a.clear();m.clear();if(pos<=24000 && pos+n>24000){if(multi)for(auto ch:{1,2,9,4})m.addEvent(juce::MidiMessage::noteOn(ch,ch==4?90:69,juce::uint8(100)),24000-pos);else m.addEvent(juce::MidiMessage::noteOn(1,69,juce::uint8(100)),24000-pos);}if(pos<=((live || clock.enabled())?168000:72000) && pos+n>((live || clock.enabled())?168000:72000)){if(multi)for(auto ch:{1,2,9,4})m.addEvent(juce::MidiMessage::noteOff(ch,ch==4?90:69),((live || clock.enabled())?168000:72000)-pos);else m.addEvent(juce::MidiMessage::noteOff(1,69),((live || clock.enabled())?168000:72000)-pos);}
 if(live){for(const auto event:std::array<std::array<int,3>,4>{{{48000,7,20},{96000,7,127},{120000,4,0},{144000,4,127}}})if(pos<=event[0] && pos+n>event[0])plugin->getParameters()[event[1]]->setValueNotifyingHost(event[2]/127.f);}
 if(clock.enabled() && clock.editPulse && (pos/256)%2==0)WrapperClock::pulse(*plugin,kModel,((pos/256)*7)%40+44);
 auto t=std::chrono::steady_clock::now();plugin->processBlock(a,m);clock.advance();maxms=std::max(maxms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());for(int c=0;c<channels;++c){capture.copyFrom(c,pos,a,c,0,n);for(int i=0;i<n;++i){auto v=double(a.getSample(c,i));if(!std::isfinite(v))return 17;channelPeaks[c]=std::max(channelPeaks[c],std::abs(v));peak=std::max(peak,std::abs(v));if(onset<0 && pos+i>=24000 && std::abs(v)>0.001)onset=pos+i;if(pos+i<23000)pre+=v*v;if(pos+i>=30000 && pos+i<65000)note+=v*v;if(pos+i>=150000)tail+=v*v;}}
 if(realtime)std::this_thread::sleep_until(begin+std::chrono::microseconds(int64_t((pos+n)*1000000.0/48000)));}
 const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
 juce::WavAudioFormat f;auto stream=juce::File(argv[3]).createOutputStream();auto w=std::unique_ptr<juce::AudioFormatWriter>(f.createWriterFor(stream.release(),48000,channels,24,{},0));if(!w || !w->writeFromAudioSampleBuffer(capture,0,192000))return 7;

 juce::MemoryBlock saved;plugin->getStateInformation(saved);juce::File(juce::String(argv[3])+".state").replaceWithData(saved.getData(),saved.getSize());int64_t underruns=-1;
 if(auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize())))if(auto* component=xml->getChildByName("IComponent")) {
  juce::MemoryBlock raw;raw.fromBase64Encoding(component->getAllSubText());
  if(auto inner=juce::AudioProcessor::getXmlFromBinary(raw.getData(),int(raw.getSize()))){underruns=inner->getStringAttribute("underruns","-1").getLargeIntValue();std::cout<<"produced="<<inner->getStringAttribute("produced")<<" consumed="<<inner->getStringAttribute("consumed")<<" midi_received="<<inner->getStringAttribute("midiReceived")<<" wave_reads="<<inner->getStringAttribute("waveReads")<<" ";}
 }
 if(!clock.verify(capture))return 19;
 if(live){std::array<double,5> rms{},diff{};const std::array<std::array<int,2>,5> windows{{{36000,44000},{65000,85000},{106000,118000},{132000,141000},{153000,162000}}};for(size_t k=0;k<windows.size();++k){for(int i=windows[k][0];i<windows[k][1];++i){double v=capture.getSample(0,i),d=v-capture.getSample(0,i-1);rms[k]+=v*v;diff[k]+=d*d;}rms[k]=sqrt(rms[k]/(windows[k][1]-windows[k][0]));diff[k]=sqrt(diff[k]/(windows[k][1]-windows[k][0]));}std::cout<<"live_rms=";for(auto v:rms)std::cout<<v<<",";std::cout<<" live_diff=";for(auto v:diff)std::cout<<v<<",";std::cout<<std::endl;if(!(diff[1]<diff[0]*0.4 && diff[2]>diff[1]*2 && rms[3]<rms[0]*0.5 && rms[4]>rms[3]*2))return 11;}
 if(multi){std::cout<<"multi_channel_peaks=";for(auto value:channelPeaks){std::cout<<value<<",";if(value<0.001)return 18;}std::cout<<std::endl;}
 std::cout<<"elapsed_audio_ms="<<elapsed<<" underruns="<<underruns<<" warmup_underruns="<<warmUnderruns<<" format=VST3 mode="<<argv[4]<<" name="<<plugin->getName()<<" parameters="<<plugin->getParameters().size()<<" peak="<<peak<<" pre_rms="<<std::sqrt(pre/46000)<<" note_rms="<<std::sqrt(note/70000)<<" tail_rms="<<std::sqrt(tail/84000)<<" onset_samples="<<onset-24000<<" reported_latency="<<plugin->getLatencySamples()<<" max_callback_ms="<<maxms<<" state_bytes="<<saved.getSize()<<std::endl;return argc==6 && !live && !multi && std::stoi(argv[5])>=8 ? (peak==0 && underruns==warmUnderruns?0:8) : (peak>0.001 && pre<0.01 && onset>24000 && underruns==warmUnderruns?0:8);}
