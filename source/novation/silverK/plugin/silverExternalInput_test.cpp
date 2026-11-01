#include "silverProcessor.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {thread_local bool inAudio=false;thread_local size_t allocations=0;}
void* operator new(size_t n){if(inAudio)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{std::free(p);}
using silverK::Processor;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void set(Processor& p,const char* id,int value){
    auto* parameter=p.uiParameter(id);
    check(parameter!=nullptr,id);parameter->setValueNotifyingHost(parameter->convertTo0to1(float(value)));
}
static void layout(Processor& p,int channels){
    auto buses=p.getBusesLayout();buses.inputBuses.set(0,channels==2?juce::AudioChannelSet::stereo():channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::disabled());
    check(p.setBusesLayout(buses),"External input bus layout rejected");
}
static double render(Processor& p,double rate,int block,float left,float right,unsigned seconds){
    juce::AudioBuffer<float> audio(4,block);juce::MidiBuffer midi;double sum=0;unsigned count=0;
    static unsigned captureIndex=0;
    const auto directory=juce::SystemStats::getEnvironmentVariable("SILVER_EXTERNAL_PCM_DIR",{});
    std::ofstream capture;
    if(directory.isNotEmpty()){
        const auto folder=juce::File(directory);check(folder.createDirectory().wasOk(),"Create private PCM directory");
        capture.open(folder.getChildFile(juce::String(captureIndex++)+".f32").getFullPathName().toStdString(),std::ios::binary);
        check(bool(capture),"Open private PCM capture");
    }
    for(unsigned at=0;at<unsigned(rate)*seconds;at+=unsigned(block)){
        audio.clear();for(int i=0;i<block;++i){const auto value=float(std::sin(2*3.14159265358979323846*997*(at+i)/rate));audio.setSample(0,i,left*value);audio.setSample(1,i,right*value);}
        inAudio=true;p.processBlock(audio,midi);inAudio=false;
        check(allocations==0,"C++ allocation in external input callback");
        if(capture.is_open())for(int c=0;c<2;++c)capture.write(reinterpret_cast<const char*>(audio.getReadPointer(c)),block*sizeof(float));
        for(int c=0;c<2;++c)for(int i=0;i<block;++i){const double v=audio.getSample(c,i);check(std::isfinite(v),"Non-finite external output");if(at>unsigned(rate)*seconds/2){sum+=v*v;++count;}}
    }
    check(!capture.is_open()||bool(capture),"Write private PCM capture");
    check(p.firmwareReady(),"Engine stopped while rendering input");return std::sqrt(sum/std::max(1u,count));
}
static void note(Processor& p,int block,bool on){
    juce::AudioBuffer<float> audio(4,block);audio.clear();juce::MidiBuffer midi;
    midi.addEvent(on?juce::MidiMessage::noteOn(1,60,juce::uint8(100)):juce::MidiMessage::noteOff(1,60),0);
    inAudio=true;p.processBlock(audio,midi);inAudio=false;check(!allocations,"MIDI/input callback allocated");
}
int main(int argc,char** argv){try{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool standalone=juce::SystemStats::getEnvironmentVariable("SILVER_EXTERNAL_STANDALONE",{}).isNotEmpty();
    if(standalone){juce::PluginHostType::jucePlugInClientCurrentWrapperType=juce::AudioProcessor::wrapperType_Standalone;juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);}
    if(argc!=5){Processor p;if(standalone)check(p.getTotalNumInputChannels()==1,"Standalone input must be enabled by default");layout(p,0);layout(p,1);layout(p,2);std::cout<<"PASS: disabled/mono/stereo input layouts; supply K ROM, KS ROM, Flash and Wave for private audio checks\n";return 0;}
    const double rate=juce::SystemStats::getEnvironmentVariable("SILVER_EXTERNAL_RATE","48000").getDoubleValue();
    const int block=juce::SystemStats::getEnvironmentVariable("SILVER_EXTERNAL_BLOCK","256").getIntValue();
    for(int model:{1,2}){
        const auto selected=juce::SystemStats::getEnvironmentVariable("SILVER_EXTERNAL_MODEL",{});
        if(selected.isNotEmpty() && selected!=(model==1?"K":"KS"))continue;
        Processor p;if(standalone)check(p.getTotalNumInputChannels()==1,"Standalone input must be enabled by default");layout(p,1);p.prepareToPlay(rate,block);p.setNonRealtime(true);
        p.flashFile=juce::File(argv[3]);p.bankFile=juce::File(argv[4]);juce::String error;
        const bool reference=juce::SystemStats::getEnvironmentVariable("SILVER_EXTERNAL_KS_H8",{}).isNotEmpty();
        check(p.setKSControllerMode(reference?0:1,error),error.toRawUTF8());
        check(p.loadFirmware(juce::File(argv[model]),error),error.toRawUTF8());
        check(juce::JSON::parse(p.performanceReport())["controller_backend"].toString()==(reference&&model==2?"original-h8":"native-cpp-semantic"),"Product did not select requested controller");
        if(model==2 && juce::SystemStats::getEnvironmentVariable("SILVER_EXTERNAL_MODE_CYCLE",{}).isNotEmpty()){
            check(p.setPerformanceMode(true,error),error.toRawUTF8());render(p,rate,block,0,0,1);
            check(p.setPerformanceMode(false,error),error.toRawUTF8());render(p,rate,block,0,0,1);
        }
        if(model==2 && juce::SystemStats::getEnvironmentVariable("SILVER_EXTERNAL_PANIC",{}).isNotEmpty()){
            for(unsigned channel=0;channel<16;++channel)for(unsigned cc:{64,123,120})check(p.sendShortMidi(uint8_t(0xb0|channel),uint8_t(cc),0),"Queue all-notes-off");
            render(p,rate,block,0,0,1);
        }
        for(auto id:{"osc1_level","osc2_level","osc3_level","noise_level","ring_level","delay_level","reverb_level","chorus_level","vocoder_balance"})set(p,id,0);
        set(p,"external_level",127);set(p,"external_to_fx",1);set(p,"volume",127);
        const auto quiet=render(p,rate,block,0,0,2);
        const auto mono=render(p,rate,block,0.1f,0,2);
        const auto tail=render(p,rate,block,0,0,2);
        layout(p,2);const auto right=render(p,rate,block,0,0.2f,2);
        const auto cancel=render(p,rate,block,0.1f,-0.1f,2);
        layout(p,0);const auto disabled=render(p,rate,block,0.1f,0.1f,2);
        std::cout<<(model==1?"K":"KS")<<" standalone="<<standalone<<" rate="<<rate<<" block="<<block<<" quiet="<<quiet<<" mono="<<mono<<" tail="<<tail<<" right="<<right<<" cancel="<<cancel<<" disabled="<<disabled<<'\n';
        check(mono>0.0001 && right>0.0001,"External signal did not reach DSP output");
        check(std::abs(right-mono)<mono*0.1,"Stereo mono fold-down gain");
        check(juce::JSON::parse(p.performanceReport())["input_overflows"].toString().getLargeIntValue()==0,"External input queue overflow");
        check(quiet<mono*0.001 && tail<mono*0.001 && cancel<mono*0.001 && disabled<mono*0.001,"External input mute/disconnect/cancellation leaked signal");
        juce::MemoryBlock state;p.getStateInformation(state);
        const auto saved=juce::SystemStats::getEnvironmentVariable("SILVER_EXTERNAL_STATE_DIR",{});
        if(saved.isNotEmpty())check(juce::File(saved).getChildFile(model==1?"K.state":"KS.state").replaceWithData(state.getData(),state.getSize()),"Save external-input host fixture");
        if(juce::SystemStats::getEnvironmentVariable("SILVER_EXTERNAL_EXTENDED",{}).isNotEmpty()){
            layout(p,1);set(p,"external_to_fx",0);set(p,"amp_attack",0);set(p,"amp_decay",0);set(p,"amp_sustain",127);set(p,"amp_release",0);
            set(p,"cutoff",127);set(p,"resonance",0);set(p,"filter_env",64);set(p,"filter_keytrack",0);set(p,"arp_on",0);
            render(p,rate,block,0,0,1);note(p,block,true);
            const auto filtered=render(p,rate,block,0.1f,0,2);
            note(p,block,false);const auto released=render(p,rate,block,0.1f,0,2);
            // Centre balance selects the vocoded carrier; 127 is raw modulator.
            set(p,"osc1_wave",2);set(p,"osc1_level",127);set(p,"vocoder_balance",64);
            // The firmware applies edit MIDI asynchronously. Settle this test
            // program before testing the audio path, then play its carrier.
            render(p,rate,block,0,0,1);note(p,block,true);
            const auto vocoder=render(p,rate,block,0.1f,0,2);
            const auto vocoderMuted=render(p,rate,block,0,0,2);note(p,block,false);
            if(model==2){
                juce::MemoryBlock snapshot;p.getStateInformation(snapshot);
                auto xml=juce::AudioProcessor::getXmlFromBinary(snapshot.getData(),int(snapshot.getSize()));
                juce::MemoryBlock program;check(xml&&program.fromBase64Encoding(xml->getStringAttribute("program")),"Read tested program");
                const auto* values=static_cast<const uint8_t*>(program.getData())+13;
                std::cout<<"KS actual";for(unsigned offset:{0,37,42,89,123,124,179,180,181,182,183,189,190})std::cout<<' '<<offset<<'='<<unsigned(values[offset]);std::cout<<'\n';
            }
            std::cout<<(model==1?"K":"KS")<<" filtered="<<filtered<<" released="<<released<<" vocoder="<<vocoder<<" vocoder_muted="<<vocoderMuted<<'\n';
            check(filtered>0.0001 && released<filtered*0.001,"External input filter/envelope path");
            check(vocoder>0.0001 && vocoderMuted<vocoder*0.1,"External input vocoder path");
            if(model==2){
                set(p,"arp_latch",1);render(p,rate,block,0,0,1);
                juce::MemoryBlock before,after;p.getStateInformation(before);
                p.setStateInformation(before.getData(),int(before.getSize()));check(p.firmwareReady(),"KS cold state restore");
                p.getStateInformation(after);
                auto first=juce::AudioProcessor::getXmlFromBinary(before.getData(),int(before.getSize()));
                auto second=juce::AudioProcessor::getXmlFromBinary(after.getData(),int(after.getSize()));
                juce::MemoryBlock a,b;
                check(first&&second&&a.fromBase64Encoding(first->getStringAttribute("program"))&&b.fromBase64Encoding(second->getStringAttribute("program")),"KS restored program read");
                check(a.getSize()==270&&b.getSize()==270,"KS Single state fixture");
                check(!std::memcmp(static_cast<const uint8_t*>(a.getData())+13,static_cast<const uint8_t*>(b.getData())+13,187),"KS cold cleanup changed stored program/arp latch");
            }
        }
    }
    std::cout<<"PASS: native K/KS external input, silence/disconnect, stereo gain/cancellation, zero callback C++ allocations\n";return 0;
}catch(const std::exception& e){inAudio=false;std::cerr<<e.what()<<'\n';return 1;}}
