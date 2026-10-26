// Explicit retained LEGACY comparison test; not a semantic NativeControllerExecutor proof.
#include "ksBoardComparison_test.h"
#include "ksRom.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
unsigned frame=0;
const char* phase="boot";
uint64_t lastReceived=0;bool observed=false;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> bytes(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"Cannot read fixture input");return {std::istreambuf_iterator<char>(in),{}};}
void same(ks::Board& a,ks::Board& b){
    const auto& x=static_cast<ks::ControllerCpu&>(ks::test::comparisonCpu(a));
    const auto& y=static_cast<ks::ControllerCpu&>(ks::test::comparisonCpu(b));
    check(x.cycles==y.cycles && x.getPC()==y.getPC(),"UART scheduling cycle/PC differs");
    check(x.ccr==y.ccr && x.exr==y.exr,"UART scheduling flags differ");
    for(unsigned n=0;n<8;++n)check(x.regs[n].er==y.regs[n].er,"UART scheduling register differs");
    check(x.lastread==y.lastread && x.lastwrite==y.lastwrite && x.indent==y.indent,"UART scheduling bus/stack frame differs");
    check(x.pending_irqs==y.pending_irqs && x.pending_irqs_high==y.pending_irqs_high,"UART pending IRQ differs");
    check(a.midiReceived==b.midiReceived,"UART byte delivery boundary differs");
    check(a.lastMidiReceiveCycle()==b.lastMidiReceiveCycle(),"UART byte delivery cycle differs");
    if(observed)check(a.midiReceived-lastReceived<=1,"UART observation skipped multiple bytes");
    lastReceived=a.midiReceived;observed=true;
    for(unsigned n=0xfffe00;n<=0xffffff;++n)check(a.read(n)==b.read(n),"UART/timer/IO state differs");
    if(ks::test::legacyRuntime(b))check(b.h8StepsSinceNativeHandoff()==0,"UART scheduling executed H8 after handoff");
}
}
int main(int argc,char** argv){try{
    if(argc!=5){std::cout<<"SKIP: KS OS, Flash, zero wave and fresh private output directory required\n";return 77;}
    const std::filesystem::path output(argv[4]);check(!std::filesystem::exists(output),"Refusing to reuse UART output directory");
    std::filesystem::create_directories(output);
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS firmware");const auto flash=bytes(argv[2]),wave=bytes(argv[3]);
    auto original=ks::test::makeLegacyBoard(os.data(),wave,flash,true);
    auto native=ks::test::makeLegacyBoard(os.data(),wave,flash,true);
    check(original->boot() && native->boot(),"UART fixture boot failed");same(*original,*native);
    const uint64_t bootMidiReceived=original->midiReceived;
    native->enableNativeController();
    original->startControllerTrace(1000000,200000);native->startControllerTrace(1000000,200000);
    auto send=[&](std::initializer_list<uint8_t> message){check(original->enqueueMidi(message.begin(),message.size()) && native->enqueueMidi(message.begin(),message.size()),"UART fixture rejected MIDI");};
    constexpr unsigned budgets[]{1,2,3,7,15,31,63,97,127,128};
    int64_t peak=0;uint64_t samples=0;
    for(frame=0;frame<12000;++frame){
        const unsigned at=frame%1024;const uint8_t note=uint8_t(48+(frame/1024)%12);
        if(at==80)send({0x90,note}); // Leave a channel packet partial.
        if(at==81)send({0xf8}); // Realtime may interrupt it on the wire.
        if(at==82)send({100});
        if(at==250)send({0xb0,64,127,0x80,note,0});
        if(at==500)send({0xb0,1,uint8_t(frame&127),0xe0,0,64,0xb0,64,0});
        if(frame%503==0)send({0xf8});
        if(frame==2000 || frame==7000)native->enableNativeController(false);
        if(frame==2300 || frame==7300)native->enableNativeController();
        phase="variable controller slice";const unsigned budget=budgets[frame%10];
        original->advance(budget);native->advance(budget);same(*original,*native);
        phase="native audio frame";const auto a=original->renderSample(),b=native->renderSample();
        check(a==b,"UART native audio bytes differ");same(*original,*native);
        for(unsigned channel=0;channel<4;++channel)peak=std::max(peak,std::abs(int64_t(a[channel])));
        ++samples;
    }
    check(original->midiReceived-bootMidiReceived>100 && peak>1000,"UART fixture was not admitted as active MIDI/audio");
    const auto x=original->finishControllerTrace(),y=native->finishControllerTrace();
    x->validate();y->validate();x->save((output/"h8.kst").string());y->save((output/"native.kst").string());
    check(bytes(output/"h8.kst")==bytes(output/"native.kst"),"UART full DSP bus/slice bytes differ");
    const auto& a=ks::test::comparisonCpu(*original);const auto& b=ks::test::comparisonCpu(*native);
    check(std::equal(std::begin(a.memory),std::end(a.memory),std::begin(b.memory)),"UART final controller memory differs");
    check(original->saveUserFlash()==native->saveUserFlash(),"UART final Flash differs");
    check(ks::stateMessage(original->state())==ks::stateMessage(native->state()),"UART final synth state differs");
    uint8_t v=0,w=0;while(original->popMidiOut(v))check(native->popMidiOut(w)&&v==w,"UART MIDI output differs");
    check(!native->popMidiOut(w),"UART native MIDI output has trailing bytes");
    std::cout<<"PASS: "<<samples<<" exact 24-stream frames, "<<(original->midiReceived-bootMidiReceived)
        <<" exact-cycle UART bytes after boot ("<<bootMidiReceived<<" bootstrap bytes counted separately), "<<x->events.size()<<" exact port events and "<<x->slices.size()
        <<" exact slices; variable budgets 1..128, empty-to-burst input, partial packets/realtime priority, two H8/native round trips; registers/flags/clocks/IO/RAM/Flash/state exact\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"frame="<<frame<<" phase="<<phase<<": "<<e.what()<<'\n';return 1;}}
