// Explicit retained LEGACY comparison test; not a semantic NativeControllerExecutor proof.
#include "ksBoardComparison_test.h"
#include "ksRom.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
std::vector<uint8_t> bytes(const char* name){std::ifstream f(name,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
}
int main(int argc,char** argv){try{
    if(argc!=4 && argc!=5){std::cout<<"SKIP: supply decoded KS OS/update, wave bank and Flash paths, optionally native\n";return 77;}
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");auto wave=bytes(argv[2]),flash=bytes(argv[3]);
    auto baseline=ks::test::makeLegacyBoard(os.data(),wave,flash,true);
    auto parallel=ks::test::makeLegacyBoard(os.data(),wave,flash,true);
    check(baseline->boot() && parallel->boot(),"Sequential original H8 boot failed");
    parallel->enableDspWorkers(48000,64);
    uint64_t hash=14695981039346656037ull,frames=0;int64_t peak=0;
    auto send=[&](const std::vector<uint8_t>& m){check(baseline->enqueueMidi(m.data(),m.size()) && parallel->enqueueMidi(m.data(),m.size()),"MIDI overflow");};
    auto compare=[&](unsigned count){for(unsigned n=0;n<count;++n){
        if(frames && (frames&4095)==0){if(frames&4096)parallel->disableDspWorkers();else parallel->enableDspWorkers(48000,64);}
        const auto a=baseline->renderSample(),b=parallel->renderSample();
        if(a!=b){std::cerr<<"Different audio at native frame "<<frames<<'\n';throw std::runtime_error("Audio mismatch");}
        for(auto value:a){hash^=uint32_t(value);hash*=1099511628211ull;peak=std::max(peak,int64_t(std::abs(int64_t(value))));}
        ++frames;
        if((frames&1023)==0){
            check(ks::test::comparisonCpu(*baseline).cycles==ks::test::comparisonCpu(*parallel).cycles && ks::test::comparisonCpu(*baseline).getPC()==ks::test::comparisonCpu(*parallel).getPC(),"H8 scheduling diverged");
            for(unsigned i=0;i<2;++i)check(baseline->dsp(i).cycles()==parallel->dsp(i).cycles() && baseline->dsp(i).instructions()==parallel->dsp(i).instructions(),"DSP scheduling diverged");
            check(baseline->serialDepth()==parallel->serialDepth() && baseline->serialUnderruns==parallel->serialUnderruns && baseline->serialOverflows==parallel->serialOverflows,"Serial exchange diverged");
        }
    }};
    send({0xb0,7,127,91,0,92,0,93,0,0x90,60,100});compare(8192);
    send({0xb0,64,127,0x80,60,0,0x90,67,96,72,80});compare(8192);
    send({0xb0,64,0,120,0,32,33,0xc0,1});compare(8192);
    auto state=baseline->state();state.performance=true;state.activeMask=15;
    for(unsigned i=0;i<4;++i){state.parts[i]=state.parts[0];state.parts[i][179]=100;state.parts[i][181]=uint8_t(i);}
    state.parts[0][189]=1;state.parts[0][190]=15;
    send({0xb0,32,65,0xc0,0});send(ks::stateMessage(state));compare(16384);
    for(uint8_t i=0;i<4;++i)send({uint8_t(0x90|i),uint8_t(48+7*i),100});compare(16384);
    for(uint8_t i=0;i<4;++i)send({uint8_t(0xb0|i),120,0});compare(8192);
    const auto a=baseline->state(),b=parallel->state();
    check(a.parts==b.parts && a.performance==b.performance && a.activeMask==b.activeMask && a.globalChannel==b.globalChannel,"Controller state diverged");
    check(baseline->saveUserFlash()==parallel->saveUserFlash() && baseline->midiReceived==parallel->midiReceived,"Flash/MIDI state diverged");
    check(peak>1000,"Equivalence passage was silent");
    std::cout<<"PASS: exact 24-stream audio; native_frames="<<frames<<" hash="<<hash<<" peak="<<peak<<"; single, sustain, program change, four parts, H8/DSP cycles, serial counters and saved state\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
