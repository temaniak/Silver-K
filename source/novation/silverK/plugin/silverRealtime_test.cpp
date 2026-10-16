#include "silverProcessor.h"
#include "dsp56kBase/threadtools.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#include <sys/resource.h>
#endif

namespace {
double processCpuSeconds(){
#if JUCE_MAC
 rusage usage{};if(getrusage(RUSAGE_SELF,&usage))return -1;
 return double(usage.ru_utime.tv_sec)+double(usage.ru_stime.tv_sec)
       +double(usage.ru_utime.tv_usec+usage.ru_stime.tv_usec)*1.e-6;
#else
 return -1; // This fixture's process CPU counter is currently macOS-only.
#endif
}
}
// Private ROMs only. Keep UI/state work off the simulated host audio thread.
int main(int argc,char** argv){
 if(argc!=5)return 2;
 juce::ScopedJuceInitialiser_GUI gui;
 const int block=juce::SystemStats::getEnvironmentVariable("SILVER_REALTIME_BLOCK","128").getIntValue();
 const bool k=juce::SystemStats::getEnvironmentVariable("SILVER_REALTIME_K","0")=="1";
 const bool sweep=juce::SystemStats::getEnvironmentVariable("SILVER_REALTIME_SWEEP","0")=="1";
 const int profile=juce::SystemStats::getEnvironmentVariable("SILVER_REALTIME_PROFILE","1").getIntValue();
 const bool save=juce::SystemStats::getEnvironmentVariable("SILVER_REALTIME_SAVE","1")=="1";
 const int prepared=juce::SystemStats::getEnvironmentVariable("SILVER_REALTIME_PREPARED_BLOCK",juce::String(block)).getIntValue();
 silverK::Processor p;p.prepareToPlay(48000,prepared);p.flashFile=juce::File(argv[3]);p.bankFile=juce::File(argv[4]);
 juce::String error;if(!p.loadFirmware(juce::File(argv[k?1:2]),error)){std::cerr<<error<<std::endl;return 3;}
 p.setBufferProfile(profile);const auto initialLatency=p.getLatencySamples();
 const auto reportPath=juce::SystemStats::getEnvironmentVariable("SILVER_PERF_REPORT",{});
 const int diagnosticMode=juce::SystemStats::getEnvironmentVariable("SILVER_PERF_MODE","1").getIntValue();
 auto set=[&](const char* id,int value){auto* parameter=p.uiParameter(id);if(parameter)parameter->setValueNotifyingHost(parameter->convertTo0to1(float(value)));};
 for(auto item:{std::pair<const char*,int>{"arp_on",0},{"amp_attack",0},{"amp_release",0},{"amp_sustain",127},{"delay_level",0},{"reverb_level",0},{"chorus_level",0},{"volume",127},{"osc1_level",127},{"osc2_level",0},{"osc3_level",0},{"filter_freq",127}})set(item.first,item.second);
 if(reportPath.isNotEmpty() && !p.setPerformanceDiagnostics(diagnosticMode)){std::cerr<<"Cannot start diagnostics\n";return 4;}
 juce::AudioBuffer<float> audio(4,block);juce::MidiBuffer midi;midi.ensureSize(4096);
 const auto before=p.underruns();std::atomic<uint64_t> warm{before};std::atomic<bool> warmCaptured{false};
 const auto beforeCpu=processCpuSeconds();const auto begin=std::chrono::steady_clock::now();
 std::atomic<bool> done{false};std::atomic<unsigned> saves{0};double maxSaveMs=0;
 std::thread stateReader([&]{
  while(!done.load()){
   std::this_thread::sleep_for(std::chrono::milliseconds(100));if(done.load())break;
   if(!warmCaptured.load() && std::chrono::steady_clock::now()-begin>=std::chrono::seconds(4)){warm=p.underruns();warmCaptured=true;}
   if(!save)continue;
   juce::MemoryBlock state;const auto call=std::chrono::steady_clock::now();p.getStateInformation(state);
   maxSaveMs=std::max(maxSaveMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-call).count());++saves;
  }
 });
 std::thread controlActions([&]{
  for(int second=1;second<=9 && !done.load();++second){
   std::this_thread::sleep_until(begin+std::chrono::seconds(second));
   if(second==6)p.prepareToPlay(48000,prepared);
   if(sweep && second==5)p.setBufferProfile(1);
   if(sweep && second==7)p.setBufferProfile(2);
   if(sweep && second==9)p.setBufferProfile(3);
  }
 });
 double maxCallback=0,peak=0;int first=-1;bool finite=true;
 const int blocks=48000*12/block,firstNote=4800/block;std::array<int,8> held{};held.fill(-1);
 std::atomic<bool> audioDone{false};std::thread audioThread([&]{
 dsp56k::ThreadTools::setCurrentThreadPriority(dsp56k::ThreadPriority::Highest);dsp56k::ThreadTools::setCurrentThreadRealtimeParameters(48000,block);
 for(int n=0;n<blocks;++n){
  if(n==firstNote)midi.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),0);
  if(n==firstNote+48000/block)midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
  if(n>firstNote+48000/block && n%std::max(1,1920/block)==0){for(int& note:held){if(note>=0)midi.addEvent(juce::MidiMessage::noteOff(1,note),0);note=36+(n*7+(&note-held.data())*5)%60;midi.addEvent(juce::MidiMessage::noteOn(1,note,uint8_t(64+note%48)),0);}}
  const auto call=std::chrono::steady_clock::now();p.processBlock(audio,midi);
  maxCallback=std::max(maxCallback,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-call).count());
  for(int i=0;i<block;++i){const auto a=std::abs(audio.getSample(0,i));finite &= std::isfinite(a);peak=std::max(peak,double(a));if(first<0 && n>=firstNote && a>1e-4)first=(n-firstNote)*block+i;}
  std::this_thread::sleep_until(begin+std::chrono::microseconds(int64_t((n+1)*block*1000000.0/48000)));
 }
 audioDone=true;
#if ! JUCE_MAC
 juce::MessageManager::getInstance()->stopDispatchLoop();
#endif
 });
 // The command-line fixture has no NSApplication delegate. Pump JUCE's
 // CoreFoundation message source on the main thread, as an AU host does.
#if JUCE_MAC
 while(!audioDone.load())CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.01,true);
#else
 juce::MessageManager::getInstance()->runDispatchLoop();
#endif
 audioThread.join();
 done=true;stateReader.join();controlActions.join();const auto after=p.underruns();
 const auto afterCpu=processCpuSeconds();
 const auto processCpu=beforeCpu>=0 && afterCpu>=0?afterCpu-beforeCpu:-1;
 const auto processWall=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
 if(reportPath.isNotEmpty()){
  p.setPerformanceDiagnostics(0);
  if(!juce::File(reportPath).replaceWithText(p.performanceReport())){std::cerr<<"Cannot save diagnostics report\n";return 4;}
 }
 std::cout<<"model="<<(k?"K":"KS")<<" prepared="<<prepared<<" block="<<block<<" save="<<save<<" profile="<<profile<<" sweep="<<sweep<<" latency_ms="<<initialLatency/48.0<<" final_latency_ms="<<p.getLatencySamples()/48.0<<" note_onset_ms="<<first/48.0<<" startup_missed_frames="<<warm.load()-before<<" steady_missed_frames="<<after-warm.load()<<" state_reads="<<saves<<" max_state_ms="<<maxSaveMs<<" max_callback_ms="<<maxCallback<<" peak="<<peak<<" process_cpu_seconds="<<processCpu<<" process_wall_seconds="<<processWall<<std::endl;
 const bool responsive=profile!=0 || prepared<=block || sweep || p.getLatencySamples()<initialLatency;
 return responsive && finite && first>=0 && peak>1e-4 && after==warm.load()?0:1;
}
