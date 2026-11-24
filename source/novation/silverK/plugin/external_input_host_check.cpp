#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#if JUCE_MAC
#include <AudioToolbox/AudioToolbox.h>
#endif

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static float inputSample(uint64_t position){return float(0.1*std::sin(2*3.14159265358979323846*997*position/48000.));}
static constexpr unsigned NoteBlock=8, ReleaseBlock=120, CheckBlocks=230;
template<class Render>
static void noteCheck(const char* format,Render render){
    double energy=0;unsigned count=0;
    for(unsigned i=0;i<CheckBlocks;++i){
        const auto channels=render(i);
        for(const auto* samples:channels)for(unsigned n=0;n<256;++n){
            const double v=samples[n];require(std::isfinite(v),"nonfinite note render");
            if(i>NoteBlock+20 && i<ReleaseBlock){energy+=v*v;++count;}
        }
    }
    const auto rms=std::sqrt(energy/count);
    std::cout<<format<<" note_rms="<<rms<<'\n';
    require(rms>0.0001,"expected audible note from the command controller");
}
static void checkCommandState(const void* data,int size){
    const auto xml=juce::AudioProcessor::getXmlFromBinary(data,size);
    require(xml && xml->hasTagName("SilverParameters"),"saved Silver-K state");
    juce::MemoryBlock archive;
    require(archive.fromBase64Encoding(xml->getStringAttribute(xml->getBoolAttribute("kModel")?"kCommandState":"ksCommandState"))
        && archive.getSize()>6 && !std::memcmp(archive.getData(),"SKCMD",5),"native command archive");
}
static void vst3(const char* path,const juce::MemoryBlock& state,bool notes){
    juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions,path);require(descriptions.size()==1,"VST3 scan");
    juce::String error;auto plugin=format.createInstanceFromDescription(*descriptions[0],48000,256,error);
    require(bool(plugin),error.toRawUTF8());
    juce::XmlElement wrapper("VST3PluginState");wrapper.createNewChildElement("IComponent")->addTextElement(state.toBase64Encoding());
    juce::MemoryBlock wrapped;juce::AudioProcessor::copyXmlToBinary(wrapper,wrapped);
    plugin->setStateInformation(wrapped.getData(),int(wrapped.getSize()));
    auto layout=plugin->getBusesLayout();require(layout.inputBuses.size()==1,"VST3 input bus missing");
    layout.inputBuses.set(0,juce::AudioChannelSet::mono());require(plugin->setBusesLayout(layout),"VST3 mono input");
    plugin->prepareToPlay(48000,256);plugin->setNonRealtime(true);
    juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
    if(notes){
        noteCheck("VST3",[&](unsigned block){
            audio.clear();midi.clear();
            if(block==NoteBlock)midi.addEvent(juce::MidiMessage::noteOn(1,60,juce::uint8(100)),13);
            if(block==ReleaseBlock)midi.addEvent(juce::MidiMessage::noteOff(1,60),5);
            plugin->processBlock(audio,midi);
            return std::array<const float*,2>{audio.getReadPointer(0),audio.getReadPointer(1)};
        });
        juce::MemoryBlock saved;plugin->getStateInformation(saved);
        const auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));
        const auto* component=xml?xml->getChildByName("IComponent"):nullptr;
        juce::MemoryBlock raw;
        require(component && raw.fromBase64Encoding(component->getAllSubText()),"saved VST3 component state");
        checkCommandState(raw.getData(),int(raw.getSize()));
        return;
    }
    std::array<double,2> rms{};
    for(unsigned mode=0;mode<2;++mode){double energy=0;unsigned count=0;
        for(unsigned at=0;at<48000*3;at+=256){
            audio.clear();if(!mode)for(unsigned n=0;n<256;++n)audio.setSample(0,int(n),inputSample(at+n));
            plugin->processBlock(audio,midi);
            for(int c=0;c<2;++c)for(int n=0;n<256;++n){const double v=audio.getSample(c,n);require(std::isfinite(v),"VST3 nonfinite");if(at>48000){energy+=v*v;++count;}}
        }
        rms[mode]=std::sqrt(energy/count);
    }
    std::cout<<"VST3 signal_rms="<<rms[0]<<" silence_rms="<<rms[1]<<'\n';
    require(rms[0]>0.0001 && rms[1]<rms[0]*0.001,"VST3 external input did not pass/mute");
}
#if JUCE_MAC
struct AUInput {
    bool signal=true;unsigned calls=0;
    static OSStatus render(void* data,AudioUnitRenderActionFlags*,const AudioTimeStamp* time,UInt32,UInt32 frames,AudioBufferList* buffers){
        auto& self=*static_cast<AUInput*>(data);++self.calls;
        for(unsigned c=0;c<buffers->mNumberBuffers;++c){auto* out=static_cast<float*>(buffers->mBuffers[c].mData);
            if(!out)return kAudio_ParamError;
            for(unsigned n=0;n<frames;++n)out[n]=self.signal?inputSample(uint64_t(time->mSampleTime)+n):0;
        }
        return noErr;
    }
};
static void au(const char* path,const juce::MemoryBlock& state,bool notes,bool registered){
    CFBundleRef bundle=nullptr;
    AudioComponent component=nullptr;
    if(registered){
        AudioComponentDescription description{kAudioUnitType_MusicDevice,'Tskv','TusP',0,0};
        component=AudioComponentFindNext(nullptr,&description);
    }else{
        auto file=juce::File(path).getFullPathName();
        auto url=CFURLCreateFromFileSystemRepresentation(nullptr,reinterpret_cast<const UInt8*>(file.toRawUTF8()),file.getNumBytesAsUTF8(),true);
        bundle=CFBundleCreate(nullptr,url);CFRelease(url);require(bundle&&CFBundleLoadExecutable(bundle),"AU bundle");
        auto factory=reinterpret_cast<AudioComponentFactoryFunction>(CFBundleGetFunctionPointerForName(bundle,CFSTR("Silver_KAUFactory")));require(factory,"AU factory");
        AudioComponentDescription description{kAudioUnitType_MusicDevice,'SkXi','TusP',0,0};
        component=AudioComponentRegister(&description,CFSTR("Silver-K private input check"),0x100,factory);
    }
    AudioUnit unit=nullptr;require(component&&AudioComponentInstanceNew(component,&unit)==noErr,"AU instance");
    struct Cleanup{AudioUnit unit;CFBundleRef bundle;~Cleanup(){AudioUnitUninitialize(unit);AudioComponentInstanceDispose(unit);if(bundle)CFRelease(bundle);}}cleanup{unit,bundle};
    CFPropertyListRef previous=nullptr;UInt32 size=sizeof(previous);
    require(AudioUnitGetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&previous,&size)==noErr,"AU state read");
    auto dictionary=CFDictionaryCreateMutableCopy(nullptr,0,static_cast<CFDictionaryRef>(previous));CFRelease(previous);
    auto data=CFDataCreate(nullptr,static_cast<const UInt8*>(state.getData()),state.getSize());CFDictionarySetValue(dictionary,CFSTR("jucePluginState"),data);CFRelease(data);
    auto status=AudioUnitSetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&dictionary,sizeof(dictionary));CFRelease(dictionary);require(status==noErr,"AU state restore");
    AudioStreamBasicDescription format{};format.mSampleRate=48000;format.mFormatID=kAudioFormatLinearPCM;format.mFormatFlags=kAudioFormatFlagsNativeFloatPacked|kAudioFormatFlagIsNonInterleaved;
    format.mBytesPerPacket=format.mBytesPerFrame=4;format.mFramesPerPacket=1;format.mBitsPerChannel=32;format.mChannelsPerFrame=1;
    require(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&format,sizeof(format))==noErr,"AU mono input format");
    format.mChannelsPerFrame=2;
    require(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,0,&format,sizeof(format))==noErr,"AU output format");
    UInt32 maximum=256,offline=1;
    require(AudioUnitSetProperty(unit,kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,0,&maximum,sizeof(maximum))==noErr,"AU max block");
    require(AudioUnitSetProperty(unit,kAudioUnitProperty_OfflineRender,kAudioUnitScope_Global,0,&offline,sizeof(offline))==noErr,"AU offline mode");
    AUInput source;AURenderCallbackStruct callback{AUInput::render,&source};
    require(AudioUnitSetProperty(unit,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&callback,sizeof(callback))==noErr,"AU input callback");
    require(AudioUnitInitialize(unit)==noErr,"AU initialize");
    std::array<std::array<float,256>,2> audio{};std::array<double,2> rms{};
    if(notes){
        source.signal=false;
        noteCheck("AU",[&](unsigned block){
            if(block==NoteBlock)require(MusicDeviceMIDIEvent(unit,0x90,60,100,13)==noErr,"AU note on");
            if(block==ReleaseBlock)require(MusicDeviceMIDIEvent(unit,0x80,60,0,5)==noErr,"AU note off");
            struct Buffers{UInt32 count;AudioBuffer buffers[2];}buffers{2,{{1,1024,audio[0].data()},{1,1024,audio[1].data()}}};
            AudioTimeStamp time{};time.mFlags=kAudioTimeStampSampleTimeValid;time.mSampleTime=block*256;AudioUnitRenderActionFlags flags=0;
            require(AudioUnitRender(unit,&flags,&time,0,256,reinterpret_cast<AudioBufferList*>(&buffers))==noErr,"AU note render");
            return std::array<const float*,2>{static_cast<float*>(buffers.buffers[0].mData),static_cast<float*>(buffers.buffers[1].mData)};
        });
        CFPropertyListRef saved=nullptr;size=sizeof(saved);
        require(AudioUnitGetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&saved,&size)==noErr,"AU saved state");
        auto archive=static_cast<CFDataRef>(CFDictionaryGetValue(static_cast<CFDictionaryRef>(saved),CFSTR("jucePluginState")));
        require(archive!=nullptr,"AU processor state");
        checkCommandState(CFDataGetBytePtr(archive),int(CFDataGetLength(archive)));
        CFRelease(saved);
        return;
    }
    for(unsigned mode=0;mode<2;++mode){source.signal=!mode;double energy=0;unsigned count=0;
        for(unsigned at=0;at<48000*3;at+=256){
            struct Buffers{UInt32 count;AudioBuffer buffers[2];}buffers{2,{{1,1024,audio[0].data()},{1,1024,audio[1].data()}}};
            AudioTimeStamp time{};time.mFlags=kAudioTimeStampSampleTimeValid;time.mSampleTime=mode*48000*3+at;AudioUnitRenderActionFlags flags=0;
            require(AudioUnitRender(unit,&flags,&time,0,256,reinterpret_cast<AudioBufferList*>(&buffers))==noErr,"AU render");
            for(const auto& channel:audio)for(float v:channel){require(std::isfinite(v),"AU nonfinite");if(at>48000){energy+=double(v)*v;++count;}}
        }
        rms[mode]=std::sqrt(energy/count);
    }
    std::cout<<"AU input_callbacks="<<source.calls<<" signal_rms="<<rms[0]<<" silence_rms="<<rms[1]<<'\n';
    require(source.calls>0&&rms[0]>0.0001&&rms[1]<rms[0]*0.001,"AU external input did not pass/mute");
}
#endif
int main(int argc,char** argv){try{
    const bool registry=argc==5 && !std::strcmp(argv[4],"--registered-note");
    require(argc==4 || (argc==5 && (!std::strcmp(argv[4],"--note") || registry)),"Usage: silver_external_host_check vst3|au bundle processor.state [--note|--registered-note]");juce::ScopedJuceInitialiser_GUI gui;
    juce::MemoryBlock state;require(juce::File(argv[3]).loadFileAsData(state),"Read private state");
    const juce::String format(argv[1]);
    if(format=="vst3")vst3(argv[2],state,argc==5);
#if JUCE_MAC
    else if(format=="au")au(argv[2],state,argc==5,registry);
#endif
    else require(false,"Unsupported format");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
