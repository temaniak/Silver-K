#include "ksBoard.h"
#include "ksRom.h"
#include "dsp56kBase/threadtools.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace {
using Clock=std::chrono::steady_clock;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
std::vector<uint8_t> bytes(const char* path){std::ifstream s(path,std::ios::binary);return {(std::istreambuf_iterator<char>(s)),{}};}
double cpu(){
#ifdef _WIN32
    FILETIME c{},e{},k{},u{};check(GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u)!=0,"GetProcessTimes failed");
    return double((uint64_t(k.dwHighDateTime)<<32)|k.dwLowDateTime)*1.e-7+double((uint64_t(u.dwHighDateTime)<<32)|u.dwLowDateTime)*1.e-7;
#elif defined(__APPLE__)
    rusage usage{};check(getrusage(RUSAGE_SELF,&usage)==0,"getrusage failed");
    return double(usage.ru_utime.tv_sec)+double(usage.ru_utime.tv_usec)*1.e-6
        +double(usage.ru_stime.tv_sec)+double(usage.ru_stime.tv_usec)*1.e-6;
#else
    return 0; // Portability/correctness runs only; do not label as CPU evidence.
#endif
}
struct Stats {
    std::array<std::array<uint64_t,16>,2> reads{},writes{};
    std::array<uint64_t,2> dataLowWrites{},interrupts{};
    explicit Stats(const ks::ControllerTape& tape){for(const auto& e:tape.events){
        ((e.flags&1)?writes:reads)[e.dsp][e.offset]++;
        if((e.flags&3)==3 && e.offset==14)++dataLowWrites[e.dsp];
        if((e.flags&3)==3 && e.offset==2 && (e.value&0x80))++interrupts[e.dsp];
    }}
    std::string json()const{std::ostringstream s;s<<'[';for(unsigned i=0;i<2;++i){if(i)s<<',';
        s<<"{\"dsp\":\""<<(i?'B':'A')<<"\",\"reads_by_register\":[";
        for(unsigned n=0;n<16;++n){if(n)s<<',';s<<reads[i][n];}s<<"],\"writes_by_register\":[";
        for(unsigned n=0;n<16;++n){if(n)s<<',';s<<writes[i][n];}s<<"],\"accepted_low_data_writes\":"<<dataLowWrites[i]<<",\"interrupt_commands\":"<<interrupts[i]<<'}';
    }s<<']';return s.str();}
};
}
int main(int argc,char** argv){try{
    if(argc!=8 && argc!=9){std::cout<<"SKIP: KS OS, Flash, wave, report JSON, mode, single|multi|midi|pedal|controls|state|clock|steal|fx, trace path, optional seconds (up to 30) required\n";return 77;}
    const double duration=argc==9?std::stod(argv[8]):2.;
    check(std::isfinite(duration)&&duration>=2.&&duration<=30.,"Controller probe duration must be 2..30 seconds");
    const std::string mode=argv[5],scenario=argv[6];
    const bool nativeMode=mode=="native" || mode=="native-record" || mode=="native-toggle" || mode=="native-toggle-record";
    const bool liveMode=nativeMode || mode=="live" || mode=="live-record" || mode=="live-toggle" || mode=="live-toggle-record";
    const bool recording=mode=="native-record" || mode=="native-toggle-record" || mode=="record" || mode=="live-record" || mode=="live-toggle-record";
    const bool toggling=mode=="native-toggle" || mode=="native-toggle-record" || mode=="live-toggle" || mode=="live-toggle-record";
    check(mode=="h8" || mode=="record" || mode=="replay" || mode=="boot" || liveMode,"Unknown controller probe mode");
    check(scenario=="single" || scenario=="multi" || scenario=="midi" || scenario=="pedal" || scenario=="controls" || scenario=="state" || scenario=="clock" || scenario=="steal" || scenario=="fx","Unknown controller probe scenario");
    const bool multi=scenario=="multi" || scenario=="pedal" || scenario=="controls" || scenario=="state" || scenario=="clock" || scenario=="steal" || scenario=="fx";
    const auto* threads=std::getenv("SILVER_KS_DSP_THREADS");const bool parallel=threads&&std::string(threads)=="2";
    const auto setup=Clock::now();ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    auto board=std::make_unique<ks::Board>(os.data(),bytes(argv[3]),bytes(argv[2]),true,nativeMode);
    if(mode=="boot")board->startControllerTrace();
    const auto bootBegin=Clock::now();const double bootCpu=cpu();check(board->boot(),"KS boot failed");
    if(nativeMode)check(board->h8InterpretedSteps()==0,"Native startup used H8 interpreter");
    const double bootCost=cpu()-bootCpu,bootWall=std::chrono::duration<double>(Clock::now()-bootBegin).count();
    const uint64_t bootH8Steps=board->h8InterpretedSteps();
    if(nativeMode)board->enableNativeController();
    auto send=[&](const std::vector<uint8_t>& m){check(board->enqueueMidi(m.data(),m.size()),"MIDI rejected");};
    auto discard=[&](unsigned n){for(unsigned i=0;i<n;++i)board->renderSample();};
    std::shared_ptr<const ks::ControllerTape> tape;
    uint64_t frames=0,hash=14695981039346656037ull;int64_t peak=0;double cost=bootCost,wall=bootWall;
    uint64_t h8Start=board->cpu().cycles;
    if(mode=="boot"){
        tape=board->finishControllerTrace();h8Start=tape->beginCycle;tape->validate();tape->save(argv[7]);
    }else{
        send({0xb0,7,127,108,0,111,0,110,127,91,0,92,0,93,0,72,127,73,0,74,0,105,127});discard(8192);
        // Use live MIDI for setup; native mode already owns post-boot preparation.
        if(multi){
            auto state=board->state();state.performance=true;state.activeMask=15;
            for(unsigned i=0;i<4;++i){state.parts[i]=state.parts[0];auto& p=state.parts[i];p[179]=100;p[181]=uint8_t(i);p[182]=0;p[183]=127;}
            if(scenario=="pedal")for(unsigned i=0;i<4;++i){auto& p=state.parts[i];p[0]=uint8_t((p[0]&~24)|(i<<3));p[88]&=~4;p[182]=36;p[183]=84;}
            if(scenario=="clock")for(auto& p:state.parts){p[88]=uint8_t((p[88]&~31)|12);p[84]=30;p[85]=9;p[86]=32;p[87]=0;}
            if(scenario=="steal")for(auto& p:state.parts)p[0]=uint8_t((p[0]&~31)|18); // Poly, two unison voices.
            if(scenario=="fx")for(auto& p:state.parts){p[100]=48;p[107]=48;p[110]=32;p[65]=0;}
            state.parts[0][189]=1;state.parts[0][190]=15;
            send({0xb0,32,65,0xc0,0});send(ks::stateMessage(state));discard(24000);
        }
        const unsigned parts=multi?4:1,perPart=8/parts;
        for(unsigned p=0;p<parts;++p)for(unsigned n=0;n<perPart;++n)send({uint8_t(0x90|p),uint8_t(36+7*(p*perPart+n)),90});discard(8192);
        for(unsigned p=0;p<parts;++p){for(unsigned n=0;n<perPart;++n)send({uint8_t(0x80|p),uint8_t(36+7*(p*perPart+n)),0});send({uint8_t(0xb0|p),120,0});}discard(16384);
        if(parallel)board->enableDspWorkers(48000,128);discard(4096);
        struct Event {uint64_t frame;std::vector<uint8_t> bytes;};std::vector<Event> events;
        auto native=[](double seconds){return uint64_t(std::llround(seconds*ks::DSP::SampleRate));};
        frames=native(duration);
        for(unsigned tick=0;.1+tick*.1<duration;++tick){std::vector<uint8_t> packet;
            // Include sustain and original per-part Program Change/control paths.
            if(tick==4)packet.insert(packet.end(),{0xb0,64,127});
            if(tick==8)packet.insert(packet.end(),{0xc0,1});
            if(tick==12)packet.insert(packet.end(),{0xb0,64,0,105,100});
            for(unsigned p=0;p<parts;++p)for(unsigned n=0;n<perPart;++n){
                if(tick)packet.insert(packet.end(),{uint8_t(0x80|p),uint8_t(36+((tick-1)*7+p*perPart+n)*5%60),0});
                packet.insert(packet.end(),{uint8_t(0x90|p),uint8_t(36+(tick*7+p*perPart+n)*5%60),uint8_t(75+n*3)});
            }
            events.push_back({native(.1+tick*.1),std::move(packet)});
        }
        if(scenario=="midi"){
            events.clear();
            // New, non-replayed MIDI values: partial packets/running status,
            // velocity-zero release, repeated pitches, CCs and fallback paths.
            const std::vector<std::vector<uint8_t>> packets{
                {0x90,53},{97},{57,82,60,71},{0xb0,1,93},{7,83},{11,74},
                {64,127},{0x80,53,31},{0x90,57,0},{0xb0,10,23},{105,91},
                {0x90,60,86,60,109},{0xb0,64,0},{123,0},{0x90,66,111},
                {0xb0,120,0},{0xc0,1},{0x90,41,102},{0xb0,1,0},{7,127},
                {11,127},{10,64},{64,127},{0x90,41,0},{0xb0,64,0},
                {0xf8,0x90,76,99},{0xf1,0,0x90,79,72},{0xb0,123,0},{120,0}};
            for(size_t n=0;n<packets.size();++n)events.push_back({native(.04+n*.06),packets[n]});
        }
        if(scenario=="pedal"){
            events.clear();
            const std::vector<std::vector<uint8_t>> packets{
                {0x90,36,92,60,101,84,87,35,109,85,110},{0xb0,64,63},
                {0x90,64,96},{0xb0,64,64},{0x80,60,127},{0x90,64,0},
                {0x90,36,103},{0xb0,64,127},{0x80,36,0,84,0},{0xb0,64,64},
                {0xb0,64,63},{64,0},{0x90,55,88,59,99,67,111},{0xb0,64,127},
                {123,0},{0x80,55,0,59,0,67,0},{0xb0,64,0},{0x90,65,104},
                {0xb0,120,0},{0x90,65,0},{0xb0,64,127},{64,0},
                {0x90,0,90,127,91},{0x80,0,0,127,0}};
            for(unsigned cycle=0;.04+cycle*2.<duration;++cycle)for(size_t n=0;n<packets.size();++n){
                const double at=.04+cycle*2.+n*.08;if(at>=duration)continue;
                std::vector<uint8_t> packet;
                for(unsigned part=0;part<4;++part){bool hasStatus=false;
                    for(auto v:packets[n]){if(v&128){v=uint8_t((v&0xf0)|part);hasStatus=true;}
                        else if(!hasStatus){packet.push_back(uint8_t(0xb0|part));hasStatus=true;}
                        packet.push_back(v);}}
                events.push_back({native(at),std::move(packet)});
            }
        }
        auto add=[&](double at,std::vector<uint8_t> data){if(at<duration)events.push_back({native(at),std::move(data)});};
        auto four=[&](std::initializer_list<uint8_t> message){std::vector<uint8_t> data;
            for(unsigned p=0;p<4;++p){bool first=true;for(auto b:message){data.push_back(first?uint8_t(b|p):b);first=false;}}return data;};
        if(scenario=="controls" || scenario=="state" || scenario=="clock" || scenario=="steal"){
            events.clear();add(.01,four({0x90,60,100}));add(.02,four({0xb0,64,127}));
            add(.04,four({0x80,60,0}));add(.06,four({0xb0,64,0}));
            add(duration-.04,four({0x80,60,0}));add(duration-.03,four({0xb0,64,0}));
        }
        if(scenario=="controls"){
            // All CC numbers with live seven-bit values, plus explicit NRPN,
            // channel pressure, poly pressure and bend, on all four channels.
            add(.08,four({0x90,60,101}));
            for(unsigned cc=0;cc<128;++cc){std::vector<uint8_t> data;
                for(unsigned p=0;p<4;++p)data.insert(data.end(),{uint8_t(0xb0|p),uint8_t(cc),uint8_t((cc*5+p*17)&127)});
                add(.1+cc*.011,std::move(data));}
            for(unsigned n=0;n<6;++n){std::vector<uint8_t> data;
                for(unsigned p=0;p<4;++p){const auto status=uint8_t(0xb0|p);
                    data.insert(data.end(),{status,99,0,status,98,uint8_t(5+n*3),status,6,uint8_t(31+n*13),
                        uint8_t(0xe0|p),uint8_t(n*19),uint8_t(54+p*5),uint8_t(0xd0|p),uint8_t(20+n*15),uint8_t(0xa0|p),60,uint8_t(99-n*11)});}
                add(.15+n*.22,std::move(data));}
        }
        if(scenario=="state"){
            auto perf=board->state();perf.performance=true;perf.activeMask=15;
            for(unsigned p=0;p<4;++p){perf.parts[p][179]=uint8_t(70+p*7);perf.parts[p][12]=uint8_t(35+p*17);}
            add(.08,ks::stateMessage(perf));add(.5,four({0x90,60,95}));
            ks::State single;single.parts[0]=perf.parts[0];single.parts[0][12]=93;
            auto stored=ks::stateMessage(single);stored[7]=1;stored[8]=1;stored[11]=1;stored[12]=3;
            add(.8,std::move(stored));add(.98,{0xb0,32,33,0xc0,3});
            add(1.13,{0x90,48,101,55,92,62,83});add(1.25,{0x80,48,0,55,0,62,0});
            add(1.4,{0xb0,32,65,0xc0,0});add(1.41,ks::stateMessage(perf));add(1.85,four({0x90,67,102}));
        }
        if(scenario=="clock"){
            add(.08,{0xfa});add(.12,four({0x90,48,104}));add(.14,four({0x90,55,96}));add(.16,four({0x90,62,88}));
            for(double t=.09;t<duration-.15;){add(t,{0xf8});t+=60./((t<1.?90.:180.)*24.);}
            add(.25,four({0xb0,64,127}));add(.65,four({0x80,48,0}));add(.9,four({0xb0,64,0}));
            add(1.35,{0xfc});add(1.55,{0xfb});add(1.7,four({0x90,69,109}));add(1.85,four({0x80,69,0}));
        }
        if(scenario=="steal"){
            for(unsigned chord=0;chord<3;++chord){std::vector<uint8_t> data;
                for(unsigned p=0;p<4;++p)for(unsigned n=0;n<6;++n)data.insert(data.end(),{uint8_t(0x90|p),uint8_t(36+p*6+n+chord*3),uint8_t(76+n*7)});
                add(.12+chord*.48,std::move(data));}
            add(.3,four({0xb0,64,127}));
            for(unsigned group=0;group<2;++group){std::vector<uint8_t> data;
                for(unsigned p=0;p<4;++p)for(unsigned n=0;n<6;++n)data.insert(data.end(),{uint8_t(0x80|p),uint8_t(36+p*6+n+group*3),0});
                add(.5+group*.5,std::move(data));}
            add(1.35,four({0xb0,64,0}));add(1.65,four({0xb0,120,0}));add(1.75,four({0x90,60,110}));
        }
        if(scenario=="fx"){
            events.clear();std::vector<uint8_t> on,off;
            for(unsigned part=0;part<4;++part)for(unsigned note=0;note<4;++note){
                on.insert(on.end(),{uint8_t(0x90|part),uint8_t(48+part+note*4),uint8_t(80+note*7)});
                off.insert(off.end(),{uint8_t(0x80|part),uint8_t(48+part+note*4),0});
            }
            add(.1,std::move(on));add(.6,four({0xb0,92,72}));add(.9,four({0xb0,91,80}));
            add(.7,four({0xb0,64,127}));add(1.2,four({0xb0,93,48}));
            add(1.6,std::move(off));add(1.7,four({0xb0,64,0}));
        }
        std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.frame<b.frame;});
        if(liveMode){if(!nativeMode)board->enableLiveMidiController();board->resetControllerMetrics();if(recording)board->enableControllerDelegationProfile();}
        if(recording)board->startControllerTrace();
        if(mode=="replay"){tape=ks::ControllerTape::load(argv[7]);board->startRecordedController(tape);}
        h8Start=board->cpu().cycles;size_t next=0;
        auto coverage=[](const ks::LiveMidiController& live){return std::array<uint64_t,8>{
            live.messages,live.noteActions.noteOnCalls,live.noteActions.noteOffCalls,live.noteActions.pedalCalls,
            live.voiceActions.allocationCalls,live.voiceActions.startCalls,live.voiceActions.releaseCalls,live.voiceActions.transportSteps};};
        std::array<uint64_t,8> beforeSwitch{};
        const auto* capturePath=std::getenv("SILVER_KS_AUDIO_CAPTURE");
        std::vector<ks::Board::AudioFrame> captured;
        if(capturePath && *capturePath){std::ifstream old(capturePath,std::ios::binary);check(!old.good(),"Refusing to overwrite audio capture");captured.resize(frames);}
        const auto begin=Clock::now();const double before=cpu();
        for(uint64_t n=0;n<frames;++n){
            if(toggling && n==native(.65)){
                // Handoff destroys this native instance. Retain coverage from
                // its actual live interval, not just the new instance at .95 s.
                beforeSwitch=coverage(*board->liveMidiController());
                board->enableLiveMidiController(false);
            }
            if(toggling && n==native(.95)){if(nativeMode)board->enableNativeController();else board->enableLiveMidiController();if(recording)board->enableControllerDelegationProfile();}
            if(mode!="replay")while(next<events.size() && events[next].frame==n)send(events[next++].bytes);
            const auto a=board->renderSample();if(!captured.empty())captured[n]=a;for(auto v:a){hash^=uint32_t(v);hash*=1099511628211ull;peak=std::max(peak,std::abs(int64_t(v)));}
        }
        cost=cpu()-before;wall=std::chrono::duration<double>(Clock::now()-begin).count();
        if(!captured.empty()){
            std::ofstream out(capturePath,std::ios::binary);out.write(reinterpret_cast<const char*>(captured.data()),std::streamsize(captured.size()*sizeof(captured[0])));
            check(bool(out),"Cannot save full native audio capture");
        }
        check(peak>1000,"Controller probe rendered silence");
        if(recording){tape=board->finishControllerTrace();tape->validate();tape->save(argv[7]);}
        if(liveMode){const auto& live=*board->liveMidiController();
            const auto last=coverage(live);
            for(unsigned i=0;i<last.size();++i)check(last[i]+beforeSwitch[i]>0,
                i<4?"Live note/sustain handlers were not exercised":"Live voice/transport handlers were not exercised");
            uint64_t delegated=0;for(const auto count:live.originalPages)delegated+=count;
            if(recording)check(delegated==live.originalStepCalls && (nativeMode?delegated==0:delegated>0),"Original H8 delegation accounting differs");}
        if(nativeMode){
            check(board->nativeController()!=nullptr && board->nativeController()->steps>0,"Native runtime did not execute");
            check(board->h8StepsSinceNativeHandoff()==0,"H8 executed after native handoff");
            if(!toggling)check(board->h8InterpretedSteps()==bootH8Steps,"H8 executed during post-boot preparation/playback");
        }
        if(mode=="replay"){
            check(board->cpu().cycles==h8Start,"Prototype executed H8 instructions");
            check(board->controllerCycles()==tape->endCycle && board->checkedControllerPorts()==tape->events.size(),"Prototype did not consume exact trace");
            const uint8_t note[]{0x90,60,100};check(!board->enqueueMidi(note,3),"Prototype accepted unsupported live MIDI");
        }
    }
    auto digest=[](const auto& data){uint64_t h=14695981039346656037ull;for(auto b:data){h^=uint8_t(b);h*=1099511628211ull;}return h;};
    std::vector<uint8_t> stateBytes,flashBytes;
    if(mode!="replay"){const auto finalState=board->state();stateBytes=ks::stateMessage(finalState);
        stateBytes.insert(stateBytes.end(),{finalState.globalChannel,finalState.activeMask});flashBytes=board->saveUserFlash();}
    auto snapshotDigest=[&](const auto& data){return mode=="replay"?std::string("null"):"\""+std::to_string(digest(data))+"\"";};
    std::vector<uint8_t> midiOut;uint8_t transmitted=0;while(board->popMidiOut(transmitted))midiOut.push_back(transmitted);
    if(nativeMode && !toggling)check(board->liveMidiController()->noteActions.keySearchCalls.partialEntries==0,"Native playback used the test-only partial-key adapter");
    if(nativeMode && !toggling)check(board->liveMidiController()->voiceActions.wordCalls.partialEntries==0,"Native playback used partial DSP-word recovery");
    if(nativeMode && !toggling)check(board->liveMidiController()->parameterScaleCalls.partialEntries==0,"Native playback used partial coefficient recovery");
    if(scenario=="state")check(board->flashRevision>0,"State scenario did not complete a Flash page write");
    const auto depth=board->serialDepth();std::ostringstream report;report<<std::setprecision(12)
        <<"{\"schema\":\"silver-k.controller-probe.v1\",\"mode\":\""<<mode<<"\",\"scenario\":\""<<scenario<<"\",\"controller_backend\":\""<<board->controllerBackend()
        <<"\",\"dsp_threads\":"<<(parallel?2:1)<<",\"frames\":"<<frames<<",\"audio_hash\":\""<<hash<<"\",\"peak\":"<<peak
        <<",\"cpu_seconds\":"<<cost<<",\"wall_seconds\":"<<wall<<",\"setup_wall_seconds\":"<<std::chrono::duration<double>(Clock::now()-setup).count()-wall
        <<",\"h8_start_cycles\":"<<h8Start<<",\"h8_end_cycles\":"<<board->cpu().cycles<<",\"controller_end_cycles\":"<<board->controllerCycles()
        <<",\"dsp_a_cycles\":"<<board->dsp(0).cycles()<<",\"dsp_b_cycles\":"<<board->dsp(1).cycles()
        <<",\"dsp_a_instructions\":"<<board->dsp(0).instructions()<<",\"dsp_b_instructions\":"<<board->dsp(1).instructions()
        <<",\"serial_depth\":["<<depth[0]<<','<<depth[1]<<"],\"serial_underruns\":["<<board->serialUnderruns[0]<<','<<board->serialUnderruns[1]
        <<"],\"serial_overflows\":["<<board->serialOverflows[0]<<','<<board->serialOverflows[1]<<']'
        <<",\"checked_ports\":"<<board->checkedControllerPorts()
        <<",\"state_digest\":"<<snapshotDigest(stateBytes)<<",\"flash_digest\":"<<snapshotDigest(flashBytes)
        <<",\"flash_revision\":"<<board->flashRevision<<",\"midi_tx_digest\":"<<snapshotDigest(midiOut)<<",\"midi_tx_bytes\":"<<midiOut.size()
        <<",\"midi_received\":"<<board->midiReceived
        <<",\"native_dsp_word_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.wordCalls.started:0)
        <<",\"native_dsp_word_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.wordCalls.completed:0)
        <<",\"native_dsp_word_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.wordCalls.partialEntries:0)
        <<",\"native_parameter_scale_calls\":"<<(board->liveMidiController()?board->liveMidiController()->parameterScaleCalls.started:0)
        <<",\"native_parameter_scale_returns\":"<<(board->liveMidiController()?board->liveMidiController()->parameterScaleCalls.completed:0)
        <<",\"native_parameter_scale_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->parameterScaleCalls.partialEntries:0)
        <<",\"native_key_search_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keySearchCalls.started:0)
        <<",\"native_key_search_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keySearchCalls.completed:0)
        <<",\"native_key_search_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keySearchCalls.partialEntries:0)
        <<",\"control_jobs\":"<<board->controlJobs<<",\"control_job_boundaries\":"<<board->controlBoundaries
        <<",\"transport_jobs\":"<<board->transportJobs<<",\"transport_job_boundaries\":"<<board->transportBoundaries;
    report<<",\"h8_interpreted_steps_boot\":"<<bootH8Steps
        <<",\"h8_interpreted_steps_after_boot\":"<<(board->h8InterpretedSteps()-bootH8Steps)
        <<",\"h8_interpreted_steps_since_handoff\":"<<board->h8StepsSinceNativeHandoff();
    if(const auto* runtime=board->nativeController())report<<",\"native_runtime_steps\":"<<runtime->steps<<",\"native_interrupt_entries\":"<<runtime->interruptEntries;
#if defined(SILVER_KS_CONTROLLER_PROFILING)
    if(const auto* runtime=board->nativeController();runtime && !runtime->boundaryProfile.empty()){
        report<<",\"native_boundary_profile\":[";bool first=true;
        for(unsigned pc=0;pc<runtime->boundaryProfile.size();++pc)if(runtime->boundaryProfile[pc]){
            if(!first)report<<',';first=false;report<<'['<<pc<<','<<runtime->boundaryProfile[pc]<<']';
        }report<<']';
    }
#endif
    if(tape)report<<",\"trace_events\":"<<tape->events.size()<<",\"trace_slices\":"<<tape->slices.size()<<",\"trace_digest\":\""<<tape->digest()<<"\",\"bus\":"<<Stats(*tape).json();
    if(const auto* live=board->liveMidiController()){report<<",\"native_midi_steps\":"<<live->nativeSteps<<",\"native_midi_bytes\":"<<live->bytes
        <<",\"native_midi_messages\":"<<live->messages<<",\"native_note_on\":"<<live->noteOn<<",\"native_note_off\":"<<live->noteOff<<",\"native_cc\":"<<live->controls
        <<",\"native_note_steps\":"<<live->noteActions.steps<<",\"native_note_on_calls\":"<<live->noteActions.noteOnCalls
        <<",\"native_note_off_calls\":"<<live->noteActions.noteOffCalls<<",\"native_sustain_calls\":"<<live->noteActions.pedalCalls
        <<",\"native_sustain_release_calls\":"<<live->noteActions.pedalReleaseCalls
        <<",\"native_voice_steps\":"<<live->voiceActions.steps<<",\"native_allocation_calls\":"<<live->voiceActions.allocationCalls
        <<",\"native_voice_start_calls\":"<<live->voiceActions.startCalls<<",\"native_voice_release_calls\":"<<live->voiceActions.releaseCalls
        <<",\"native_transport_steps\":"<<live->voiceActions.transportSteps<<",\"original_h8_step_calls\":"<<live->originalStepCalls
        <<",\"original_h8_pages\":[";
        bool first=true;for(unsigned page=0;page<live->originalPages.size();++page)if(live->originalPages[page]){
            if(!first)report<<',';first=false;report<<"{\"page\":"<<page<<",\"calls\":"<<live->originalPages[page]<<'}';
        }report<<']';
    }
    report<<",\"notes\":\"Fresh native C++ mode starts at reset and prepares/warms up with zero H8 interpreter calls; fixed C++ control flow still preserves the compatibility register/PC/flag/stack ABI and timing grid. H8/hybrid modes keep original startup. This is not full semantic controller-model removal. Hybrid live mode retains original callees. h8 cycle fields are the shared guest clock, not proof of interpreted execution. Interpreter call counters establish handoff; native mode has no fallback. original_h8_step_calls includes interrupt arbitration, not instruction or CPU-time counts; page counts cover only the current enabled segment. Replay is separate finite tape playback. CPU counts whole process user+kernel on Windows/macOS, no boot/setup/trace file IO; unavailable on other platforms. Raw trace may contain private firmware/presets. Native runtime retains H8 state/clock storage for reversible comparison, not H8 instruction execution. ROM update/bootloader entry is outside the native playback range.\"}";
    std::ifstream existing(argv[4]);check(!existing.good(),"Refusing to overwrite controller report");std::ofstream out(argv[4]);out<<report.str();check(bool(out),"Cannot save controller report");
    std::cout<<report.str()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
