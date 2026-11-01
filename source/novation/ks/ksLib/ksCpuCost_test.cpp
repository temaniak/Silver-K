// Explicit retained LEGACY comparison test; not a semantic NativeControllerExecutor proof.
#include "ksBoardComparison_test.h"
#include "ksRom.h"
#include "dsp56kBase/threadtools.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace {
using Clock=std::chrono::steady_clock;
std::vector<uint8_t> bytes(const char* path){std::ifstream f(path,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
double cpu(){
#ifdef _WIN32
    FILETIME created{},exited{},kernel{},user{};
    check(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)!=0,"GetProcessTimes failed");
    return double((uint64_t(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime)*1.e-7+
           double((uint64_t(user.dwHighDateTime)<<32)|user.dwLowDateTime)*1.e-7;
#else
    throw std::runtime_error("Windows CPU accounting required");
#endif
}
}
int main(int argc,char** argv){try{
    if(argc!=5){std::cout<<"SKIP: KS OS, Flash, wave, output JSON required\n";return 77;}
#ifndef _WIN32
    return 77;
#endif
    const auto* flag=std::getenv("SILVER_KS_DSP_THREADS");const bool parallel=flag&&std::string(flag)=="2";
    const double setupCpu=cpu();const auto setupBegin=Clock::now();
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    auto board=ks::test::makeLegacyBoard(os.data(),bytes(argv[3]),bytes(argv[2]),true);
    check(board->boot(),"Original H8 boot failed");
    const double setupCost=cpu()-setupCpu,setupWall=std::chrono::duration<double>(Clock::now()-setupBegin).count();
    dsp56k::ThreadTools::setCurrentThreadPriority(dsp56k::ThreadPriority::Highest);
    dsp56k::ThreadTools::setCurrentThreadRealtimeParameters(48000,128);
    auto send=[&](const std::vector<uint8_t>& m){check(board->enqueueMidi(m.data(),m.size()),"MIDI overflow");};
    auto discard=[&](unsigned frames){for(unsigned i=0;i<frames;++i)board->renderSample();};
    const auto warmBegin=Clock::now();const double warmCpu=cpu();
    send({0xb0,7,127,108,0,111,0,110,127,91,0,92,0,93,0,72,127,73,0,74,0,105,127});discard(8192);
    for(uint8_t n=0;n<8;++n)send({0x90,uint8_t(36+7*n),90});discard(16384);
    for(uint8_t n=0;n<8;++n)send({0x80,uint8_t(36+7*n),0});send({0xb0,120,0});discard(16384);
    if(parallel)board->enableDspWorkers(48000,128);
    discard(4096); // Exclude helper startup and transfer from measured interval.
    const double warmCost=cpu()-warmCpu,warmWall=std::chrono::duration<double>(Clock::now()-warmBegin).count();
    struct Event {uint64_t frame;std::vector<uint8_t> message;};std::vector<Event> events;
    auto frame=[](double seconds){return uint64_t(std::llround(seconds*ks::DSP::SampleRate));};
    events.push_back({frame(.1),{0x90,60,100}});events.push_back({frame(1.1),{0x80,60,0}});
    std::array<int,8> held{};held.fill(-1);
    for(unsigned tick=0;1.12+tick*.04<12.;++tick){std::vector<uint8_t> packet;
        for(unsigned n=0;n<8;++n){if(held[n]>=0)packet.insert(packet.end(),{0x80,uint8_t(held[n]),0});
            held[n]=36+int((tick*7+n*5)%60);packet.insert(packet.end(),{0x90,uint8_t(held[n]),uint8_t(64+held[n]%48)});}
        events.push_back({frame(1.12+tick*.04),std::move(packet)});
    }
    const uint64_t frames=frame(12.);size_t next=0;uint64_t hash=14695981039346656037ull;int64_t peak=0;
    const auto beforeCpu=cpu();const auto begin=Clock::now();
    for(uint64_t n=0;n<frames;++n){
        while(next<events.size() && events[next].frame==n)send(events[next++].message);
        const auto audio=board->renderSample();
        for(auto v:audio){hash^=uint32_t(v);hash*=1099511628211ull;peak=std::max(peak,std::abs(int64_t(v)));}
    }
    const double wall=std::chrono::duration<double>(Clock::now()-begin).count(),cost=cpu()-beforeCpu;
    check(next==events.size() && peak>1000,"Incomplete or silent native passage");
    std::ostringstream report;report<<std::setprecision(12)
        <<"{\"schema\":\"silver-k.cpu-cost.v1\",\"mode\":\"native-fixed-work\",\"dsp_execution_backend\":\""<<(parallel?"two-workers-slice-barrier":"sequential")
        <<"\",\"frames\":"<<frames<<",\"audio_seconds\":"<<double(frames)/ks::DSP::SampleRate<<",\"native_rate_hz\":"<<ks::DSP::SampleRate
        <<",\"wall_seconds\":"<<wall<<",\"process_cpu_seconds\":"<<cost
        <<",\"setup_cpu_seconds\":"<<setupCost<<",\"setup_wall_seconds\":"<<setupWall
        <<",\"warmup_cpu_seconds\":"<<warmCost<<",\"warmup_wall_seconds\":"<<warmWall
        <<",\"missing_frames\":0,\"audio_hash\":\""<<hash<<"\",\"peak\":"<<peak
        <<",\"h8_cycles\":"<<ks::test::comparisonCpu(*board).cycles<<",\"h8_pc\":"<<ks::test::comparisonCpu(*board).getPC()
        <<",\"dsp_a_cycles\":"<<board->dsp(0).cycles()<<",\"dsp_b_cycles\":"<<board->dsp(1).cycles()
        <<",\"dsp_a_instructions\":"<<board->dsp(0).instructions()<<",\"dsp_b_instructions\":"<<board->dsp(1).instructions()
        <<",\"notes\":\"GetProcessTimes user+kernel for coordinator and both DSP helpers. Fixed complete native Board render, original H8/JIT/serial schedule. Boot and warm-up excluded. No host callback, resampler, output queue polling, pacing or diagnostics. 24-stream integer hash must agree. Not a real DAW/driver or an energy measurement.\"}";
    std::ofstream file(argv[4]);file<<report.str();check(bool(file),"Cannot save report");std::cout<<report.str()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
