#include "WaveBank.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>

namespace { thread_local bool guard=false;thread_local unsigned allocations=0,deletions=0;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"Invalid input accepted");}
double rms(const std::vector<float>& x){double e=0;for(auto v:x)e+=v*v;return std::sqrt(e/x.size());}
}
void* operator new(std::size_t n){if(guard)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {if(guard&&p)++deletions;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}

int main()
{
    using namespace waveRom;
    try {
        Bank bank;size_t next=0;int eight=0;
        for(size_t i=0;i<slots().size();++i){auto s=slots()[i];check(s.offset==next,"Noncontiguous layout");next+=s.bytes();eight+=s.bits==8;check(rms(bank.decode(i))==0,"Empty slot is not silent");}
        check(next==bankSize&&eight==2,"Wrong bank geometry");
        std::vector<uint8_t> original(bankSize);uint32_t random=42;
        for(auto& v:original){random=random*1664525+1013904223;v=uint8_t(random>>24);}
        bank.load(original);for(size_t i=0;i<slots().size();++i)bank.put(i,bank.decode(i));
        check(bank.bytes()==original,"Decode/encode must preserve every PCM code and unedited ROM byte");
        bank.load(original);auto edit=bank.decode(1);edit[0]=.25f;bank.put(1,edit);
        for(size_t i=0;i<bankSize;++i)if(i<2048||i>=4096)check(bank.bytes()[i]==original[i],"Editing one slot changed another");
        bank.load(std::vector<uint8_t>(bankSize,0));check(bank.placeholder(),"Zero placeholder not recognized");check(rms(bank.decode(19))==0,"Zero placeholder must be auditioned as silence");
        check(bank.bytes()==std::vector<uint8_t>(bankSize,0),"Unchanged placeholder must roundtrip");
        bank.put(1,std::vector<float>(1024,.25f));check(!bank.placeholder()&&rms(bank.decode(19))==0,"Editing placeholder left full-scale 8-bit DC in unused slots");
        rejects([&]{bank.load({1,2,3});});
        constexpr double rate=48000,frequency=196.7,pi=3.14159265358979323846;
        std::vector<float> tone(48000);
        for(size_t i=0;i<tone.size();++i){double p=2*pi*frequency*i/rate;tone[i]=float(.5*std::sin(p)+.17*std::sin(p*2));}
        auto cycle=findCycle(tone,rate);
        check(std::abs(rate/(cycle.end-cycle.begin)-frequency)<.3,"Cycle detector chose wrong pitch or octave");
        auto prepared=prepare(tone,rate,cycle,slots()[1],{});
        double peak=0;for(auto v:prepared)peak=std::max(peak,std::abs(double(v)));
        check(prepared.size()==1024&&std::abs(peak-std::pow(10.,-.05))<.0001,"Normalization or slot fit failed");
        check(seamError(prepared)<.05,"Cycle boundary has a discontinuity");
        // Isolate the first of two hits, retaining the onset and excluding silence.
        std::vector<float> drums(48000);
        for(int first:{4800,28000})for(int i=0;i<15000&&first+i<int(drums.size());++i)drums[size_t(first+i)]=float(.7*std::exp(-i/1300.)*std::cos(i*.31));
        auto transient=findTransient(drums,rate);check(transient.begin<4800&&transient.begin>=4650&&transient.end<28000&&transient.end>9000,"Transient bounds cut attack or include the next hit");
        Processing fade;fade.fadeInMs=2;fade.fadeOutMs=10;
        auto drum=prepare(drums,rate,transient,slots()[12],fade);
        check(std::abs(drum.front())<.01&&std::abs(drum.back())<.01,"Fade endpoints are not silent");
        std::vector<float> alias(8192);for(size_t i=0;i<alias.size();++i)alias[i]=float(std::sin(2*pi*.375*i));
        check(rms(resample(alias,1024,true))<.005,"Downsampling aliases ultrasonic content");
        rejects([&]{findCycle(std::vector<float>(1000),rate);});
        rejects([&]{findTransient(std::vector<float>(1000),rate);});
        rejects([&]{prepare(tone,rate,{-1,12,0},slots()[0],{});});
        rejects([&]{auto p=Processing{};p.peakDb=0;prepare(tone,rate,cycle,slots()[0],p);});
        check(noteName(60)=="C4"&&noteHz(69)==440,"MIDI note convention changed");
        PreviewVoice voice;auto preview=tonalPreview(prepared,69,rate);check(voice.publish(std::move(preview)),"Cannot queue preview");
        std::vector<float> audio(48000),right(48000);float* channels[]{audio.data(),right.data()};
        guard=true;voice.render(channels,2,int(audio.size()),.5f);guard=false;
        check(allocations==0&&deletions==0,"Audio callback allocated or reclaimed storage");
        int crossings=0;for(size_t i=1;i<audio.size();++i)if(audio[i-1]<=0&&audio[i]>0)++crossings;
        check(std::abs(crossings-440)<=1&&audio==right,"Note audition pitch or stereo output is wrong");
        for(int n=0;n<200;++n){check(voice.publish(tonalPreview(prepared,n%80+24,rate)),"Buffer queue filled unexpectedly");guard=true;voice.render(channels,2,64,.3f);guard=false;voice.collect();}
        check(allocations==0&&deletions==0,"Buffer swaps allocated/freed in callback");
        voice.stop();guard=true;voice.render(channels,2,64,.3f);guard=false;check(!voice.playing(),"Stop failed");
        auto oneShot=samplePreview(drum,drum.size()/((transient.end-transient.begin)/rate),60,rate,false);
        check(std::abs(double(oneShot->pcm.size())-(transient.end-transient.begin))<=1,"One-shot root note changed duration");
        auto octave=samplePreview(drum,32000,72,rate,false);check(octave->pcm.size()==drum.size()*3/4,"Octave transposition did not double playback speed");
        PreviewVoice concurrent;std::atomic<bool> finish{false},safe{true};
        std::thread audioThread([&]{float l[64]{},r[64]{};float* out[]{l,r};while(!finish.load()){guard=true;concurrent.render(out,2,64,.3f);guard=false;if(allocations||deletions)safe.store(false);std::this_thread::yield();}});
        for(int i=0;i<2000;++i){auto p=std::make_unique<PreviewBuffer>();p->pcm=prepared;p->loop=true;while(!concurrent.publish(std::move(p))){p=std::make_unique<PreviewBuffer>();p->pcm=prepared;p->loop=true;std::this_thread::yield();}}
        finish.store(true);audioThread.join();concurrent.collect();check(safe.load(),"Concurrent publication reclaimed memory on the audio thread");
        std::cout<<"PASS: 24-slot layout, all PCM codes, byte-exact ROM roundtrip, isolated edit, zero placeholder, cycle/attack detection, fades, anti-aliasing, note pitch and allocation-free playback\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
