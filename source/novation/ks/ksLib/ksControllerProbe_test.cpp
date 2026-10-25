#include "ksBoard.h"
#include "ksPanelIoFixture_test.h"
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
    if(argc!=8 && argc!=9){std::cout<<"SKIP: KS OS, Flash, wave, report JSON, mode, single|multi|midi|pedal|controls|state|clock|steal|fx|panel-store-single|panel-store-multi|panel-save-part|panel-io-single|panel-io-multi|panel-screen-single|panel-screen-multi|panel-edit-single|panel-edit-multi|panel-parts-single|panel-parts-multi|panel-actions-single|panel-actions-multi|panel-functions-single|panel-functions-multi|panel-keyboard-single|panel-keyboard-multi, trace path, optional seconds (up to 30) required\n";return 77;}
    const double duration=argc==9?std::stod(argv[8]):2.;
    check(std::isfinite(duration)&&duration>=2.&&duration<=30.,"Controller probe duration must be 2..30 seconds");
    const std::string mode=argv[5],scenario=argv[6];
    const bool nativeMode=mode=="native" || mode=="native-record" || mode=="native-toggle" || mode=="native-toggle-record";
    const bool liveMode=nativeMode || mode=="live" || mode=="live-record" || mode=="live-toggle" || mode=="live-toggle-record";
    const bool recording=mode=="native-record" || mode=="native-toggle-record" || mode=="record" || mode=="live-record" || mode=="live-toggle-record";
    const bool toggling=mode=="native-toggle" || mode=="native-toggle-record" || mode=="live-toggle" || mode=="live-toggle-record";
    check(mode=="h8" || mode=="record" || mode=="replay" || mode=="boot" || liveMode,"Unknown controller probe mode");
    // PROGRAM_MENU_FLAGS_BEGIN
    const bool programMenuSingle=scenario=="panel-mode-single";
    const bool programMenuToMulti=scenario=="panel-mode-multi";
    const bool programPartNavigation=scenario=="panel-part-wrap-multi"||scenario=="panel-part-name"||scenario=="panel-part-category"||scenario=="panel-part-special";
    const bool programBrowserFixture=scenario=="panel-browse-single"||scenario=="panel-browse-multi"||scenario=="panel-browse-part";
    const bool programStoreMenuFixture=scenario=="panel-menu-global-save"||scenario=="panel-menu-dump-global"||scenario=="panel-menu-dump-multi"||scenario=="panel-menu-factory";
    const bool programMenuFixture=programMenuSingle||programMenuToMulti||programPartNavigation||programBrowserFixture||programStoreMenuFixture;
    const bool programMenuMulti=programMenuSingle||programPartNavigation||scenario=="panel-browse-multi"||scenario=="panel-browse-part"||scenario=="panel-menu-dump-multi";
    // PROGRAM_MENU_FLAGS_END
    const bool panelPartSave=scenario=="panel-save-part";
    const bool panelActions=scenario=="panel-actions-single"||scenario=="panel-actions-multi";
    const bool panelFunctions=scenario=="panel-functions-single"||scenario=="panel-functions-multi";
    const bool panelKeyboard=scenario=="panel-keyboard-single"||scenario=="panel-keyboard-multi";
    const bool panelParts=scenario=="panel-parts-single" || scenario=="panel-parts-multi";
    const bool panelEdit=scenario=="panel-edit-single" || scenario=="panel-edit-multi";
    const bool panelScreen=panelActions || panelFunctions || panelKeyboard || panelParts || panelEdit || scenario=="panel-screen-single" || scenario=="panel-screen-multi";
    const bool panelIo=panelScreen || scenario=="panel-io-single" || scenario=="panel-io-multi";
    if(panelIo||programMenuFixture)check(duration>=4.,"Panel/queued-menu scenario requires at least four seconds");
    const bool panelStore=panelPartSave || panelIo || scenario=="panel-store-single" || scenario=="panel-store-multi";
    check(scenario=="single" || scenario=="multi" || scenario=="midi" || scenario=="pedal" || scenario=="controls" || scenario=="state" || scenario=="clock" || scenario=="steal" || scenario=="fx" || panelStore || programMenuFixture,"Unknown controller probe scenario");
    const bool multi=programMenuMulti || panelPartSave || scenario=="panel-actions-multi"||scenario=="panel-functions-multi"||scenario=="panel-keyboard-multi"||scenario=="panel-parts-multi" || scenario=="panel-edit-multi" || scenario=="panel-screen-multi" || scenario=="panel-io-multi" || scenario=="multi" || scenario=="pedal" || scenario=="controls" || scenario=="state" || scenario=="clock" || scenario=="steal" || scenario=="fx" || scenario=="panel-store-multi";
    const auto* threads=std::getenv("SILVER_KS_DSP_THREADS");const bool parallel=threads&&std::string(threads)=="2";
    const auto setup=Clock::now();ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    std::unique_ptr<ks::test::PanelIoFixture> ioFixture;
    auto board=std::make_unique<ks::Board>(os.data(),bytes(argv[3]),bytes(argv[2]),true,nativeMode);
    // Private comparison metadata: CPU scratch bytes cease to be resources
    // when a native interrupt owns its continuation. Keep both digests so a
    // discarded stack cannot hide a difference in any other address.
    const auto memoryDigests=[&]{
        std::array<uint64_t,2> digests{{14695981039346656037ull,14695981039346656037ull}};
        for(unsigned address=0;address<(1u<<24);++address){
            auto& digest=digests[address>=0xfff000&&address<0xfffc00?1:0];
            digest^=board->cpu().memory[address];digest*=1099511628211ull;
        }
        return digests;
    };
    std::array<uint64_t,2> memoryStart{};
    if(mode=="boot"){memoryStart=memoryDigests();board->startControllerTrace();}
    const auto bootBegin=Clock::now();const double bootCpu=cpu();check(board->boot(),"KS boot failed");
    if(nativeMode)check(board->h8InterpretedSteps()==0,"Native startup used H8 interpreter");
    const double bootCost=cpu()-bootCpu,bootWall=std::chrono::duration<double>(Clock::now()-bootBegin).count();
    const uint64_t bootH8Steps=board->h8InterpretedSteps();
    const auto* bootLive=board->liveMidiController();
    const uint64_t bootStreamStarts=bootLive?bootLive->bootStreamCalls.started:0,bootStreamReturns=bootLive?bootLive->bootStreamCalls.completed:0,bootStreamPartial=bootLive?bootLive->bootStreamCalls.partialEntries:0;
    const uint64_t bootSetupStarts=bootLive?bootLive->bootSetupCalls.started:0,bootSetupReturns=bootLive?bootLive->bootSetupCalls.completed:0,bootSetupPartial=bootLive?bootLive->bootSetupCalls.partialEntries:0;
    const uint64_t startupStarts=bootLive?bootLive->startupCalls.startupStarted:0,startupReturns=bootLive?bootLive->startupCalls.startupCompleted:0,resetReturns=bootLive?bootLive->startupCalls.resetCompleted:0;
    if(nativeMode)check(startupStarts==1 && startupReturns==1 && resetReturns==1 && !bootLive->startupCalls.partialEntries && !bootLive->voiceActions.selectCalls.partialEntries,"Native start used incomplete reset/startup or partial recovery");
    if(nativeMode)check(bootSetupStarts==2 && bootSetupReturns==2 && !bootSetupPartial,"Native startup did not complete both DSP setup jobs without recovery");
    if(nativeMode)check(bootStreamStarts==2 && bootStreamReturns==2 && !bootStreamPartial,"Native startup did not complete both DSP firmware streams without recovery");
    if(nativeMode)check(!bootLive->internalMidiQueueCalls.partialEntries,"Native startup used partial InternalMidiQueue recovery before metric reset");
    if(nativeMode)check(!bootLive->midiOutputCalls.partialEntries,"Native startup used partial MidiOutput recovery before metric reset");
    if(nativeMode)check(!bootLive->internalMidiServiceCalls.partialEntries,"Native startup used partial InternalMidiService recovery before metric reset");
    if(nativeMode)check(!bootLive->stateForegroundCalls.partialEntries,"Native startup used partial StateForeground recovery before metric reset");
    if(nativeMode)check(!bootLive->stateButtonDispatchCalls.partialEntries,"Native startup used partial StateButtonDispatch recovery before metric reset");
    if(nativeMode)check(!bootLive->panelAnalogCalls.partialEntries,"Native startup used partial PanelAnalog recovery before metric reset");
    if(nativeMode)check(!bootLive->clockSyncCalls.partialEntries,"Native startup used partial ClockSync recovery before metric reset");
    if(nativeMode)check(!bootLive->clockPulseCalls.partialEntries,"Native startup used partial ClockPulse recovery before metric reset");
    if(nativeMode)check(!bootLive->clockDisplayCalls.partialEntries,"Native startup used partial ClockDisplay recovery before metric reset");
    if(nativeMode)check(!bootLive->stateScreenCalls.partialEntries,"Native startup used partial StateScreen recovery before metric reset");
    if(nativeMode)check(!bootLive->panelSettingsCalls.partialEntries,"Native startup used partial PanelSettings recovery before metric reset");
    if(nativeMode)check(!bootLive->stateEditContextCalls.partialEntries,"Native startup used partial StateEditContext recovery before metric reset");
    if(nativeMode)check(!bootLive->stateScreenEditCalls.partialEntries,"Native startup used partial StateScreenEdit recovery before metric reset");
    if(nativeMode)check(!bootLive->stateMenuFormatCalls.partialEntries,"Native startup used partial StateMenuFormat recovery before metric reset");
    if(nativeMode)check(!bootLive->systemServicesCalls.partialEntries,"Native startup used partial SystemServices recovery before metric reset");
    if(nativeMode)check(!bootLive->keyboardEventsCalls.partialEntries,"Native startup used partial KeyboardEvents recovery before metric reset");
    if(nativeMode)check(!bootLive->statePartSelectionCalls.partialEntries,"Native startup used partial StatePartSelection recovery before metric reset");
    if(nativeMode)check(!bootLive->stateMenuActionsCalls.partialEntries,"Native startup used partial StateMenuActions recovery before metric reset");
    if(nativeMode)check(!bootLive->keyboardScanCalls.partialEntries,"Native startup used partial KeyboardScan recovery before metric reset");
    if(nativeMode)check(!bootLive->diagnosticCodecCalls.partialEntries,"Native startup used partial DiagnosticCodec recovery before metric reset");
    if(nativeMode)check(!bootLive->diagnosticTransferCalls.partialEntries,"Native startup used partial DiagnosticTransfer recovery before metric reset");
    if(nativeMode)check(!bootLive->memoryDiagnosticsCalls.partialEntries,"Native startup used partial MemoryDiagnostics recovery before metric reset");
    if(nativeMode)check(!bootLive->startupBannerCalls.partialEntries,"Native startup used partial StartupBanner recovery before metric reset");
    if(nativeMode)check(!bootLive->stateScreenActionsCalls.partialEntries,"Native startup used partial StateScreenActions recovery before metric reset");
    if(nativeMode)check(!bootLive->statePanelModesCalls.partialEntries,"Native startup used partial StatePanelModes recovery before metric reset");
    if(nativeMode)check(!bootLive->stateFunctionKeysCalls.partialEntries,"Native startup used partial StateFunctionKeys recovery before metric reset");
    if(nativeMode)check(!bootLive->stateEditWorkflowCalls.partialEntries,"Native startup used partial StateEditWorkflow recovery before metric reset");
    if(nativeMode)check(!bootLive->loaderInputsCalls.partialEntries,"Native startup used partial LoaderInputs recovery before metric reset");
    if(nativeMode)check(!bootLive->loaderCodecCalls.partialEntries,"Native startup used partial LoaderCodec recovery before metric reset");
    if(nativeMode)check(!bootLive->loaderPacketsCalls.partialEntries,"Native startup used partial LoaderPackets recovery before metric reset");
    if(nativeMode)check(!bootLive->loaderProgramCalls.partialEntries,"Native startup used partial LoaderProgram recovery before metric reset");
    if(nativeMode)check(!bootLive->loaderSessionCalls.partialEntries,"Native startup used partial LoaderSession recovery before metric reset");
    if(nativeMode)check(!bootLive->stateValueBoundsCalls.partialEntries,"Native startup used partial StateValueBounds recovery before metric reset");
    if(nativeMode)check(!bootLive->stateEditLabelCalls.partialEntries,"Native startup used partial StateEditLabel recovery before metric reset");
    if(nativeMode)check(!bootLive->stateFieldActionsCalls.partialEntries,"Native startup used partial StateFieldActions recovery before metric reset");
    if(nativeMode)check(!bootLive->stateBankNavigationCalls.partialEntries,"Native startup used partial StateBankNavigation recovery before metric reset");
    if(nativeMode)check(!bootLive->stateStatusDisplayCalls.partialEntries,"Native startup used partial StateStatusDisplay recovery before metric reset");
    if(nativeMode)check(!bootLive->stateStoredValueCalls.partialEntries,"Native startup used partial StateStoredValue recovery before metric reset");
    if(nativeMode)check(!bootLive->stateBrowseWorkflowCalls.partialEntries,"Native startup used partial StateBrowseWorkflow recovery before metric reset");
    if(nativeMode)check(!bootLive->stateProgramBrowseCalls.partialEntries,"Native startup used partial StateProgramBrowse recovery before metric reset");
    if(nativeMode)check(!bootLive->stateProgramDisplayCalls.partialEntries,"Native startup used partial StateProgramDisplay recovery before metric reset");
    if(nativeMode)check(!bootLive->stateProgramEventCalls.partialEntries,"Native startup used partial StateProgramEvent recovery before metric reset");
    if(nativeMode)check(!bootLive->panelAdcScaleCalls.partialEntries&&!bootLive->panelAdcCalibrationCalls.partialEntries&&!bootLive->panelAdcIrqCalls.partialEntries&&!bootLive->panelAdcScanCalls.partialEntries,"Native startup used partial ADC recovery before metric reset");
    if(nativeMode)board->enableNativeController();
    auto send=[&](const std::vector<uint8_t>& m){check(board->enqueueMidi(m.data(),m.size()),"MIDI rejected");};
    auto discard=[&](unsigned n){for(unsigned i=0;i<n;++i)board->renderSample();};
    std::shared_ptr<const ks::ControllerTape> tape;
    uint64_t panelEvents=0;std::array<uint8_t,8> panelPages{};
    std::array<uint8_t,16> ioPages{};unsigned ioPageCount=1;unsigned physicalStore=0;
    // PROGRAM_MENU_DECLARATIONS_BEGIN
    struct ProgramMenuInput {uint64_t frame;unsigned handler,logical;bool pressed;};
    struct ProgramMenuWrite {uint64_t frame;uint32_t address;uint8_t value;};
    std::vector<ProgramMenuInput> programMenuInputs;
    std::vector<ProgramMenuWrite> programMenuWrites;
    size_t programMenuInputCount=0,programMenuWriteCount=0;
    std::array<uint8_t,7> programMenuFinalUi{};
    uint32_t programMenuSelectedSource=0;
    std::array<uint8_t,10> storeMenuFinalUi{};
    std::array<uint8_t,21> storeMenuGlobalsRam{},storeMenuGlobalsFlash{};
    // Probe-only const observations of actual children. They do not add native
    // admissions or change the graph. Parent-return masks are published only
    // after the exact sampled Menu job increments its real completion counter.
    struct SaveWitness {uint32_t kinds=0,observedComplete=0;std::array<uint32_t,15> components{},facts{};};
    struct StoreMenuWitness {
        uint64_t samples=0,sampledJobs=0,returnedJobs=0,started=0,returnsBefore=0;
        bool observing=false;
        SaveWitness sampled{},returned{},pending{};
        uint32_t factory=0,returnedFactory=0,pendingFactory=0,dump=0,returnedDump=0,pendingDump=0;
        uint32_t output=0,returnedOutput=0,pendingOutput=0,playback=0,returnedPlayback=0,pendingPlayback=0;
        uint64_t payloads=0,returnedPayloads=0,pendingPayloads=0,sendBytes=0,returnedSendBytes=0,pendingSendBytes=0;
        bool multi=false,returnedMulti=false,pendingMulti=false;
    } storeMenuWitness;
    const auto mergeSaveWitness=[](SaveWitness&dst,const SaveWitness&source){dst.kinds|=source.kinds;dst.observedComplete|=source.observedComplete;for(unsigned i=0;i<dst.components.size();++i){dst.components[i]|=source.components[i];dst.facts[i]|=source.facts[i];}};
    const auto observeStoreMenu=[&]{
        const auto*runtime=board->nativeController();if(!nativeMode||toggling||!runtime)return;
        const auto&fore=runtime->stateStoreMenuForeground;auto&w=storeMenuWitness;
        if(w.observing&&fore.completed>w.returnsBefore){
            mergeSaveWitness(w.returned,w.pending);w.returnedFactory|=w.pendingFactory;w.returnedDump|=w.pendingDump;w.returnedOutput|=w.pendingOutput;w.returnedPlayback|=w.pendingPlayback;
            w.returnedPayloads+=w.pendingPayloads;w.returnedSendBytes=std::max(w.returnedSendBytes,w.pendingSendBytes);w.returnedMulti|=w.pendingMulti;++w.returnedJobs;w.observing=false;
        }
        const auto*menu=fore.service();if(!menu)return;++w.samples;
        if(!w.observing||w.started!=fore.started){
            w.pending={};w.pendingFactory=w.pendingDump=w.pendingOutput=w.pendingPlayback=0;w.pendingPayloads=w.pendingSendBytes=0;w.pendingMulti=false;
            w.started=fore.started;w.returnsBefore=fore.completed;w.observing=true;++w.sampledJobs;
        }
        const auto save=[&](const ks::StateSaveService*job){if(!job)return;const auto k=unsigned(job->kind());check(k<w.pending.components.size(),"Unknown sampled Menu Save kind");
            w.pending.kinds|=1u<<k;w.pending.components[k]|=job->components();if(job->complete())w.pending.observedComplete|=1u<<k;
            const auto&f=job->summary();w.pending.facts[k]|=unsigned(f.global)|(unsigned(f.page)<<1)|(unsigned(f.reloaded)<<2)|(unsigned(f.packed)<<3)|(unsigned(f.named)<<4)|(unsigned(f.notice)<<5);
        };
        for(unsigned i=0;i<menu->saveCount();++i)save(menu->saveOutcome(i));
        if(const auto*f=menu->factory()){w.pendingFactory|=f->components();save(f->save());save(f->notice());}
        if(const auto*d=menu->dump()){
            w.pendingDump|=d->components();w.pendingPayloads=std::max(w.pendingPayloads,d->summary().sentPayloads);w.pendingMulti|=d->summary().multi;
            if(const auto*o=d->output())w.pendingOutput|=o->components();
            if(const auto*o=d->lastSend()){w.pendingOutput|=o->components();w.pendingSendBytes=std::max(w.pendingSendBytes,uint64_t(o->summary().bytesSent));}
        }
        if(const auto*p=menu->playback())w.pendingPlayback|=p->components();
        mergeSaveWitness(w.sampled,w.pending);w.factory|=w.pendingFactory;w.dump|=w.pendingDump;w.output|=w.pendingOutput;w.playback|=w.pendingPlayback;
        w.payloads=std::max(w.payloads,w.pendingPayloads);w.sendBytes=std::max(w.sendBytes,w.pendingSendBytes);w.multi|=w.pendingMulti;
    };
    const auto returnedMenuSave=[&](ks::StateSaveService::Kind kind,uint32_t requiredComponents){const auto k=unsigned(kind);return (storeMenuWitness.returned.kinds&(1u<<k))&&(storeMenuWitness.returned.components[k]&requiredComponents)==requiredComponents;};
    // PROGRAM_MENU_DECLARATIONS_END
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
        if(panelStore||programMenuFixture){
            events.clear();add(.05,four({0x90,60,101}));add(.07,four({0xb0,105,93}));
            add(.12,four({0xb0,64,127}));add(.16,four({0x80,60,0}));
            add(1.6,four({0xb0,64,0}));add(1.7,four({0x90,67,99}));add(2.,four({0x80,67,0}));
        }
        // Derive the Store key from authenticated descriptors. The physical
        // fixture supplies external pins; the older fixture supplies queue
        // events. Both retain normal UI dispatch and Flash save handling.
        unsigned storeKey=0,storeKeys=0;
        if(panelStore){
            check(!toggling&&mode!="replay","Panel store requires a live reference/native controller");
            check(board->overlay,"Panel fixture requires the active SRAM overlay");
            const auto* memory=board->cpu().memory+0xa00000;unsigned dispatch=0,matches=0;
            for(unsigned key=0;key<32;++key){
                const unsigned at=0xfd1e + key*4;
                const uint32_t target=(uint32_t(memory[at])<<24)|(uint32_t(memory[at+1])<<16)|(uint32_t(memory[at+2])<<8)|memory[at+3];
                if((target&0xffffff)==0x472c){dispatch=key;++matches;}
            }
            check(matches==1,"Store dispatch descriptor is ambiguous");
            for(unsigned key=0;key<32;++key)if((memory[0xfcc4+key*3]&127)==1&&memory[0xfcc4+key*3+1]==dispatch){storeKey=key;++storeKeys;}
            check(storeKeys==1,"Store key descriptor is ambiguous");
            if(panelIo){
                unsigned matches=0;
                for(unsigned key=0;key<48;++key)if(memory[0xb8e8+key]==storeKey){physicalStore=key;++matches;}
                check(matches==1,"Physical Store button mapping is ambiguous");
                ioFixture=std::make_unique<ks::test::PanelIoFixture>(*board);
                if(panelScreen){
                    auto physicalForHandler=[&](unsigned handler){unsigned logical=0,found=0;
                        for(unsigned key=0;key<32;++key){const unsigned descriptor=0xfcc4+key*3;const unsigned action=memory[descriptor]&127;const unsigned index=memory[descriptor+1];const unsigned at=0xfd1e + index*4;
                            const uint32_t target=(uint32_t(memory[at])<<24)|(uint32_t(memory[at+1])<<16)|(uint32_t(memory[at+2])<<8)|memory[at+3];if(action==1&&(target&0xffffff)==handler){logical=key;++found;}}
                        check(found==1,"Screen action descriptor ambiguous");unsigned physical=0;found=0;
                        for(unsigned key=0;key<48;++key)if(memory[0xb8e8+key]==logical){physical=key;++found;}check(found==1,"Screen physical key ambiguous");return physical;};
                    auto physicalLogical=[&](unsigned logical){unsigned physical=0,found=0;for(unsigned key=0;key<48;++key)if(memory[0xb8e8+key]==logical){physical=key;++found;}check(found==1,"Logical physical key ambiguous");return physical;};
                    if(panelActions)ioFixture->configureSequence({physicalForHandler(0x4a76),physicalForHandler(0x4ab8),physicalForHandler(0x4b08),physicalForHandler(0x4b50),physicalForHandler(0x4a32),physicalForHandler(0x4a4c),physicalForHandler(0x4a66),physicalForHandler(0x496c),physicalForHandler(0x4974)},true);
                    else if(panelFunctions)ioFixture->configureSequence({physicalForHandler(0x49b0),physicalLogical(0x61),physicalLogical(0x62),physicalLogical(0x63),physicalLogical(0x64),physicalLogical(0x65),physicalLogical(0x69),physicalLogical(0x60),physicalForHandler(0x49b0)},true);
                    else if(panelKeyboard){const auto key=physicalLogical(0x21);ioFixture->configureSequence({key,key,key,key,key,key,key,key,key});ioFixture->configureKeyboard();}
                    else if(panelParts){
                        auto physicalPart=[&](unsigned logical){unsigned physical=0,found=0;for(unsigned key=0;key<48;++key)if(memory[0xb8e8+key]==logical){physical=key;++found;}check(found==1,"Part physical key ambiguous");return physical;};
                        ioFixture->configureScreens(physicalPart(0x21),physicalPart(0x22),physicalPart(0x41));
                    }else ioFixture->configureScreens(physicalForHandler(0x49b0),physicalForHandler(0x46ec),physicalForHandler(0x46ac),panelEdit);
                }
                board->cpu().memmap(ioFixture.get(),0,1<<24);
                ioPages[0]=board->read(panelParts?0x167b8:panelScreen?0x16787:0x16790);
            }
            if(!panelScreen)board->write(0x168fc+239,1); // Explicit synthetic fixture: writes allowed.
        }
        auto queueStore=[&](bool pressed){
            auto* memory=board->cpu().memory+0xa00000;
            const unsigned producer=(unsigned(memory[0x1537c])<<8)|memory[0x1537d];
            const unsigned consumer=(unsigned(memory[0x1537e])<<8)|memory[0x1537f];
            const unsigned nextProducer=(producer+2)&0x7e;
            check(producer<=0x7e&&!(producer&1)&&nextProducer!=consumer,"Panel event ring full/invalid");
            check(panelEvents<panelPages.size(),"Panel event fixture overflow");panelPages[panelEvents]=memory[0x16790];
            memory[0x15380+producer]=pressed?1:0;memory[0x15381+producer]=uint8_t(storeKey);
            memory[0x1537c]=uint8_t(nextProducer>>8);memory[0x1537d]=uint8_t(nextProducer);++panelEvents;
        };
    // PROGRAM_MENU_PLAN_BEGIN
        if(programMenuFixture){
            check(!toggling&&mode!="replay","Program menu input requires live H8/native controller");
            check(board->overlay,"Program menu fixture requires active SRAM overlay");
            const auto* menuMemory=board->cpu().memory+0xa00000;
            auto logicalForHandler=[&](unsigned handler){unsigned logical=0,matches=0;
                for(unsigned key=0;key<32;++key){const auto descriptor=0xfcc4+key*3;const auto at=0xfd1e + unsigned(menuMemory[descriptor+1])*4;
                    const auto target=(uint32_t(menuMemory[at])<<24)|(uint32_t(menuMemory[at+1])<<16)|(uint32_t(menuMemory[at+2])<<8)|menuMemory[at+3];
                    if((menuMemory[descriptor]&127)==1&&(target&0xffffff)==handler){logical=key;++matches;}}
                check(matches==1,"Program menu button descriptor ambiguous");return logical;};
            auto menuWrite=[&](double at,uint32_t address,uint8_t value){programMenuWrites.push_back({native(at),address,value});};
            auto menuButton=[&](double at,unsigned handler){const auto logical=logicalForHandler(handler);programMenuInputs.push_back({native(at),handler,logical,true});programMenuInputs.push_back({native(at+.04),handler,logical,false});};
            auto context=[&](double at,uint8_t page,uint8_t special){
                for(auto address:{0x16784u,0x16791u,0x16792u,0x167d5u})menuWrite(at,address,0);
                menuWrite(at,0x16790,page);menuWrite(at,0x1679e,special);menuWrite(at,0x1679f,0);
            };
            if(programStoreMenuFixture){
                // Real queued Store472C selected through the immutable ROM
                // descriptor. The same explicit UI/data inputs reach H8/native.
                // Hardware1 makes the normal ScreenGate590A reject calibration.
                programMenuSelectedSource=scenario=="panel-menu-global-save"?0x169e6:scenario=="panel-menu-dump-global"?0x81ff00:scenario=="panel-menu-dump-multi"?0x169fc:0x81fe00;
                context(.18,0,0);menuWrite(.18,0xffec14,1);
                menuWrite(.18,0x16784,1);menuWrite(.18,0x16785,uint8_t(scenario=="panel-menu-global-save"?0:9));
                menuWrite(.18,0x16787,uint8_t(scenario=="panel-menu-factory"));
                menuWrite(.18,0x169eb,1);
                if(scenario=="panel-menu-dump-global")menuWrite(.18,0x169e1,12);
                if(scenario=="panel-menu-dump-multi")menuWrite(.18,0x169e1,0);
                menuButton(.2,0x472c);
            }
            else if(programBrowserFixture){
                // Explicit queued user navigation with authenticated button descriptors.
                // These are UI/data mutations; no execution state is supplied.
                context(.18,0,0);menuWrite(.18,0x16799,uint8_t(scenario=="panel-browse-part"));menuWrite(.18,0x167fb,0);
                menuWrite(.18,0x167d5,uint8_t(scenario=="panel-browse-multi"));
                for(unsigned i=0;i<4;++i)menuButton(.2+i*.4,i&1?0x46ac:0x46ec);
            }
            else if(programMenuSingle||programMenuToMulti){
                context(.18,0,0);
                // Normal user-selected stored destinations, independent of CPU fields.
                menuWrite(.18,0x167ac,1);menuWrite(.18,0x167ad,3);menuWrite(.18,0x167ae,1);menuWrite(.18,0x167af,0);
                const auto handler=programMenuSingle?0x4864u:0x4924u;menuButton(.2,handler);menuButton(.8,handler);
            }
            else {
                const auto page=uint8_t(scenario=="panel-part-name"?2:scenario=="panel-part-category"?3:0);
                const bool special=scenario=="panel-part-special";
                context(.18,page,uint8_t(special));
                menuWrite(.18,0x16799,1);menuWrite(.18,0x167d7,0);menuWrite(.18,0x167da,0);
                if(special){
                    // Select an existing live working-data image, never an execution address.
                    for(unsigned i=0;i<4;++i)programMenuSelectedSource=(programMenuSelectedSource<<8)|uint8_t(board->read(0x167bc+i));
                    for(unsigned i=0;i<4;++i)menuWrite(.18,0x1748e + i,uint8_t(programMenuSelectedSource>>(24-i*8)));
                    menuWrite(.18,0x17492,1);menuWrite(.18,0x17493,0);
                }
                for(unsigned i=0;i<4;++i){if(i)context(.18+i*.4,page,uint8_t(special));menuButton(.2+i*.4,i&1?0x4974:0x496c);}
            }
            std::stable_sort(programMenuWrites.begin(),programMenuWrites.end(),[](const auto&x,const auto&y){return x.frame<y.frame;});
        }
        auto queueProgramMenu=[&](unsigned logical,bool pressed){
            auto* menuMemory=board->cpu().memory+0xa00000;
            const unsigned producer=(unsigned(menuMemory[0x1537c])<<8)|menuMemory[0x1537d],consumer=(unsigned(menuMemory[0x1537e])<<8)|menuMemory[0x1537f],nextProducer=(producer+2)&0x7e;
            check(producer<=0x7e&&!(producer&1)&&nextProducer!=consumer,"Program menu button ring full/invalid");
            menuMemory[0x15380+producer]=pressed?1:0;menuMemory[0x15381+producer]=uint8_t(logical);
            menuMemory[0x1537c]=uint8_t(nextProducer>>8);menuMemory[0x1537d]=uint8_t(nextProducer);
        };
    // PROGRAM_MENU_PLAN_END
        std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.frame<b.frame;});
        if(liveMode){if(!nativeMode)board->enableLiveMidiController();board->resetControllerMetrics();if(recording)board->enableControllerDelegationProfile();}
        memoryStart=memoryDigests();
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
    // PROGRAM_MENU_APPLY_BEGIN
            while(programMenuWriteCount<programMenuWrites.size()&&programMenuWrites[programMenuWriteCount].frame==n){const auto&w=programMenuWrites[programMenuWriteCount++];board->write(w.address,w.value);}
            while(programMenuInputCount<programMenuInputs.size()&&programMenuInputs[programMenuInputCount].frame==n){const auto&e=programMenuInputs[programMenuInputCount++];queueProgramMenu(e.logical,e.pressed);}
    // PROGRAM_MENU_APPLY_END
            if(panelIo)ioFixture->apply(n,ks::DSP::SampleRate,physicalStore,multi);
            if(panelPartSave){if(n==native(.2)){board->write(0x167ac,1);board->write(0x167ad,3);board->write(0x16790,3);queueStore(true);}if(n==native(.24))queueStore(false);}
            if(panelStore&&!panelIo&&!panelPartSave)for(unsigned press=0;press<(multi?3u:4u);++press){
                if(n==native(.2+press*.35))queueStore(true);
                if(n==native(.24+press*.35))queueStore(false);
            }
            if(mode!="replay")while(next<events.size() && events[next].frame==n)send(events[next++].bytes);
            observeStoreMenu();
            const auto a=board->renderSample();
            observeStoreMenu();
            if(panelIo){const auto page=board->read(panelParts?0x167b8:panelScreen?0x16787:0x16790);if(page!=ioPages[ioPageCount-1]){check(ioPageCount<ioPages.size(),"Too many physical panel page transitions");ioPages[ioPageCount++]=page;}}
            if(!captured.empty())captured[n]=a;for(auto v:a){hash^=uint32_t(v);hash*=1099511628211ull;peak=std::max(peak,std::abs(int64_t(v)));}
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
            const auto* runtime=board->nativeController();
            // Complete playback graphs own ordinary part release internally.
            // Accept that recorded role only after the graph has returned;
            // retain the raw legacy release counter and all other coverage gates.
            const bool playbackReleaseCovered=nativeMode&&!toggling&&runtime
                &&runtime->statePlaybackModeForeground.completed>0&&!runtime->statePlaybackModeForeground.active()
                && (runtime->statePlaybackModeForeground.components&(1u<<unsigned(ks::StatePlaybackModeService::Component::Voices)));
            const bool menuPlaybackReleaseCovered=nativeMode&&!toggling&&runtime
                &&storeMenuWitness.returnedJobs>0
                &&(storeMenuWitness.returnedPlayback&(1u<<unsigned(ks::StatePlaybackModeService::Component::Voices)));
            for(unsigned i=0;i<last.size();++i)if(!last[i]&&!beforeSwitch[i]&&!(i==6&&(playbackReleaseCovered||menuPlaybackReleaseCovered)))
                throw std::runtime_error("Live handler coverage absent at index "+std::to_string(i));
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
    if(nativeMode && !toggling)check(!board->liveMidiController()->startupCalls.partialEntries && !board->liveMidiController()->voiceActions.selectCalls.partialEntries,"Native playback used partial startup/selection recovery");
    if(nativeMode && !toggling)check(!board->liveMidiController()->voiceActions.routingCalls.partialEntries,"Native playback used partial routing recovery");
    if(nativeMode && !toggling)check(!board->liveMidiController()->noteActions.partControlCalls.partialEntries && !board->liveMidiController()->voiceActions.partControlCalls.partialEntries,"Native playback used partial part-control recovery");
    if(nativeMode && !toggling)check(!board->liveMidiController()->voiceActions.velocityCurveCalls.partialEntries,"Native playback used partial velocity recovery");
    if(nativeMode && !toggling)check(!board->liveMidiController()->voiceActions.selectionCalls.partialEntries,"Native playback used partial selection/gate recovery");
    if(nativeMode && !toggling)check(!board->liveMidiController()->voiceActions.populationCalls.partialEntries,"Native playback used partial population recovery");
    if(nativeMode && !toggling)check(board->liveMidiController()->bootSetupCalls.partialEntries==0,"Native playback used partial bootstrap setup recovery");
    if(nativeMode && !toggling)check(board->liveMidiController()->bootStreamCalls.partialEntries==0,"Native playback used partial bootstrap-stream recovery");
    if(nativeMode && !toggling)check(board->liveMidiController()->faultCalls.partialEntries==0,"Native playback used partial fault recovery");
    if(nativeMode && !toggling)check(board->liveMidiController()->voiceActions.addressCalls.partialEntries==0,"Native playback used partial DSP-address recovery");
    if(nativeMode && !toggling)check(board->liveMidiController()->voiceActions.wordCalls.partialEntries==0,"Native playback used partial DSP-word recovery");
    if(nativeMode && !toggling)check(board->liveMidiController()->parameterScaleCalls.partialEntries==0,"Native playback used partial coefficient recovery");
    if(nativeMode&&!toggling){
        const auto& live=*board->liveMidiController();
        const auto admitted=[](const auto& calls){
            check(!calls.partialEntries,"Native panel used partial recovery");
            check(calls.started+calls.initialActive==calls.completed+calls.activeCalls(),"Native panel call lifetime differs");
        };
        admitted(live.panelMatrixCalls);admitted(live.pedalSenseCalls);
        admitted(live.panelEncoderCalls);admitted(live.panelAdcSamplesCalls);
        admitted(live.panelAdcScaleCalls);admitted(live.panelAdcCalibrationCalls);
        admitted(live.panelAdcIrqCalls);admitted(live.panelAdcScanCalls);
    }
    // PROGRAM_MENU_FINAL_BEGIN
    if(programMenuFixture){
        check(programMenuInputCount==programMenuInputs.size()&&programMenuWriteCount==programMenuWrites.size(),"Missing explicit program menu input/mutation");
        constexpr unsigned addresses[]{0x16790,0x16799,0x167ba,0x167d7,0x167da,0x1679e,0x17493};
        for(unsigned i=0;i<7;++i)programMenuFinalUi[i]=uint8_t(board->read(addresses[i]));
        if(programMenuSingle)check(programMenuFinalUi[2]==0,"Queued Single button did not enter Single");
        if(programMenuToMulti)check(programMenuFinalUi[2]==1,"Queued Multi button did not enter Multi");
        if(scenario=="panel-part-wrap-multi")check(programMenuFinalUi[1]==1,"Part wrap sequence did not return to part1");
    }
    if(programStoreMenuFixture){
        constexpr unsigned addresses[]{0x16784,0x16785,0x16787,0x16790,0x16792,0x1679e,0x1679f,0x167ba,0x169e1,0xffec14};
        for(unsigned i=0;i<storeMenuFinalUi.size();++i)storeMenuFinalUi[i]=uint8_t(board->read(addresses[i]));
        for(unsigned i=0;i<storeMenuGlobalsRam.size();++i){storeMenuGlobalsRam[i]=uint8_t(board->read(0x169e6 + i));storeMenuGlobalsFlash[i]=uint8_t(board->read(0x81ff00 + i));}
        if(scenario=="panel-menu-global-save"||scenario=="panel-menu-factory")check(!storeMenuFinalUi[0]&&!storeMenuFinalUi[4],"Queued Store menu did not clear edit/latch");
        else check(storeMenuFinalUi[0]==1&&storeMenuFinalUi[1]==9&&storeMenuFinalUi[2]==0,"Queued dump menu context differs");
        if(nativeMode){const auto&menu=board->nativeController()->stateStoreMenuForeground;
            check(menu.kinds[0]>0&&menu.completed>0&&!menu.active()&&storeMenuWitness.returnedJobs>0,"Queued Store menu ownership/return absent");
            if(scenario=="panel-menu-global-save")check(returnedMenuSave(ks::StateSaveService::Kind::GlobalSave,(1u<<9)|(1u<<5)|(1u<<7))&&(storeMenuWitness.returned.facts[9]&3u)==3u,"Global Save typed completed Page comparison absent");
            if(scenario=="panel-menu-factory")check((storeMenuWitness.returnedFactory&15u)==15u&&returnedMenuSave(ks::StateSaveService::Kind::Offset,(1u<<4)|(1u<<5)|(1u<<7)),"Factory copy/save/globals/notice typed chain absent");
            if(scenario=="panel-menu-dump-global"||scenario=="panel-menu-dump-multi"){
                check(storeMenuWitness.returnedPayloads==1&&storeMenuWitness.returnedSendBytes==(scenario=="panel-menu-dump-global"?256u:1024u),"Bounded dump payload absent");
                if(scenario=="panel-menu-dump-multi")check(storeMenuWitness.returnedMulti,"Multi serialization before dump absent");
            }
        }
    }
    // PROGRAM_MENU_FINAL_END
    if(panelPartSave&&!board->flashRevision)std::cerr<<"Part-save outcome page="<<unsigned(board->read(0x16790))<<" program="<<std::hex<<unsigned(board->read(0x167ac))<<':'<<unsigned(board->read(0x167ad))<<" bank="<<unsigned(board->read(0x167ab))<<" mode="<<unsigned(board->read(0x167ba))<<std::dec<<'\n';
    if(panelStore&&!panelScreen){if(!panelIo)check(panelEvents==(panelPartSave?2u:multi?6u:8u),"Missing panel store events");check(board->flashRevision>0,"Panel store did not write Flash");check(board->read(0x16790)==0,"Store page did not return to playback");}
    if(panelPartSave&&nativeMode){const auto&save=board->nativeController()->stateSaveForeground;
        check((save.kinds[1]&&(save.components&(1u<<16)))||returnedMenuSave(ks::StateSaveService::Kind::CommitSingle,1u<<16),"Part save did not run the full native reload");
    }
    if(scenario=="state")check(board->flashRevision>0,"State scenario did not complete a Flash page write");
    if(panelKeyboard)check(ioFixture->keyboardReads>0&&ioFixture->keyboardTransitions==12,"Keyboard contact source not observed");
    if(panelParts&&nativeMode)check(board->liveMidiController()->statePartSelectionCalls.started>0,"Part handlers were not observed");
    if(panelEdit)check(ioFixture->encoderTransitions>0,"Active screen edit did not move encoder");
    if(panelIo){check(ioFixture->matrixReads&&ioFixture->pedalReads&&ioFixture->encoderReads&&ioFixture->adcReads,"Physical input source not observed");check(ioFixture->buttonTransitions==(panelScreen?18u:(multi?6u:8u)),"Missing physical button transitions");check(ioFixture->pedalTransitions==4&&(panelScreen||(ioFixture->encoderTransitions&&ioFixture->adcTransitions)),"Missing physical pedal/encoder/ADC transitions");}
    const auto memoryEnd=memoryDigests();
    const auto depth=board->serialDepth();std::ostringstream report;report<<std::setprecision(12)
        <<"{\"schema\":\"silver-k.controller-probe.v1\",\"mode\":\""<<mode<<"\",\"scenario\":\""<<scenario<<"\",\"controller_backend\":\""<<board->controllerBackend()
        <<"\",\"dsp_threads\":"<<(parallel?2:1)<<",\"frames\":"<<frames<<",\"audio_hash\":\""<<hash<<"\",\"peak\":"<<peak
        <<",\"cpu_seconds\":"<<cost<<",\"wall_seconds\":"<<wall<<",\"setup_wall_seconds\":"<<std::chrono::duration<double>(Clock::now()-setup).count()-wall
        <<",\"h8_start_cycles\":"<<h8Start<<",\"h8_end_cycles\":"<<board->cpu().cycles<<",\"controller_end_cycles\":"<<board->controllerCycles()
        <<",\"resource_start_digest\":\""<<memoryStart[0]<<"\",\"resource_end_digest\":\""<<memoryEnd[0]<<"\""
        <<",\"cpu_scratch_start_digest\":\""<<memoryStart[1]<<"\",\"cpu_scratch_end_digest\":\""<<memoryEnd[1]<<"\""
        <<",\"dsp_a_cycles\":"<<board->dsp(0).cycles()<<",\"dsp_b_cycles\":"<<board->dsp(1).cycles()
        <<",\"dsp_a_instructions\":"<<board->dsp(0).instructions()<<",\"dsp_b_instructions\":"<<board->dsp(1).instructions()
        <<",\"serial_depth\":["<<depth[0]<<','<<depth[1]<<"],\"serial_underruns\":["<<board->serialUnderruns[0]<<','<<board->serialUnderruns[1]
        <<"],\"serial_overflows\":["<<board->serialOverflows[0]<<','<<board->serialOverflows[1]<<']'
        <<",\"checked_ports\":"<<board->checkedControllerPorts()
        <<",\"state_digest\":"<<snapshotDigest(stateBytes)<<",\"flash_digest\":"<<snapshotDigest(flashBytes)
        <<",\"flash_revision\":"<<board->flashRevision<<",\"midi_tx_digest\":"<<snapshotDigest(midiOut)<<",\"midi_tx_bytes\":"<<midiOut.size()
        <<",\"midi_received\":"<<board->midiReceived
        <<",\"panel_button_events\":"<<panelEvents
        <<",\"native_startup_calls_before_metrics\":"<<startupStarts
        <<",\"native_startup_returns_before_metrics\":"<<startupReturns
        <<",\"native_dsp_reset_returns_before_metrics\":"<<resetReturns
        <<",\"native_startup_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->startupCalls.partialEntries:0)
        <<",\"native_voice_start_jobs\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.startCallsNative.started:0)
        <<",\"native_voice_start_finishes\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.startCallsNative.completed:0)
        <<",\"native_voice_start_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.startCallsNative.handoffs:0)
        <<",\"native_voice_start_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.startCallsNative.initialActive:0)
        <<",\"native_voice_start_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.startCallsNative.activeCalls():0)
        <<",\"native_voice_start_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.startCallsNative.partialEntries:0)
        <<",\"native_glide_jobs\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.glideCalls.started:0)
        <<",\"native_glide_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.glideCalls.completed:0)
        <<",\"native_glide_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.glideCalls.initialActive:0)
        <<",\"native_glide_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.glideCalls.activeCalls():0)
        <<",\"native_glide_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.glideCalls.partialEntries:0)
        <<",\"native_velocity_modulation_jobs\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.modulationCalls.started:0)
        <<",\"native_velocity_modulation_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.modulationCalls.completed:0)
        <<",\"native_velocity_modulation_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.modulationCalls.initialActive:0)
        <<",\"native_velocity_modulation_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.modulationCalls.activeCalls():0)
        <<",\"native_velocity_modulation_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.modulationCalls.partialEntries:0)
        <<",\"native_tracking_jobs\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.trackingCalls.started:0)
        <<",\"native_tracking_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.trackingCalls.completed:0)
        <<",\"native_tracking_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.trackingCalls.initialActive:0)
        <<",\"native_tracking_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.trackingCalls.activeCalls():0)
        <<",\"native_tracking_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.trackingCalls.partialEntries:0)
        <<",\"native_unison_jobs\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.unisonCalls.started:0)
        <<",\"native_unison_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.unisonCalls.completed:0)
        <<",\"native_unison_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.unisonCalls.partialEntries:0)
        <<",\"native_release_jobs\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.releaseCalls.started:0)
        <<",\"native_release_job_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.releaseCalls.completed:0)
        <<",\"native_release_restarts\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.releaseCalls.restarted:0)
        <<",\"native_release_abandoned\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.releaseCalls.abandoned:0)
        <<",\"native_release_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.releaseCalls.partialEntries:0)
        <<",\"native_oscillator_modulation_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.oscillatorModulationCalls.started:0)
        <<",\"native_oscillator_modulation_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.oscillatorModulationCalls.completed:0)
        <<",\"native_oscillator_modulation_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.oscillatorModulationCalls.partialEntries:0)
        <<",\"native_global_modulation_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalModulationCalls.started:0)
        <<",\"native_global_modulation_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalModulationCalls.completed:0)
        <<",\"native_global_modulation_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalModulationCalls.partialEntries:0)
        <<",\"native_voice_sweep_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.sweepCalls.started:0)
        <<",\"native_voice_sweep_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.sweepCalls.completed:0)
        <<",\"native_voice_sweep_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.sweepCalls.partialEntries:0)
        <<",\"native_key_priority_compact_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyPriorityCalls.compactStarted:0)
        <<",\"native_key_priority_compact_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyPriorityCalls.compactCompleted:0)
        <<",\"native_key_priority_mono_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyPriorityCalls.monoStarted:0)
        <<",\"native_key_priority_mono_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyPriorityCalls.monoCompleted:0)
        <<",\"native_key_priority_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyPriorityCalls.partialEntries:0)
        <<",\"native_oscillator_pitch_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.oscillatorPitchCalls.started:0)
        <<",\"native_oscillator_pitch_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.oscillatorPitchCalls.completed:0)
        <<",\"native_oscillator_pitch_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.oscillatorPitchCalls.partialEntries:0)
        <<",\"native_parameter_pitch_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterPitchCalls.started:0)
        <<",\"native_parameter_pitch_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterPitchCalls.completed:0)
        <<",\"native_parameter_pitch_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterPitchCalls.partialEntries:0)
        <<",\"native_parameter_modulation_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterModulationCalls.started:0)
        <<",\"native_parameter_modulation_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterModulationCalls.completed:0)
        <<",\"native_parameter_modulation_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterModulationCalls.partialEntries:0)
        <<",\"native_voice_refresh_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.refreshCalls.started:0)
        <<",\"native_voice_refresh_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.refreshCalls.completed:0)
        <<",\"native_voice_refresh_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.refreshCalls.partialEntries:0)
        <<",\"native_global_setup_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalSetupCalls.started:0)
        <<",\"native_global_setup_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalSetupCalls.completed:0)
        <<",\"native_global_setup_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalSetupCalls.partialEntries:0)
        <<",\"native_note_off_state_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteOffStateCalls.started:0)
        <<",\"native_note_off_state_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteOffStateCalls.completed:0)
        <<",\"native_note_off_state_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteOffStateCalls.partialEntries:0)
        <<",\"native_parameter_scalar_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterScalarCalls.started:0)
        <<",\"native_parameter_scalar_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterScalarCalls.completed:0)
        <<",\"native_parameter_scalar_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterScalarCalls.partialEntries:0)
        <<",\"native_global_scalar_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalScalarCalls.started:0)
        <<",\"native_global_scalar_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalScalarCalls.completed:0)
        <<",\"native_global_scalar_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalScalarCalls.partialEntries:0)
        <<",\"native_global_time_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTimeCalls.started:0)
        <<",\"native_global_time_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTimeCalls.completed:0)
        <<",\"native_global_time_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTimeCalls.partialEntries:0)
        <<",\"native_parameter_time_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterTimeCalls.started:0)
        <<",\"native_parameter_time_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterTimeCalls.completed:0)
        <<",\"native_parameter_time_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterTimeCalls.partialEntries:0)
        <<",\"native_parameter_curve_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterCurveCalls.started:0)
        <<",\"native_parameter_curve_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterCurveCalls.completed:0)
        <<",\"native_parameter_curve_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterCurveCalls.partialEntries:0)
        <<",\"native_parameter_voice_scale_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterVoiceScaleCalls.started:0)
        <<",\"native_parameter_voice_scale_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterVoiceScaleCalls.completed:0)
        <<",\"native_parameter_voice_scale_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterVoiceScaleCalls.partialEntries:0)
        <<",\"native_global_shape_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalShapeCalls.started:0)
        <<",\"native_global_shape_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalShapeCalls.completed:0)
        <<",\"native_global_shape_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalShapeCalls.partialEntries:0)
        <<",\"native_global_range_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalRangeCalls.started:0)
        <<",\"native_global_range_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalRangeCalls.completed:0)
        <<",\"native_global_range_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalRangeCalls.partialEntries:0)
        <<",\"native_part_note_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.partNoteCalls.started:0)
        <<",\"native_part_note_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.partNoteCalls.completed:0)
        <<",\"native_part_note_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.partNoteCalls.partialEntries:0)
        <<",\"native_pedal_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.pedalNativeCalls.started:0)
        <<",\"native_pedal_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.pedalNativeCalls.completed:0)
        <<",\"native_pedal_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.pedalNativeCalls.partialEntries:0)
        <<",\"native_note_program_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteProgramCalls.started:0)
        <<",\"native_note_program_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteProgramCalls.completed:0)
        <<",\"native_note_program_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteProgramCalls.partialEntries:0)
        <<",\"native_parameter_scalar_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterScalarCalls.initialActive:0)
        <<",\"native_parameter_scalar_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterScalarCalls.activeCalls():0)
        <<",\"native_parameter_pitch_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterPitchCalls.initialActive:0)
        <<",\"native_parameter_pitch_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterPitchCalls.activeCalls():0)
        <<",\"native_parameter_modulation_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterModulationCalls.initialActive:0)
        <<",\"native_parameter_modulation_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterModulationCalls.activeCalls():0)
        <<",\"native_parameter_time_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterTimeCalls.initialActive:0)
        <<",\"native_parameter_time_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterTimeCalls.activeCalls():0)
        <<",\"native_parameter_curve_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterCurveCalls.initialActive:0)
        <<",\"native_parameter_curve_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterCurveCalls.activeCalls():0)
        <<",\"native_parameter_voice_scale_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterVoiceScaleCalls.initialActive:0)
        <<",\"native_parameter_voice_scale_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterVoiceScaleCalls.activeCalls():0)
        <<",\"native_global_setup_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalSetupCalls.initialActive:0)
        <<",\"native_global_setup_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalSetupCalls.activeCalls():0)
        <<",\"native_global_scalar_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalScalarCalls.initialActive:0)
        <<",\"native_global_scalar_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalScalarCalls.activeCalls():0)
        <<",\"native_global_time_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTimeCalls.initialActive:0)
        <<",\"native_global_time_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTimeCalls.activeCalls():0)
        <<",\"native_global_shape_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalShapeCalls.initialActive:0)
        <<",\"native_global_shape_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalShapeCalls.activeCalls():0)
        <<",\"native_global_range_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalRangeCalls.initialActive:0)
        <<",\"native_global_range_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalRangeCalls.activeCalls():0)
        <<",\"native_part_note_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.partNoteCalls.initialActive:0)
        <<",\"native_part_note_active\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.partNoteCalls.activeCalls():0)
        <<",\"native_pedal_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.pedalNativeCalls.initialActive:0)
        <<",\"native_pedal_active\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.pedalNativeCalls.activeCalls():0)
        <<",\"native_note_program_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteProgramCalls.initialActive:0)
        <<",\"native_note_program_active\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteProgramCalls.activeCalls():0)
        <<",\"native_pedal_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.pedalNativeCalls.handedOff:0)
        <<",\"native_parameter_header_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterHeaderCalls.started:0)
        <<",\"native_parameter_header_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterHeaderCalls.completed:0)
        <<",\"native_parameter_header_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterHeaderCalls.partialEntries:0)
        <<",\"native_parameter_header_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterHeaderCalls.initialActive:0)
        <<",\"native_parameter_header_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterHeaderCalls.activeCalls():0)
        <<",\"native_parameter_signed_square_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterSignedSquareCalls.started:0)
        <<",\"native_parameter_signed_square_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterSignedSquareCalls.completed:0)
        <<",\"native_parameter_signed_square_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterSignedSquareCalls.partialEntries:0)
        <<",\"native_parameter_signed_square_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterSignedSquareCalls.initialActive:0)
        <<",\"native_parameter_signed_square_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterSignedSquareCalls.activeCalls():0)
        <<",\"native_parameter_filter_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterFilterCalls.started:0)
        <<",\"native_parameter_filter_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterFilterCalls.completed:0)
        <<",\"native_parameter_filter_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterFilterCalls.partialEntries:0)
        <<",\"native_parameter_filter_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterFilterCalls.initialActive:0)
        <<",\"native_parameter_filter_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterFilterCalls.activeCalls():0)
        <<",\"native_parameter_volume_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterVolumeCalls.started:0)
        <<",\"native_parameter_volume_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterVolumeCalls.completed:0)
        <<",\"native_parameter_volume_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterVolumeCalls.partialEntries:0)
        <<",\"native_parameter_volume_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterVolumeCalls.initialActive:0)
        <<",\"native_parameter_volume_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterVolumeCalls.activeCalls():0)
        <<",\"native_parameter_gate_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterGateCalls.started:0)
        <<",\"native_parameter_gate_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterGateCalls.completed:0)
        <<",\"native_parameter_gate_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterGateCalls.partialEntries:0)
        <<",\"native_parameter_gate_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterGateCalls.initialActive:0)
        <<",\"native_parameter_gate_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterGateCalls.activeCalls():0)
        <<",\"native_parameter_support_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterSupportCalls.started:0)
        <<",\"native_parameter_support_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterSupportCalls.completed:0)
        <<",\"native_parameter_support_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterSupportCalls.partialEntries:0)
        <<",\"native_parameter_support_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterSupportCalls.initialActive:0)
        <<",\"native_parameter_support_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterSupportCalls.activeCalls():0)
        <<",\"native_parameter_lfo_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterLfoCalls.started:0)
        <<",\"native_parameter_lfo_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterLfoCalls.completed:0)
        <<",\"native_parameter_lfo_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterLfoCalls.partialEntries:0)
        <<",\"native_parameter_lfo_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterLfoCalls.initialActive:0)
        <<",\"native_parameter_lfo_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.parameterLfoCalls.activeCalls():0)
        <<",\"native_global_mix_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalMixCalls.started:0)
        <<",\"native_global_mix_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalMixCalls.completed:0)
        <<",\"native_global_mix_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalMixCalls.partialEntries:0)
        <<",\"native_global_mix_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalMixCalls.initialActive:0)
        <<",\"native_global_mix_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalMixCalls.activeCalls():0)
        <<",\"native_global_table_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTableCalls.started:0)
        <<",\"native_global_table_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTableCalls.completed:0)
        <<",\"native_global_table_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTableCalls.partialEntries:0)
        <<",\"native_global_table_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTableCalls.initialActive:0)
        <<",\"native_global_table_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTableCalls.activeCalls():0)
        <<",\"native_square_root_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.squareRootCalls.started:0)
        <<",\"native_square_root_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.squareRootCalls.completed:0)
        <<",\"native_square_root_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.squareRootCalls.partialEntries:0)
        <<",\"native_square_root_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.squareRootCalls.initialActive:0)
        <<",\"native_square_root_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.squareRootCalls.activeCalls():0)
        <<",\"native_global_matrix_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalMatrixCalls.started:0)
        <<",\"native_global_matrix_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalMatrixCalls.completed:0)
        <<",\"native_global_matrix_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalMatrixCalls.partialEntries:0)
        <<",\"native_global_matrix_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalMatrixCalls.initialActive:0)
        <<",\"native_global_matrix_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalMatrixCalls.activeCalls():0)
        <<",\"native_global_clamped_time_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalClampedTimeCalls.started:0)
        <<",\"native_global_clamped_time_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalClampedTimeCalls.completed:0)
        <<",\"native_global_clamped_time_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalClampedTimeCalls.partialEntries:0)
        <<",\"native_global_clamped_time_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalClampedTimeCalls.initialActive:0)
        <<",\"native_global_clamped_time_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalClampedTimeCalls.activeCalls():0)
        <<",\"native_global_pair_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalPairCalls.started:0)
        <<",\"native_global_pair_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalPairCalls.completed:0)
        <<",\"native_global_pair_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalPairCalls.partialEntries:0)
        <<",\"native_global_pair_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalPairCalls.initialActive:0)
        <<",\"native_global_pair_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalPairCalls.activeCalls():0)
        <<",\"native_global_tail_scalar_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTailScalarCalls.started:0)
        <<",\"native_global_tail_scalar_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTailScalarCalls.completed:0)
        <<",\"native_global_tail_scalar_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTailScalarCalls.partialEntries:0)
        <<",\"native_global_tail_scalar_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTailScalarCalls.initialActive:0)
        <<",\"native_global_tail_scalar_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTailScalarCalls.activeCalls():0)
        <<",\"native_global_time_tail_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTimeTailCalls.started:0)
        <<",\"native_global_time_tail_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTimeTailCalls.completed:0)
        <<",\"native_global_time_tail_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTimeTailCalls.partialEntries:0)
        <<",\"native_global_time_tail_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTimeTailCalls.initialActive:0)
        <<",\"native_global_time_tail_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalTimeTailCalls.activeCalls():0)
        <<",\"native_global_finish_scalar_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalFinishScalarCalls.started:0)
        <<",\"native_global_finish_scalar_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalFinishScalarCalls.completed:0)
        <<",\"native_global_finish_scalar_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalFinishScalarCalls.partialEntries:0)
        <<",\"native_global_finish_scalar_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalFinishScalarCalls.initialActive:0)
        <<",\"native_global_finish_scalar_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalFinishScalarCalls.activeCalls():0)
        <<",\"native_global_final_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalFinalCalls.started:0)
        <<",\"native_global_final_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalFinalCalls.completed:0)
        <<",\"native_global_final_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalFinalCalls.partialEntries:0)
        <<",\"native_global_final_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalFinalCalls.initialActive:0)
        <<",\"native_global_final_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalFinalCalls.activeCalls():0)
        <<",\"native_midi_decode_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiDecodeCalls.started:0)
        <<",\"native_midi_decode_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiDecodeCalls.completed:0)
        <<",\"native_midi_decode_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->midiDecodeCalls.handedOff:0)
        <<",\"native_midi_decode_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->midiDecodeCalls.partialEntries:0)
        <<",\"native_midi_decode_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiDecodeCalls.initialActive:0)
        <<",\"native_midi_decode_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiDecodeCalls.activeCalls():0)
        <<",\"native_midi_decode_descriptor_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiDecodeCalls.descriptorStarted:0)
        <<",\"native_midi_decode_descriptor_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiDecodeCalls.descriptorCompleted:0)
        <<",\"native_midi_decode_initial_descriptors\":"<<(board->liveMidiController()?board->liveMidiController()->midiDecodeCalls.initialDescriptors:0)
        <<",\"native_midi_decode_active_descriptors\":"<<(board->liveMidiController()?board->liveMidiController()->midiDecodeCalls.activeDescriptors():0)
        <<",\"native_midi_control_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiControlCalls.started:0)
        <<",\"native_midi_control_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiControlCalls.completed:0)
        <<",\"native_midi_control_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->midiControlCalls.handedOff:0)
        <<",\"native_midi_control_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->midiControlCalls.partialEntries:0)
        <<",\"native_midi_control_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiControlCalls.initialActive:0)
        <<",\"native_midi_control_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiControlCalls.activeCalls():0)
        <<",\"native_midi_aux_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiAuxCalls.started:0)
        <<",\"native_midi_aux_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiAuxCalls.completed:0)
        <<",\"native_midi_aux_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->midiAuxCalls.handedOff:0)
        <<",\"native_midi_aux_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->midiAuxCalls.partialEntries:0)
        <<",\"native_midi_aux_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiAuxCalls.initialActive:0)
        <<",\"native_midi_aux_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiAuxCalls.activeCalls():0)
        <<",\"native_midi_part_init_part_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiPartInitCalls.partStarted:0)
        <<",\"native_midi_part_init_part_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiPartInitCalls.partCompleted:0)
        <<",\"native_midi_part_init_control_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiPartInitCalls.controlStarted:0)
        <<",\"native_midi_part_init_control_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiPartInitCalls.controlCompleted:0)
        <<",\"native_midi_part_init_initial_parts\":"<<(board->liveMidiController()?board->liveMidiController()->midiPartInitCalls.initialParts:0)
        <<",\"native_midi_part_init_initial_controls\":"<<(board->liveMidiController()?board->liveMidiController()->midiPartInitCalls.initialControls:0)
        <<",\"native_midi_part_init_active_parts\":"<<(board->liveMidiController()?board->liveMidiController()->midiPartInitCalls.activeParts():0)
        <<",\"native_midi_part_init_active_controls\":"<<(board->liveMidiController()?board->liveMidiController()->midiPartInitCalls.activeControls():0)
        <<",\"native_midi_part_init_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->midiPartInitCalls.partialEntries:0)
        <<",\"native_midi_route_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiRouteCalls.started:0)
        <<",\"native_midi_route_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiRouteCalls.completed:0)
        <<",\"native_midi_route_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiRouteCalls.initialActive:0)
        <<",\"native_midi_route_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiRouteCalls.activeCalls():0)
        <<",\"native_midi_route_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->midiRouteCalls.partialEntries:0)
        <<",\"native_midi_route_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->midiRouteCalls.handedOff:0)
        <<",\"native_state_field_map_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateFieldMapCalls.started:0)
        <<",\"native_state_field_map_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateFieldMapCalls.completed:0)
        <<",\"native_state_field_map_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateFieldMapCalls.initialActive:0)
        <<",\"native_state_field_map_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateFieldMapCalls.activeCalls():0)
        <<",\"native_state_field_map_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateFieldMapCalls.partialEntries:0)
        <<",\"native_global_effect_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalEffectCalls.started:0)
        <<",\"native_global_effect_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalEffectCalls.completed:0)
        <<",\"native_global_effect_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalEffectCalls.initialActive:0)
        <<",\"native_global_effect_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalEffectCalls.activeCalls():0)
        <<",\"native_global_effect_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.globalEffectCalls.partialEntries:0)
        <<",\"native_part_modulation_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partModulationCalls.started:0)
        <<",\"native_part_modulation_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partModulationCalls.completed:0)
        <<",\"native_part_modulation_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partModulationCalls.initialActive:0)
        <<",\"native_part_modulation_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partModulationCalls.activeCalls():0)
        <<",\"native_part_modulation_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partModulationCalls.partialEntries:0)
        <<",\"native_part_route_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partRouteCalls.started:0)
        <<",\"native_part_route_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partRouteCalls.completed:0)
        <<",\"native_part_route_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partRouteCalls.initialActive:0)
        <<",\"native_part_route_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partRouteCalls.activeCalls():0)
        <<",\"native_part_route_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partRouteCalls.partialEntries:0)
        <<",\"native_voice_feedback_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.voiceFeedbackCalls.started:0)
        <<",\"native_voice_feedback_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.voiceFeedbackCalls.completed:0)
        <<",\"native_voice_feedback_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.voiceFeedbackCalls.initialActive:0)
        <<",\"native_voice_feedback_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.voiceFeedbackCalls.activeCalls():0)
        <<",\"native_voice_feedback_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.voiceFeedbackCalls.partialEntries:0)
        <<",\"native_state_parameter_edit_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateParameterEditCalls.started:0)
        <<",\"native_state_parameter_edit_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateParameterEditCalls.completed:0)
        <<",\"native_state_parameter_edit_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateParameterEditCalls.initialActive:0)
        <<",\"native_state_parameter_edit_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateParameterEditCalls.activeCalls():0)
        <<",\"native_state_parameter_edit_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateParameterEditCalls.partialEntries:0)
        <<",\"native_state_patch_name_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePatchNameCalls.started:0)
        <<",\"native_state_patch_name_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePatchNameCalls.completed:0)
        <<",\"native_state_patch_name_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePatchNameCalls.initialActive:0)
        <<",\"native_state_patch_name_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePatchNameCalls.activeCalls():0)
        <<",\"native_state_patch_name_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePatchNameCalls.partialEntries:0)
        <<",\"native_state_storage_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateStorageCalls.started:0)
        <<",\"native_state_storage_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateStorageCalls.completed:0)
        <<",\"native_state_storage_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateStorageCalls.initialActive:0)
        <<",\"native_state_storage_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateStorageCalls.activeCalls():0)
        <<",\"native_state_storage_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateStorageCalls.partialEntries:0)
        <<",\"native_part_enable_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partEnableCalls.started:0)
        <<",\"native_part_enable_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partEnableCalls.completed:0)
        <<",\"native_part_enable_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partEnableCalls.initialActive:0)
        <<",\"native_part_enable_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partEnableCalls.activeCalls():0)
        <<",\"native_part_enable_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partEnableCalls.partialEntries:0)
        <<",\"native_part_signal_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partSignalCalls.started:0)
        <<",\"native_part_signal_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partSignalCalls.completed:0)
        <<",\"native_part_signal_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partSignalCalls.initialActive:0)
        <<",\"native_part_signal_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partSignalCalls.activeCalls():0)
        <<",\"native_part_signal_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partSignalCalls.partialEntries:0)
        <<",\"native_hardware_setup_calls\":"<<(board->liveMidiController()?board->liveMidiController()->hardwareSetupCalls.started:0)
        <<",\"native_hardware_setup_returns\":"<<(board->liveMidiController()?board->liveMidiController()->hardwareSetupCalls.completed:0)
        <<",\"native_hardware_setup_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->hardwareSetupCalls.initialActive:0)
        <<",\"native_hardware_setup_active\":"<<(board->liveMidiController()?board->liveMidiController()->hardwareSetupCalls.activeCalls():0)
        <<",\"native_hardware_setup_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->hardwareSetupCalls.partialEntries:0)
        <<",\"native_voice_table_init_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceTableInitCalls.started:0)
        <<",\"native_voice_table_init_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceTableInitCalls.completed:0)
        <<",\"native_voice_table_init_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceTableInitCalls.initialActive:0)
        <<",\"native_voice_table_init_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceTableInitCalls.activeCalls():0)
        <<",\"native_voice_table_init_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceTableInitCalls.partialEntries:0)
        <<",\"native_state_mode_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateModeCalls.started:0)
        <<",\"native_state_mode_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateModeCalls.completed:0)
        <<",\"native_state_mode_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateModeCalls.initialActive:0)
        <<",\"native_state_mode_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateModeCalls.activeCalls():0)
        <<",\"native_state_mode_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateModeCalls.partialEntries:0)
        <<",\"native_state_parameter_expand_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateParameterExpandCalls.started:0)
        <<",\"native_state_parameter_expand_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateParameterExpandCalls.completed:0)
        <<",\"native_state_parameter_expand_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateParameterExpandCalls.initialActive:0)
        <<",\"native_state_parameter_expand_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateParameterExpandCalls.activeCalls():0)
        <<",\"native_state_parameter_expand_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateParameterExpandCalls.partialEntries:0)
        <<",\"native_state_pack_fields_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePackFieldsCalls.started:0)
        <<",\"native_state_pack_fields_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePackFieldsCalls.completed:0)
        <<",\"native_state_pack_fields_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePackFieldsCalls.initialActive:0)
        <<",\"native_state_pack_fields_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePackFieldsCalls.activeCalls():0)
        <<",\"native_state_pack_fields_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePackFieldsCalls.partialEntries:0)
        <<",\"native_state_pack_refresh_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePackRefreshCalls.started:0)
        <<",\"native_state_pack_refresh_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePackRefreshCalls.completed:0)
        <<",\"native_state_pack_refresh_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePackRefreshCalls.initialActive:0)
        <<",\"native_state_pack_refresh_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePackRefreshCalls.activeCalls():0)
        <<",\"native_state_pack_refresh_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePackRefreshCalls.partialEntries:0)
        <<",\"native_state_globals_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateGlobalsCalls.started:0)
        <<",\"native_state_globals_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateGlobalsCalls.completed:0)
        <<",\"native_state_globals_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateGlobalsCalls.initialActive:0)
        <<",\"native_state_globals_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateGlobalsCalls.activeCalls():0)
        <<",\"native_state_globals_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateGlobalsCalls.partialEntries:0)
        <<",\"native_periodic_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.periodicCalls.started:0)
        <<",\"native_periodic_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.periodicCalls.completed:0)
        <<",\"native_periodic_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.periodicCalls.initialActive:0)
        <<",\"native_periodic_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.periodicCalls.activeCalls():0)
        <<",\"native_periodic_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.periodicCalls.partialEntries:0)
        <<",\"native_sync_value_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.syncValueCalls.started:0)
        <<",\"native_sync_value_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.syncValueCalls.completed:0)
        <<",\"native_sync_value_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.syncValueCalls.initialActive:0)
        <<",\"native_sync_value_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.syncValueCalls.activeCalls():0)
        <<",\"native_sync_value_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.syncValueCalls.partialEntries:0)
        <<",\"native_part_refresh_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partRefreshCalls.started:0)
        <<",\"native_part_refresh_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partRefreshCalls.completed:0)
        <<",\"native_part_refresh_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partRefreshCalls.initialActive:0)
        <<",\"native_part_refresh_active\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partRefreshCalls.activeCalls():0)
        <<",\"native_part_refresh_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partRefreshCalls.partialEntries:0)
        <<",\"native_panel_byte_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelByteCalls.started:0)
        <<",\"native_panel_byte_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelByteCalls.completed:0)
        <<",\"native_panel_byte_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelByteCalls.initialActive:0)
        <<",\"native_panel_byte_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelByteCalls.activeCalls():0)
        <<",\"native_panel_byte_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelByteCalls.partialEntries:0)
        <<",\"native_panel_poll_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelPollCalls.started:0)
        <<",\"native_panel_poll_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelPollCalls.completed:0)
        <<",\"native_panel_poll_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelPollCalls.initialActive:0)
        <<",\"native_panel_poll_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelPollCalls.activeCalls():0)
        <<",\"native_panel_poll_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelPollCalls.partialEntries:0)
        <<",\"native_panel_action_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelActionCalls.started:0)
        <<",\"native_panel_action_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelActionCalls.completed:0)
        <<",\"native_panel_action_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelActionCalls.initialActive:0)
        <<",\"native_panel_action_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelActionCalls.activeCalls():0)
        <<",\"native_panel_action_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelActionCalls.partialEntries:0)
        <<",\"native_arp_primitive_calls\":"<<(board->liveMidiController()?board->liveMidiController()->arpPrimitiveCalls.started:0)
        <<",\"native_arp_primitive_returns\":"<<(board->liveMidiController()?board->liveMidiController()->arpPrimitiveCalls.completed:0)
        <<",\"native_arp_primitive_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->arpPrimitiveCalls.initialActive:0)
        <<",\"native_arp_primitive_active\":"<<(board->liveMidiController()?board->liveMidiController()->arpPrimitiveCalls.activeCalls():0)
        <<",\"native_arp_primitive_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->arpPrimitiveCalls.partialEntries:0)
        <<",\"native_arp_schedule_calls\":"<<(board->liveMidiController()?board->liveMidiController()->arpScheduleCalls.started:0)
        <<",\"native_arp_schedule_returns\":"<<(board->liveMidiController()?board->liveMidiController()->arpScheduleCalls.completed:0)
        <<",\"native_arp_schedule_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->arpScheduleCalls.initialActive:0)
        <<",\"native_arp_schedule_active\":"<<(board->liveMidiController()?board->liveMidiController()->arpScheduleCalls.activeCalls():0)
        <<",\"native_arp_schedule_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->arpScheduleCalls.partialEntries:0)
        <<",\"native_arp_main_calls\":"<<(board->liveMidiController()?board->liveMidiController()->arpMainCalls.started:0)
        <<",\"native_arp_main_returns\":"<<(board->liveMidiController()?board->liveMidiController()->arpMainCalls.completed:0)
        <<",\"native_arp_main_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->arpMainCalls.initialActive:0)
        <<",\"native_arp_main_active\":"<<(board->liveMidiController()?board->liveMidiController()->arpMainCalls.activeCalls():0)
        <<",\"native_arp_main_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->arpMainCalls.partialEntries:0)
        <<",\"native_arp_main_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->arpMainCalls.handedOff:0)
        <<",\"native_panel_decay_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelDecayCalls.started:0)
        <<",\"native_panel_decay_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelDecayCalls.completed:0)
        <<",\"native_panel_decay_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelDecayCalls.initialActive:0)
        <<",\"native_panel_decay_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelDecayCalls.activeCalls():0)
        <<",\"native_panel_decay_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelDecayCalls.partialEntries:0)
        <<",\"native_state_multi_serialize_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiSerializeCalls.started:0)
        <<",\"native_state_multi_serialize_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiSerializeCalls.completed:0)
        <<",\"native_state_multi_serialize_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiSerializeCalls.initialActive:0)
        <<",\"native_state_multi_serialize_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiSerializeCalls.activeCalls():0)
        <<",\"native_state_multi_serialize_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiSerializeCalls.partialEntries:0)
        <<",\"native_state_patch_load_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePatchLoadCalls.started:0)
        <<",\"native_state_patch_load_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePatchLoadCalls.completed:0)
        <<",\"native_state_patch_load_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePatchLoadCalls.initialActive:0)
        <<",\"native_state_patch_load_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePatchLoadCalls.activeCalls():0)
        <<",\"native_state_patch_load_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePatchLoadCalls.partialEntries:0)
        <<",\"native_state_program_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramCalls.started:0)
        <<",\"native_state_program_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramCalls.completed:0)
        <<",\"native_state_program_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramCalls.initialActive:0)
        <<",\"native_state_program_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramCalls.activeCalls():0)
        <<",\"native_state_program_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramCalls.partialEntries:0)
        <<",\"native_state_multi_load_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiLoadCalls.started:0)
        <<",\"native_state_multi_load_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiLoadCalls.completed:0)
        <<",\"native_state_multi_load_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiLoadCalls.initialActive:0)
        <<",\"native_state_multi_load_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiLoadCalls.activeCalls():0)
        <<",\"native_state_multi_load_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiLoadCalls.partialEntries:0)
        <<",\"native_state_preset_address_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetAddressCalls.started:0)
        <<",\"native_state_preset_address_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetAddressCalls.completed:0)
        <<",\"native_state_preset_address_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetAddressCalls.initialActive:0)
        <<",\"native_state_preset_address_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetAddressCalls.activeCalls():0)
        <<",\"native_state_preset_address_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetAddressCalls.partialEntries:0)
        <<",\"native_state_single_load_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateSingleLoadCalls.started:0)
        <<",\"native_state_single_load_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateSingleLoadCalls.completed:0)
        <<",\"native_state_single_load_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateSingleLoadCalls.initialActive:0)
        <<",\"native_state_single_load_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateSingleLoadCalls.activeCalls():0)
        <<",\"native_state_single_load_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateSingleLoadCalls.partialEntries:0)
        <<",\"native_serial_queue_calls\":"<<(board->liveMidiController()?board->liveMidiController()->serialQueueCalls.started:0)
        <<",\"native_serial_queue_returns\":"<<(board->liveMidiController()?board->liveMidiController()->serialQueueCalls.completed:0)
        <<",\"native_serial_queue_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialQueueCalls.initialActive:0)
        <<",\"native_serial_queue_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialQueueCalls.activeCalls():0)
        <<",\"native_serial_queue_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->serialQueueCalls.partialEntries:0)
        <<",\"native_serial_receive_calls\":"<<(board->liveMidiController()?board->liveMidiController()->serialReceiveCalls.started:0)
        <<",\"native_serial_receive_returns\":"<<(board->liveMidiController()?board->liveMidiController()->serialReceiveCalls.completed:0)
        <<",\"native_serial_receive_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialReceiveCalls.initialActive:0)
        <<",\"native_serial_receive_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialReceiveCalls.activeCalls():0)
        <<",\"native_serial_receive_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->serialReceiveCalls.partialEntries:0)
        <<",\"native_serial_setup_calls\":"<<(board->liveMidiController()?board->liveMidiController()->serialSetupCalls.started:0)
        <<",\"native_serial_setup_returns\":"<<(board->liveMidiController()?board->liveMidiController()->serialSetupCalls.completed:0)
        <<",\"native_serial_setup_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialSetupCalls.initialActive:0)
        <<",\"native_serial_setup_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialSetupCalls.activeCalls():0)
        <<",\"native_serial_setup_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->serialSetupCalls.partialEntries:0)
        <<",\"native_serial_transmit_calls\":"<<(board->liveMidiController()?board->liveMidiController()->serialTransmitCalls.started:0)
        <<",\"native_serial_transmit_returns\":"<<(board->liveMidiController()?board->liveMidiController()->serialTransmitCalls.completed:0)
        <<",\"native_serial_transmit_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialTransmitCalls.initialActive:0)
        <<",\"native_serial_transmit_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialTransmitCalls.activeCalls():0)
        <<",\"native_serial_transmit_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->serialTransmitCalls.partialEntries:0)
        <<",\"native_serial_irq_calls\":"<<(board->liveMidiController()?board->liveMidiController()->serialIrqCalls.started:0)
        <<",\"native_serial_irq_returns\":"<<(board->liveMidiController()?board->liveMidiController()->serialIrqCalls.completed:0)
        <<",\"native_serial_irq_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialIrqCalls.initialActive:0)
        <<",\"native_serial_irq_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialIrqCalls.activeCalls():0)
        <<",\"native_serial_irq_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->serialIrqCalls.partialEntries:0)
        <<",\"native_serial_direct_calls\":"<<(board->liveMidiController()?board->liveMidiController()->serialDirectCalls.started:0)
        <<",\"native_serial_direct_returns\":"<<(board->liveMidiController()?board->liveMidiController()->serialDirectCalls.completed:0)
        <<",\"native_serial_direct_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialDirectCalls.initialActive:0)
        <<",\"native_serial_direct_active\":"<<(board->liveMidiController()?board->liveMidiController()->serialDirectCalls.activeCalls():0)
        <<",\"native_serial_direct_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->serialDirectCalls.partialEntries:0)
        <<",\"native_arp_event_calls\":"<<(board->liveMidiController()?board->liveMidiController()->arpEventCalls.started:0)
        <<",\"native_arp_event_returns\":"<<(board->liveMidiController()?board->liveMidiController()->arpEventCalls.completed:0)
        <<",\"native_arp_event_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->arpEventCalls.initialActive:0)
        <<",\"native_arp_event_active\":"<<(board->liveMidiController()?board->liveMidiController()->arpEventCalls.activeCalls():0)
        <<",\"native_arp_event_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->arpEventCalls.partialEntries:0)
        <<",\"native_arp_select_calls\":"<<(board->liveMidiController()?board->liveMidiController()->arpSelectCalls.started:0)
        <<",\"native_arp_select_returns\":"<<(board->liveMidiController()?board->liveMidiController()->arpSelectCalls.completed:0)
        <<",\"native_arp_select_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->arpSelectCalls.initialActive:0)
        <<",\"native_arp_select_active\":"<<(board->liveMidiController()?board->liveMidiController()->arpSelectCalls.activeCalls():0)
        <<",\"native_arp_select_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->arpSelectCalls.partialEntries:0)
        <<",\"native_arp_select_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->arpSelectCalls.handedOff:0)
        <<",\"native_reset_image_calls\":"<<(board->liveMidiController()?board->liveMidiController()->resetImageCalls.started:0)
        <<",\"native_reset_image_returns\":"<<(board->liveMidiController()?board->liveMidiController()->resetImageCalls.completed:0)
        <<",\"native_reset_image_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->resetImageCalls.initialActive:0)
        <<",\"native_reset_image_active\":"<<(board->liveMidiController()?board->liveMidiController()->resetImageCalls.activeCalls():0)
        <<",\"native_reset_image_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->resetImageCalls.partialEntries:0)
        <<",\"native_state_manual_edit_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateManualEditCalls.started:0)
        <<",\"native_state_manual_edit_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateManualEditCalls.completed:0)
        <<",\"native_state_manual_edit_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateManualEditCalls.initialActive:0)
        <<",\"native_state_manual_edit_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateManualEditCalls.activeCalls():0)
        <<",\"native_state_manual_edit_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateManualEditCalls.partialEntries:0)
        <<",\"native_state_post_edit_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePostEditCalls.started:0)
        <<",\"native_state_post_edit_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePostEditCalls.completed:0)
        <<",\"native_state_post_edit_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePostEditCalls.initialActive:0)
        <<",\"native_state_post_edit_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePostEditCalls.activeCalls():0)
        <<",\"native_state_post_edit_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePostEditCalls.partialEntries:0)
        <<",\"native_panel_control_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelControlCalls.started:0)
        <<",\"native_panel_control_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelControlCalls.completed:0)
        <<",\"native_panel_control_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelControlCalls.initialActive:0)
        <<",\"native_panel_control_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelControlCalls.activeCalls():0)
        <<",\"native_panel_control_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelControlCalls.partialEntries:0)
        <<",\"native_panel_toggle_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelToggleCalls.started:0)
        <<",\"native_panel_toggle_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelToggleCalls.completed:0)
        <<",\"native_panel_toggle_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelToggleCalls.initialActive:0)
        <<",\"native_panel_toggle_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelToggleCalls.activeCalls():0)
        <<",\"native_panel_toggle_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelToggleCalls.partialEntries:0)
        <<",\"native_panel_decimal_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelDecimalCalls.started:0)
        <<",\"native_panel_decimal_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelDecimalCalls.completed:0)
        <<",\"native_panel_decimal_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelDecimalCalls.initialActive:0)
        <<",\"native_panel_decimal_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelDecimalCalls.activeCalls():0)
        <<",\"native_panel_decimal_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelDecimalCalls.partialEntries:0)
        <<",\"native_panel_text_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelTextCalls.started:0)
        <<",\"native_panel_text_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelTextCalls.completed:0)
        <<",\"native_panel_text_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelTextCalls.initialActive:0)
        <<",\"native_panel_text_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelTextCalls.activeCalls():0)
        <<",\"native_panel_text_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelTextCalls.partialEntries:0)
        <<",\"native_internal_midi_calls\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiCalls.started:0)
        <<",\"native_internal_midi_returns\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiCalls.completed:0)
        <<",\"native_internal_midi_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiCalls.initialActive:0)
        <<",\"native_internal_midi_active\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiCalls.activeCalls():0)
        <<",\"native_internal_midi_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiCalls.partialEntries:0)
        <<",\"native_internal_midi_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiCalls.handedOff:0)
        <<",\"native_internal_note_calls\":"<<(board->liveMidiController()?board->liveMidiController()->internalNoteCalls.started:0)
        <<",\"native_internal_note_returns\":"<<(board->liveMidiController()?board->liveMidiController()->internalNoteCalls.completed:0)
        <<",\"native_internal_note_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->internalNoteCalls.initialActive:0)
        <<",\"native_internal_note_active\":"<<(board->liveMidiController()?board->liveMidiController()->internalNoteCalls.activeCalls():0)
        <<",\"native_internal_note_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->internalNoteCalls.partialEntries:0)
        <<",\"native_internal_note_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->internalNoteCalls.handedOff:0)
        <<",\"native_sys_ex_frame_calls\":"<<(board->liveMidiController()?board->liveMidiController()->sysExFrameCalls.started:0)
        <<",\"native_sys_ex_frame_returns\":"<<(board->liveMidiController()?board->liveMidiController()->sysExFrameCalls.completed:0)
        <<",\"native_sys_ex_frame_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->sysExFrameCalls.initialActive:0)
        <<",\"native_sys_ex_frame_active\":"<<(board->liveMidiController()?board->liveMidiController()->sysExFrameCalls.activeCalls():0)
        <<",\"native_sys_ex_frame_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->sysExFrameCalls.partialEntries:0)
        <<",\"native_sys_ex_frame_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->sysExFrameCalls.handedOff:0)
        <<",\"native_exception_return_calls\":"<<(board->liveMidiController()?board->liveMidiController()->exceptionReturnCalls.started:0)
        <<",\"native_exception_return_returns\":"<<(board->liveMidiController()?board->liveMidiController()->exceptionReturnCalls.completed:0)
        <<",\"native_exception_return_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->exceptionReturnCalls.initialActive:0)
        <<",\"native_exception_return_active\":"<<(board->liveMidiController()?board->liveMidiController()->exceptionReturnCalls.activeCalls():0)
        <<",\"native_exception_return_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->exceptionReturnCalls.partialEntries:0)
        <<",\"native_internal_midi_descriptor_calls\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiCalls.descriptorStarted:0)
        <<",\"native_internal_midi_descriptor_returns\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiCalls.descriptorCompleted:0)
        <<",\"native_internal_midi_initial_descriptors\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiCalls.initialDescriptors:0)
        <<",\"native_internal_midi_active_descriptors\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiCalls.activeDescriptors():0)
        <<",\"native_state_edit_address_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditAddressCalls.started:0)
        <<",\"native_state_edit_address_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditAddressCalls.completed:0)
        <<",\"native_state_edit_address_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditAddressCalls.initialActive:0)
        <<",\"native_state_edit_address_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditAddressCalls.activeCalls():0)
        <<",\"native_state_edit_address_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditAddressCalls.partialEntries:0)
        <<",\"native_state_store_ui_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoreUiCalls.started:0)
        <<",\"native_state_store_ui_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoreUiCalls.completed:0)
        <<",\"native_state_store_ui_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoreUiCalls.initialActive:0)
        <<",\"native_state_store_ui_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoreUiCalls.activeCalls():0)
        <<",\"native_state_store_ui_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoreUiCalls.partialEntries:0)
        <<",\"native_panel_matrix_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelMatrixCalls.started:0)
        <<",\"native_panel_matrix_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelMatrixCalls.completed:0)
        <<",\"native_panel_matrix_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelMatrixCalls.initialActive:0)
        <<",\"native_panel_matrix_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelMatrixCalls.activeCalls():0)
        <<",\"native_panel_matrix_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelMatrixCalls.partialEntries:0)
        <<",\"native_pedal_sense_calls\":"<<(board->liveMidiController()?board->liveMidiController()->pedalSenseCalls.started:0)
        <<",\"native_pedal_sense_returns\":"<<(board->liveMidiController()?board->liveMidiController()->pedalSenseCalls.completed:0)
        <<",\"native_pedal_sense_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->pedalSenseCalls.initialActive:0)
        <<",\"native_pedal_sense_active\":"<<(board->liveMidiController()?board->liveMidiController()->pedalSenseCalls.activeCalls():0)
        <<",\"native_pedal_sense_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->pedalSenseCalls.partialEntries:0)
        <<",\"native_panel_encoder_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelEncoderCalls.started:0)
        <<",\"native_panel_encoder_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelEncoderCalls.completed:0)
        <<",\"native_panel_encoder_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelEncoderCalls.initialActive:0)
        <<",\"native_panel_encoder_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelEncoderCalls.activeCalls():0)
        <<",\"native_panel_encoder_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelEncoderCalls.partialEntries:0)
        <<",\"native_panel_adc_samples_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcSamplesCalls.started:0)
        <<",\"native_panel_adc_samples_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcSamplesCalls.completed:0)
        <<",\"native_panel_adc_samples_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcSamplesCalls.initialActive:0)
        <<",\"native_panel_adc_samples_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcSamplesCalls.activeCalls():0)
        <<",\"native_panel_adc_samples_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcSamplesCalls.partialEntries:0)
        <<",\"native_panel_adc_scale_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcScaleCalls.started:0)
        <<",\"native_panel_adc_scale_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcScaleCalls.completed:0)
        <<",\"native_panel_adc_scale_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcScaleCalls.initialActive:0)
        <<",\"native_panel_adc_scale_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcScaleCalls.activeCalls():0)
        <<",\"native_panel_adc_scale_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcScaleCalls.partialEntries:0)
        <<",\"native_panel_adc_calibration_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcCalibrationCalls.started:0)
        <<",\"native_panel_adc_calibration_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcCalibrationCalls.completed:0)
        <<",\"native_panel_adc_calibration_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcCalibrationCalls.initialActive:0)
        <<",\"native_panel_adc_calibration_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcCalibrationCalls.activeCalls():0)
        <<",\"native_panel_adc_calibration_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcCalibrationCalls.partialEntries:0)
        <<",\"native_panel_adc_irq_quick_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.quickReturns:0)
        <<",\"native_panel_adc_irq_settle_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.settleReturns:0)
        <<",\"native_panel_adc_irq_scan_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.scanReturns:0)
        <<",\"native_panel_adc_irq_accepted_channels\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.acceptedChannels:0)
        <<",\"native_panel_adc_irq_preview_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.previewHandoffs:0)
        <<",\"native_panel_adc_irq_special_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.specialHandoffs:0)
        <<",\"native_panel_adc_irq_conversion_restarts\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.conversionRestarts:0)
        <<",\"native_panel_adc_irq_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.started:0)
        <<",\"native_panel_adc_irq_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.completed:0)
        <<",\"native_panel_adc_irq_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.initialActive:0)
        <<",\"native_panel_adc_irq_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.activeCalls():0)
        <<",\"native_panel_adc_irq_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcIrqCalls.partialEntries:0)
        <<",\"native_panel_adc_scan_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcScanCalls.started:0)
        <<",\"native_panel_adc_scan_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcScanCalls.completed:0)
        <<",\"native_panel_adc_scan_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcScanCalls.initialActive:0)
        <<",\"native_panel_adc_scan_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcScanCalls.activeCalls():0)
        <<",\"native_panel_adc_scan_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelAdcScanCalls.partialEntries:0)
        <<",\"native_internal_midi_queue_calls\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiQueueCalls.started:0)
        <<",\"native_internal_midi_queue_returns\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiQueueCalls.completed:0)
        <<",\"native_internal_midi_queue_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiQueueCalls.initialActive:0)
        <<",\"native_internal_midi_queue_active\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiQueueCalls.activeCalls():0)
        <<",\"native_internal_midi_queue_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiQueueCalls.partialEntries:0)
        <<",\"native_midi_output_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiOutputCalls.started:0)
        <<",\"native_midi_output_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiOutputCalls.completed:0)
        <<",\"native_midi_output_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiOutputCalls.initialActive:0)
        <<",\"native_midi_output_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiOutputCalls.activeCalls():0)
        <<",\"native_midi_output_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->midiOutputCalls.partialEntries:0)
        <<",\"native_internal_midi_service_calls\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiServiceCalls.started:0)
        <<",\"native_internal_midi_service_returns\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiServiceCalls.completed:0)
        <<",\"native_internal_midi_service_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiServiceCalls.initialActive:0)
        <<",\"native_internal_midi_service_active\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiServiceCalls.activeCalls():0)
        <<",\"native_internal_midi_service_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->internalMidiServiceCalls.partialEntries:0)
        <<",\"native_state_foreground_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateForegroundCalls.started:0)
        <<",\"native_state_foreground_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateForegroundCalls.completed:0)
        <<",\"native_state_foreground_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateForegroundCalls.initialActive:0)
        <<",\"native_state_foreground_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateForegroundCalls.activeCalls():0)
        <<",\"native_state_foreground_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateForegroundCalls.partialEntries:0)
        <<",\"native_state_button_dispatch_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateButtonDispatchCalls.started:0)
        <<",\"native_state_button_dispatch_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateButtonDispatchCalls.completed:0)
        <<",\"native_state_button_dispatch_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateButtonDispatchCalls.initialActive:0)
        <<",\"native_state_button_dispatch_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateButtonDispatchCalls.activeCalls():0)
        <<",\"native_state_button_dispatch_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateButtonDispatchCalls.partialEntries:0)
        <<",\"native_panel_analog_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelAnalogCalls.started:0)
        <<",\"native_panel_analog_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelAnalogCalls.completed:0)
        <<",\"native_panel_analog_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAnalogCalls.initialActive:0)
        <<",\"native_panel_analog_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelAnalogCalls.activeCalls():0)
        <<",\"native_panel_analog_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelAnalogCalls.partialEntries:0)
        <<",\"native_clock_sync_calls\":"<<(board->liveMidiController()?board->liveMidiController()->clockSyncCalls.started:0)
        <<",\"native_clock_sync_returns\":"<<(board->liveMidiController()?board->liveMidiController()->clockSyncCalls.completed:0)
        <<",\"native_clock_sync_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->clockSyncCalls.initialActive:0)
        <<",\"native_clock_sync_active\":"<<(board->liveMidiController()?board->liveMidiController()->clockSyncCalls.activeCalls():0)
        <<",\"native_clock_sync_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->clockSyncCalls.partialEntries:0)
        <<",\"native_clock_pulse_calls\":"<<(board->liveMidiController()?board->liveMidiController()->clockPulseCalls.started:0)
        <<",\"native_clock_pulse_returns\":"<<(board->liveMidiController()?board->liveMidiController()->clockPulseCalls.completed:0)
        <<",\"native_clock_pulse_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->clockPulseCalls.initialActive:0)
        <<",\"native_clock_pulse_active\":"<<(board->liveMidiController()?board->liveMidiController()->clockPulseCalls.activeCalls():0)
        <<",\"native_clock_pulse_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->clockPulseCalls.partialEntries:0)
        <<",\"native_clock_display_calls\":"<<(board->liveMidiController()?board->liveMidiController()->clockDisplayCalls.started:0)
        <<",\"native_clock_display_returns\":"<<(board->liveMidiController()?board->liveMidiController()->clockDisplayCalls.completed:0)
        <<",\"native_clock_display_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->clockDisplayCalls.initialActive:0)
        <<",\"native_clock_display_active\":"<<(board->liveMidiController()?board->liveMidiController()->clockDisplayCalls.activeCalls():0)
        <<",\"native_clock_display_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->clockDisplayCalls.partialEntries:0)
        <<",\"native_state_screen_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenCalls.started:0)
        <<",\"native_state_screen_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenCalls.completed:0)
        <<",\"native_state_screen_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenCalls.initialActive:0)
        <<",\"native_state_screen_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenCalls.activeCalls():0)
        <<",\"native_state_screen_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenCalls.partialEntries:0)
        <<",\"native_panel_settings_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelSettingsCalls.started:0)
        <<",\"native_panel_settings_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelSettingsCalls.completed:0)
        <<",\"native_panel_settings_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelSettingsCalls.initialActive:0)
        <<",\"native_panel_settings_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelSettingsCalls.activeCalls():0)
        <<",\"native_panel_settings_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelSettingsCalls.partialEntries:0)
        <<",\"native_state_edit_context_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditContextCalls.started:0)
        <<",\"native_state_edit_context_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditContextCalls.completed:0)
        <<",\"native_state_edit_context_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditContextCalls.initialActive:0)
        <<",\"native_state_edit_context_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditContextCalls.activeCalls():0)
        <<",\"native_state_edit_context_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditContextCalls.partialEntries:0)
        <<",\"native_state_screen_edit_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenEditCalls.started:0)
        <<",\"native_state_screen_edit_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenEditCalls.completed:0)
        <<",\"native_state_screen_edit_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenEditCalls.initialActive:0)
        <<",\"native_state_screen_edit_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenEditCalls.activeCalls():0)
        <<",\"native_state_screen_edit_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenEditCalls.partialEntries:0)
        <<",\"native_state_menu_format_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateMenuFormatCalls.started:0)
        <<",\"native_state_menu_format_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateMenuFormatCalls.completed:0)
        <<",\"native_state_menu_format_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateMenuFormatCalls.initialActive:0)
        <<",\"native_state_menu_format_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateMenuFormatCalls.activeCalls():0)
        <<",\"native_state_menu_format_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateMenuFormatCalls.partialEntries:0)
        <<",\"native_system_services_calls\":"<<(board->liveMidiController()?board->liveMidiController()->systemServicesCalls.started:0)
        <<",\"native_system_services_returns\":"<<(board->liveMidiController()?board->liveMidiController()->systemServicesCalls.completed:0)
        <<",\"native_system_services_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->systemServicesCalls.initialActive:0)
        <<",\"native_system_services_active\":"<<(board->liveMidiController()?board->liveMidiController()->systemServicesCalls.activeCalls():0)
        <<",\"native_system_services_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->systemServicesCalls.partialEntries:0)
        <<",\"native_keyboard_events_calls\":"<<(board->liveMidiController()?board->liveMidiController()->keyboardEventsCalls.started:0)
        <<",\"native_keyboard_events_returns\":"<<(board->liveMidiController()?board->liveMidiController()->keyboardEventsCalls.completed:0)
        <<",\"native_keyboard_events_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->keyboardEventsCalls.initialActive:0)
        <<",\"native_keyboard_events_active\":"<<(board->liveMidiController()?board->liveMidiController()->keyboardEventsCalls.activeCalls():0)
        <<",\"native_keyboard_events_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->keyboardEventsCalls.partialEntries:0)
        <<",\"native_state_part_selection_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePartSelectionCalls.started:0)
        <<",\"native_state_part_selection_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePartSelectionCalls.completed:0)
        <<",\"native_state_part_selection_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePartSelectionCalls.initialActive:0)
        <<",\"native_state_part_selection_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePartSelectionCalls.activeCalls():0)
        <<",\"native_state_part_selection_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePartSelectionCalls.partialEntries:0)
        <<",\"native_state_menu_actions_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateMenuActionsCalls.started:0)
        <<",\"native_state_menu_actions_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateMenuActionsCalls.completed:0)
        <<",\"native_state_menu_actions_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateMenuActionsCalls.initialActive:0)
        <<",\"native_state_menu_actions_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateMenuActionsCalls.activeCalls():0)
        <<",\"native_state_menu_actions_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateMenuActionsCalls.partialEntries:0)
        <<",\"keyboard_io_reads\":"<<(ioFixture?ioFixture->keyboardReads:0)
        <<",\"keyboard_io_transitions\":"<<(ioFixture?ioFixture->keyboardTransitions:0)
        <<",\"native_keyboard_scan_calls\":"<<(board->liveMidiController()?board->liveMidiController()->keyboardScanCalls.started:0)
        <<",\"native_keyboard_scan_returns\":"<<(board->liveMidiController()?board->liveMidiController()->keyboardScanCalls.completed:0)
        <<",\"native_keyboard_scan_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->keyboardScanCalls.initialActive:0)
        <<",\"native_keyboard_scan_active\":"<<(board->liveMidiController()?board->liveMidiController()->keyboardScanCalls.activeCalls():0)
        <<",\"native_keyboard_scan_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->keyboardScanCalls.partialEntries:0)
        <<",\"native_diagnostic_codec_calls\":"<<(board->liveMidiController()?board->liveMidiController()->diagnosticCodecCalls.started:0)
        <<",\"native_diagnostic_codec_returns\":"<<(board->liveMidiController()?board->liveMidiController()->diagnosticCodecCalls.completed:0)
        <<",\"native_diagnostic_codec_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->diagnosticCodecCalls.initialActive:0)
        <<",\"native_diagnostic_codec_active\":"<<(board->liveMidiController()?board->liveMidiController()->diagnosticCodecCalls.activeCalls():0)
        <<",\"native_diagnostic_codec_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->diagnosticCodecCalls.partialEntries:0)
        <<",\"native_diagnostic_transfer_calls\":"<<(board->liveMidiController()?board->liveMidiController()->diagnosticTransferCalls.started:0)
        <<",\"native_diagnostic_transfer_returns\":"<<(board->liveMidiController()?board->liveMidiController()->diagnosticTransferCalls.completed:0)
        <<",\"native_diagnostic_transfer_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->diagnosticTransferCalls.initialActive:0)
        <<",\"native_diagnostic_transfer_active\":"<<(board->liveMidiController()?board->liveMidiController()->diagnosticTransferCalls.activeCalls():0)
        <<",\"native_diagnostic_transfer_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->diagnosticTransferCalls.partialEntries:0)
        <<",\"native_memory_diagnostics_calls\":"<<(board->liveMidiController()?board->liveMidiController()->memoryDiagnosticsCalls.started:0)
        <<",\"native_memory_diagnostics_returns\":"<<(board->liveMidiController()?board->liveMidiController()->memoryDiagnosticsCalls.completed:0)
        <<",\"native_memory_diagnostics_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->memoryDiagnosticsCalls.initialActive:0)
        <<",\"native_memory_diagnostics_active\":"<<(board->liveMidiController()?board->liveMidiController()->memoryDiagnosticsCalls.activeCalls():0)
        <<",\"native_memory_diagnostics_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->memoryDiagnosticsCalls.partialEntries:0)
        <<",\"native_startup_banner_calls\":"<<(board->liveMidiController()?board->liveMidiController()->startupBannerCalls.started:0)
        <<",\"native_startup_banner_returns\":"<<(board->liveMidiController()?board->liveMidiController()->startupBannerCalls.completed:0)
        <<",\"native_startup_banner_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->startupBannerCalls.initialActive:0)
        <<",\"native_startup_banner_active\":"<<(board->liveMidiController()?board->liveMidiController()->startupBannerCalls.activeCalls():0)
        <<",\"native_startup_banner_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->startupBannerCalls.partialEntries:0)
        <<",\"native_state_screen_actions_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenActionsCalls.started:0)
        <<",\"native_state_screen_actions_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenActionsCalls.completed:0)
        <<",\"native_state_screen_actions_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenActionsCalls.initialActive:0)
        <<",\"native_state_screen_actions_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenActionsCalls.activeCalls():0)
        <<",\"native_state_screen_actions_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateScreenActionsCalls.partialEntries:0)
        <<",\"native_state_panel_modes_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePanelModesCalls.started:0)
        <<",\"native_state_panel_modes_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePanelModesCalls.completed:0)
        <<",\"native_state_panel_modes_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePanelModesCalls.initialActive:0)
        <<",\"native_state_panel_modes_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePanelModesCalls.activeCalls():0)
        <<",\"native_state_panel_modes_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePanelModesCalls.partialEntries:0)
        <<",\"native_state_function_keys_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateFunctionKeysCalls.started:0)
        <<",\"native_state_function_keys_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateFunctionKeysCalls.completed:0)
        <<",\"native_state_function_keys_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateFunctionKeysCalls.initialActive:0)
        <<",\"native_state_function_keys_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateFunctionKeysCalls.activeCalls():0)
        <<",\"native_state_function_keys_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateFunctionKeysCalls.partialEntries:0)
        <<",\"native_state_edit_workflow_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditWorkflowCalls.started:0)
        <<",\"native_state_edit_workflow_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditWorkflowCalls.completed:0)
        <<",\"native_state_edit_workflow_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditWorkflowCalls.initialActive:0)
        <<",\"native_state_edit_workflow_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditWorkflowCalls.activeCalls():0)
        <<",\"native_state_edit_workflow_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditWorkflowCalls.partialEntries:0)
        <<",\"native_loader_inputs_calls\":"<<(board->liveMidiController()?board->liveMidiController()->loaderInputsCalls.started:0)
        <<",\"native_loader_inputs_returns\":"<<(board->liveMidiController()?board->liveMidiController()->loaderInputsCalls.completed:0)
        <<",\"native_loader_inputs_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->loaderInputsCalls.initialActive:0)
        <<",\"native_loader_inputs_active\":"<<(board->liveMidiController()?board->liveMidiController()->loaderInputsCalls.activeCalls():0)
        <<",\"native_loader_inputs_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->loaderInputsCalls.partialEntries:0)
        <<",\"native_loader_codec_calls\":"<<(board->liveMidiController()?board->liveMidiController()->loaderCodecCalls.started:0)
        <<",\"native_loader_codec_returns\":"<<(board->liveMidiController()?board->liveMidiController()->loaderCodecCalls.completed:0)
        <<",\"native_loader_codec_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->loaderCodecCalls.initialActive:0)
        <<",\"native_loader_codec_active\":"<<(board->liveMidiController()?board->liveMidiController()->loaderCodecCalls.activeCalls():0)
        <<",\"native_loader_codec_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->loaderCodecCalls.partialEntries:0)
        <<",\"native_loader_packets_calls\":"<<(board->liveMidiController()?board->liveMidiController()->loaderPacketsCalls.started:0)
        <<",\"native_loader_packets_returns\":"<<(board->liveMidiController()?board->liveMidiController()->loaderPacketsCalls.completed:0)
        <<",\"native_loader_packets_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->loaderPacketsCalls.initialActive:0)
        <<",\"native_loader_packets_active\":"<<(board->liveMidiController()?board->liveMidiController()->loaderPacketsCalls.activeCalls():0)
        <<",\"native_loader_packets_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->loaderPacketsCalls.partialEntries:0)
        <<",\"native_loader_program_calls\":"<<(board->liveMidiController()?board->liveMidiController()->loaderProgramCalls.started:0)
        <<",\"native_loader_program_returns\":"<<(board->liveMidiController()?board->liveMidiController()->loaderProgramCalls.completed:0)
        <<",\"native_loader_program_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->loaderProgramCalls.initialActive:0)
        <<",\"native_loader_program_active\":"<<(board->liveMidiController()?board->liveMidiController()->loaderProgramCalls.activeCalls():0)
        <<",\"native_loader_program_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->loaderProgramCalls.partialEntries:0)
        <<",\"native_loader_session_calls\":"<<(board->liveMidiController()?board->liveMidiController()->loaderSessionCalls.started:0)
        <<",\"native_loader_session_returns\":"<<(board->liveMidiController()?board->liveMidiController()->loaderSessionCalls.completed:0)
        <<",\"native_loader_session_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->loaderSessionCalls.initialActive:0)
        <<",\"native_loader_session_active\":"<<(board->liveMidiController()?board->liveMidiController()->loaderSessionCalls.activeCalls():0)
        <<",\"native_loader_session_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->loaderSessionCalls.partialEntries:0)
        <<",\"native_state_value_bounds_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateValueBoundsCalls.started:0)
        <<",\"native_state_value_bounds_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateValueBoundsCalls.completed:0)
        <<",\"native_state_value_bounds_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateValueBoundsCalls.initialActive:0)
        <<",\"native_state_value_bounds_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateValueBoundsCalls.activeCalls():0)
        <<",\"native_state_value_bounds_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateValueBoundsCalls.partialEntries:0)
        <<",\"native_state_edit_label_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditLabelCalls.started:0)
        <<",\"native_state_edit_label_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditLabelCalls.completed:0)
        <<",\"native_state_edit_label_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditLabelCalls.initialActive:0)
        <<",\"native_state_edit_label_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditLabelCalls.activeCalls():0)
        <<",\"native_state_edit_label_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateEditLabelCalls.partialEntries:0)
        <<",\"native_state_field_actions_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateFieldActionsCalls.started:0)
        <<",\"native_state_field_actions_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateFieldActionsCalls.completed:0)
        <<",\"native_state_field_actions_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateFieldActionsCalls.initialActive:0)
        <<",\"native_state_field_actions_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateFieldActionsCalls.activeCalls():0)
        <<",\"native_state_field_actions_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateFieldActionsCalls.partialEntries:0)
        <<",\"native_state_bank_navigation_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateBankNavigationCalls.started:0)
        <<",\"native_state_bank_navigation_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateBankNavigationCalls.completed:0)
        <<",\"native_state_bank_navigation_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateBankNavigationCalls.initialActive:0)
        <<",\"native_state_bank_navigation_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateBankNavigationCalls.activeCalls():0)
        <<",\"native_state_bank_navigation_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateBankNavigationCalls.partialEntries:0)
        <<",\"native_state_status_display_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateStatusDisplayCalls.started:0)
        <<",\"native_state_status_display_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateStatusDisplayCalls.completed:0)
        <<",\"native_state_status_display_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateStatusDisplayCalls.initialActive:0)
        <<",\"native_state_status_display_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateStatusDisplayCalls.activeCalls():0)
        <<",\"native_state_status_display_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateStatusDisplayCalls.partialEntries:0)
        <<",\"native_state_stored_value_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoredValueCalls.started:0)
        <<",\"native_state_stored_value_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoredValueCalls.completed:0)
        <<",\"native_state_stored_value_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoredValueCalls.initialActive:0)
        <<",\"native_state_stored_value_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoredValueCalls.activeCalls():0)
        <<",\"native_state_stored_value_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoredValueCalls.partialEntries:0)
        <<",\"native_state_browse_workflow_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateBrowseWorkflowCalls.started:0)
        <<",\"native_state_browse_workflow_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateBrowseWorkflowCalls.completed:0)
        <<",\"native_state_browse_workflow_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateBrowseWorkflowCalls.initialActive:0)
        <<",\"native_state_browse_workflow_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateBrowseWorkflowCalls.activeCalls():0)
        <<",\"native_state_browse_workflow_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateBrowseWorkflowCalls.partialEntries:0)
        <<",\"native_state_program_browse_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramBrowseCalls.started:0)
        <<",\"native_state_program_browse_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramBrowseCalls.completed:0)
        <<",\"native_state_program_browse_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramBrowseCalls.initialActive:0)
        <<",\"native_state_program_browse_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramBrowseCalls.activeCalls():0)
        <<",\"native_state_program_browse_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramBrowseCalls.partialEntries:0)
        <<",\"native_state_program_display_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramDisplayCalls.started:0)
        <<",\"native_state_program_display_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramDisplayCalls.completed:0)
        <<",\"native_state_program_display_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramDisplayCalls.initialActive:0)
        <<",\"native_state_program_display_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramDisplayCalls.activeCalls():0)
        <<",\"native_state_program_display_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramDisplayCalls.partialEntries:0)
        <<",\"native_state_program_event_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramEventCalls.started:0)
        <<",\"native_state_program_event_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramEventCalls.completed:0)
        <<",\"native_state_program_event_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramEventCalls.initialActive:0)
        <<",\"native_state_program_event_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramEventCalls.activeCalls():0)
        <<",\"native_state_program_event_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramEventCalls.partialEntries:0)
        <<",\"native_panel_input_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelInputCalls.started:0)
        <<",\"native_panel_input_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelInputCalls.completed:0)
        <<",\"native_panel_input_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelInputCalls.initialActive:0)
        <<",\"native_panel_input_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelInputCalls.activeCalls():0)
        <<",\"native_panel_input_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelInputCalls.partialEntries:0)
        <<",\"native_state_channel_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateChannelCalls.started:0)
        <<",\"native_state_channel_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateChannelCalls.completed:0)
        <<",\"native_state_channel_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateChannelCalls.initialActive:0)
        <<",\"native_state_channel_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateChannelCalls.activeCalls():0)
        <<",\"native_state_channel_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateChannelCalls.partialEntries:0)
        <<",\"native_timer_service_calls\":"<<(board->liveMidiController()?board->liveMidiController()->timerServiceCalls.started:0)
        <<",\"native_timer_service_returns\":"<<(board->liveMidiController()?board->liveMidiController()->timerServiceCalls.completed:0)
        <<",\"native_timer_service_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->timerServiceCalls.initialActive:0)
        <<",\"native_timer_service_active\":"<<(board->liveMidiController()?board->liveMidiController()->timerServiceCalls.activeCalls():0)
        <<",\"native_timer_service_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->timerServiceCalls.partialEntries:0)
        <<",\"native_flash_page_calls\":"<<(board->liveMidiController()?board->liveMidiController()->flashPageCalls.started:0)
        <<",\"native_flash_page_returns\":"<<(board->liveMidiController()?board->liveMidiController()->flashPageCalls.completed:0)
        <<",\"native_flash_page_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->flashPageCalls.initialActive:0)
        <<",\"native_flash_page_active\":"<<(board->liveMidiController()?board->liveMidiController()->flashPageCalls.activeCalls():0)
        <<",\"native_flash_page_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->flashPageCalls.partialEntries:0)
        <<",\"native_state_flash_commit_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateFlashCommitCalls.started:0)
        <<",\"native_state_flash_commit_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateFlashCommitCalls.completed:0)
        <<",\"native_state_flash_commit_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateFlashCommitCalls.initialActive:0)
        <<",\"native_state_flash_commit_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateFlashCommitCalls.activeCalls():0)
        <<",\"native_state_flash_commit_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateFlashCommitCalls.partialEntries:0)
        <<",\"native_state_store_services_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoreServicesCalls.started:0)
        <<",\"native_state_store_services_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoreServicesCalls.completed:0)
        <<",\"native_state_store_services_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoreServicesCalls.initialActive:0)
        <<",\"native_state_store_services_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoreServicesCalls.activeCalls():0)
        <<",\"native_state_store_services_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateStoreServicesCalls.partialEntries:0)
        <<",\"native_state_preset_format_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetFormatCalls.started:0)
        <<",\"native_state_preset_format_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetFormatCalls.completed:0)
        <<",\"native_state_preset_format_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetFormatCalls.initialActive:0)
        <<",\"native_state_preset_format_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetFormatCalls.activeCalls():0)
        <<",\"native_state_preset_format_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetFormatCalls.partialEntries:0)
        <<",\"native_state_preset_save_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetSaveCalls.started:0)
        <<",\"native_state_preset_save_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetSaveCalls.completed:0)
        <<",\"native_state_preset_save_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetSaveCalls.initialActive:0)
        <<",\"native_state_preset_save_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetSaveCalls.activeCalls():0)
        <<",\"native_state_preset_save_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePresetSaveCalls.partialEntries:0)
        <<",\"native_state_global_save_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateGlobalSaveCalls.started:0)
        <<",\"native_state_global_save_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateGlobalSaveCalls.completed:0)
        <<",\"native_state_global_save_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateGlobalSaveCalls.initialActive:0)
        <<",\"native_state_global_save_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateGlobalSaveCalls.activeCalls():0)
        <<",\"native_state_global_save_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateGlobalSaveCalls.partialEntries:0)
        <<",\"native_state_factory_services_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateFactoryServicesCalls.started:0)
        <<",\"native_state_factory_services_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateFactoryServicesCalls.completed:0)
        <<",\"native_state_factory_services_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateFactoryServicesCalls.initialActive:0)
        <<",\"native_state_factory_services_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateFactoryServicesCalls.activeCalls():0)
        <<",\"native_state_factory_services_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateFactoryServicesCalls.partialEntries:0)
        <<",\"native_panel_format_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelFormatCalls.started:0)
        <<",\"native_panel_format_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelFormatCalls.completed:0)
        <<",\"native_panel_format_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelFormatCalls.initialActive:0)
        <<",\"native_panel_format_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelFormatCalls.activeCalls():0)
        <<",\"native_panel_format_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelFormatCalls.partialEntries:0)
        <<",\"native_panel_hex_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelHexCalls.started:0)
        <<",\"native_panel_hex_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelHexCalls.completed:0)
        <<",\"native_panel_hex_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelHexCalls.initialActive:0)
        <<",\"native_panel_hex_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelHexCalls.activeCalls():0)
        <<",\"native_panel_hex_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelHexCalls.partialEntries:0)
        <<",\"native_panel_percent_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelPercentCalls.started:0)
        <<",\"native_panel_percent_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelPercentCalls.completed:0)
        <<",\"native_panel_percent_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelPercentCalls.initialActive:0)
        <<",\"native_panel_percent_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelPercentCalls.activeCalls():0)
        <<",\"native_panel_percent_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelPercentCalls.partialEntries:0)
        <<",\"native_panel_version_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelVersionCalls.started:0)
        <<",\"native_panel_version_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelVersionCalls.completed:0)
        <<",\"native_panel_version_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelVersionCalls.initialActive:0)
        <<",\"native_panel_version_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelVersionCalls.activeCalls():0)
        <<",\"native_panel_version_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelVersionCalls.partialEntries:0)
        <<",\"native_patch_expand_calls\":"<<(board->liveMidiController()?board->liveMidiController()->patchExpandCalls.started:0)
        <<",\"native_patch_expand_returns\":"<<(board->liveMidiController()?board->liveMidiController()->patchExpandCalls.completed:0)
        <<",\"native_patch_expand_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->patchExpandCalls.initialActive:0)
        <<",\"native_patch_expand_active\":"<<(board->liveMidiController()?board->liveMidiController()->patchExpandCalls.activeCalls():0)
        <<",\"native_patch_expand_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->patchExpandCalls.partialEntries:0)
        <<",\"native_sys_ex_action_calls\":"<<(board->liveMidiController()?board->liveMidiController()->sysExActionCalls.started:0)
        <<",\"native_sys_ex_action_returns\":"<<(board->liveMidiController()?board->liveMidiController()->sysExActionCalls.completed:0)
        <<",\"native_sys_ex_action_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->sysExActionCalls.initialActive:0)
        <<",\"native_sys_ex_action_active\":"<<(board->liveMidiController()?board->liveMidiController()->sysExActionCalls.activeCalls():0)
        <<",\"native_sys_ex_action_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->sysExActionCalls.partialEntries:0)
        <<",\"native_sys_ex_action_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->sysExActionCalls.handedOff:0)
        <<",\"native_legacy_sys_ex_calls\":"<<(board->liveMidiController()?board->liveMidiController()->legacySysExCalls.started:0)
        <<",\"native_legacy_sys_ex_returns\":"<<(board->liveMidiController()?board->liveMidiController()->legacySysExCalls.completed:0)
        <<",\"native_legacy_sys_ex_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->legacySysExCalls.initialActive:0)
        <<",\"native_legacy_sys_ex_active\":"<<(board->liveMidiController()?board->liveMidiController()->legacySysExCalls.activeCalls():0)
        <<",\"native_legacy_sys_ex_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->legacySysExCalls.partialEntries:0)
        <<",\"native_legacy_sys_ex_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->legacySysExCalls.handedOff:0)
        <<",\"native_state_multi_save_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiSaveCalls.started:0)
        <<",\"native_state_multi_save_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiSaveCalls.completed:0)
        <<",\"native_state_multi_save_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiSaveCalls.initialActive:0)
        <<",\"native_state_multi_save_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiSaveCalls.activeCalls():0)
        <<",\"native_state_multi_save_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateMultiSaveCalls.partialEntries:0)
        <<",\"native_state_name_codec_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateNameCodecCalls.started:0)
        <<",\"native_state_name_codec_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateNameCodecCalls.completed:0)
        <<",\"native_state_name_codec_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateNameCodecCalls.initialActive:0)
        <<",\"native_state_name_codec_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateNameCodecCalls.activeCalls():0)
        <<",\"native_state_name_codec_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateNameCodecCalls.partialEntries:0)
        <<",\"native_state_deferred_parts_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateDeferredPartsCalls.started:0)
        <<",\"native_state_deferred_parts_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateDeferredPartsCalls.completed:0)
        <<",\"native_state_deferred_parts_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateDeferredPartsCalls.initialActive:0)
        <<",\"native_state_deferred_parts_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateDeferredPartsCalls.activeCalls():0)
        <<",\"native_state_deferred_parts_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateDeferredPartsCalls.partialEntries:0)
        <<",\"native_state_panel_refresh_calls\":"<<(board->liveMidiController()?board->liveMidiController()->statePanelRefreshCalls.started:0)
        <<",\"native_state_panel_refresh_returns\":"<<(board->liveMidiController()?board->liveMidiController()->statePanelRefreshCalls.completed:0)
        <<",\"native_state_panel_refresh_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePanelRefreshCalls.initialActive:0)
        <<",\"native_state_panel_refresh_active\":"<<(board->liveMidiController()?board->liveMidiController()->statePanelRefreshCalls.activeCalls():0)
        <<",\"native_state_panel_refresh_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->statePanelRefreshCalls.partialEntries:0)
        <<",\"native_state_audition_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateAuditionCalls.started:0)
        <<",\"native_state_audition_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateAuditionCalls.completed:0)
        <<",\"native_state_audition_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateAuditionCalls.initialActive:0)
        <<",\"native_state_audition_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateAuditionCalls.activeCalls():0)
        <<",\"native_state_audition_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateAuditionCalls.partialEntries:0)
        <<",\"native_panel_publish_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelPublishCalls.started:0)
        <<",\"native_panel_publish_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelPublishCalls.completed:0)
        <<",\"native_panel_publish_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelPublishCalls.initialActive:0)
        <<",\"native_panel_publish_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelPublishCalls.activeCalls():0)
        <<",\"native_panel_publish_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelPublishCalls.partialEntries:0)
        <<",\"native_panel_masks_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelMasksCalls.started:0)
        <<",\"native_panel_masks_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelMasksCalls.completed:0)
        <<",\"native_panel_masks_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelMasksCalls.initialActive:0)
        <<",\"native_panel_masks_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelMasksCalls.activeCalls():0)
        <<",\"native_panel_masks_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelMasksCalls.partialEntries:0)
        <<",\"native_panel_initialize_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelInitializeCalls.started:0)
        <<",\"native_panel_initialize_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelInitializeCalls.completed:0)
        <<",\"native_panel_initialize_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelInitializeCalls.initialActive:0)
        <<",\"native_panel_initialize_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelInitializeCalls.activeCalls():0)
        <<",\"native_panel_initialize_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelInitializeCalls.partialEntries:0)
        <<",\"native_panel_detect_calls\":"<<(board->liveMidiController()?board->liveMidiController()->panelDetectCalls.started:0)
        <<",\"native_panel_detect_returns\":"<<(board->liveMidiController()?board->liveMidiController()->panelDetectCalls.completed:0)
        <<",\"native_panel_detect_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelDetectCalls.initialActive:0)
        <<",\"native_panel_detect_active\":"<<(board->liveMidiController()?board->liveMidiController()->panelDetectCalls.activeCalls():0)
        <<",\"native_panel_detect_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->panelDetectCalls.partialEntries:0)
        <<",\"native_firmware_layout_check_calls\":"<<(board->liveMidiController()?board->liveMidiController()->firmwareLayoutCheckCalls.started:0)
        <<",\"native_firmware_layout_check_returns\":"<<(board->liveMidiController()?board->liveMidiController()->firmwareLayoutCheckCalls.completed:0)
        <<",\"native_firmware_layout_check_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->firmwareLayoutCheckCalls.initialActive:0)
        <<",\"native_firmware_layout_check_active\":"<<(board->liveMidiController()?board->liveMidiController()->firmwareLayoutCheckCalls.activeCalls():0)
        <<",\"native_firmware_layout_check_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->firmwareLayoutCheckCalls.partialEntries:0)
        <<",\"native_sys_ex_dump_calls\":"<<(board->liveMidiController()?board->liveMidiController()->sysExDumpCalls.started:0)
        <<",\"native_sys_ex_dump_returns\":"<<(board->liveMidiController()?board->liveMidiController()->sysExDumpCalls.completed:0)
        <<",\"native_sys_ex_dump_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->sysExDumpCalls.initialActive:0)
        <<",\"native_sys_ex_dump_active\":"<<(board->liveMidiController()?board->liveMidiController()->sysExDumpCalls.activeCalls():0)
        <<",\"native_sys_ex_dump_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->sysExDumpCalls.partialEntries:0)
        <<",\"native_sys_ex_dump_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->sysExDumpCalls.handedOff:0)
        <<",\"native_midi_file_time_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileTimeCalls.started:0)
        <<",\"native_midi_file_time_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileTimeCalls.completed:0)
        <<",\"native_midi_file_time_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileTimeCalls.initialActive:0)
        <<",\"native_midi_file_time_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileTimeCalls.activeCalls():0)
        <<",\"native_midi_file_time_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileTimeCalls.partialEntries:0)
        <<",\"native_state_program_change_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramChangeCalls.started:0)
        <<",\"native_state_program_change_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramChangeCalls.completed:0)
        <<",\"native_state_program_change_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramChangeCalls.initialActive:0)
        <<",\"native_state_program_change_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramChangeCalls.activeCalls():0)
        <<",\"native_state_program_change_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateProgramChangeCalls.partialEntries:0)
        <<",\"native_state_name_editor_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateNameEditorCalls.started:0)
        <<",\"native_state_name_editor_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateNameEditorCalls.completed:0)
        <<",\"native_state_name_editor_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateNameEditorCalls.initialActive:0)
        <<",\"native_state_name_editor_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateNameEditorCalls.activeCalls():0)
        <<",\"native_state_name_editor_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateNameEditorCalls.partialEntries:0)
        <<",\"native_state_bank_display_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateBankDisplayCalls.started:0)
        <<",\"native_state_bank_display_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateBankDisplayCalls.completed:0)
        <<",\"native_state_bank_display_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateBankDisplayCalls.initialActive:0)
        <<",\"native_state_bank_display_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateBankDisplayCalls.activeCalls():0)
        <<",\"native_state_bank_display_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateBankDisplayCalls.partialEntries:0)
        <<",\"native_state_dsp_factory_transfer_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryTransferCalls.started:0)
        <<",\"native_state_dsp_factory_transfer_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryTransferCalls.completed:0)
        <<",\"native_state_dsp_factory_transfer_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryTransferCalls.initialActive:0)
        <<",\"native_state_dsp_factory_transfer_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryTransferCalls.activeCalls():0)
        <<",\"native_state_dsp_factory_transfer_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryTransferCalls.partialEntries:0)
        <<",\"native_state_dsp_factory_reset_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryResetCalls.started:0)
        <<",\"native_state_dsp_factory_reset_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryResetCalls.completed:0)
        <<",\"native_state_dsp_factory_reset_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryResetCalls.initialActive:0)
        <<",\"native_state_dsp_factory_reset_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryResetCalls.activeCalls():0)
        <<",\"native_state_dsp_factory_reset_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryResetCalls.partialEntries:0)
        <<",\"native_state_dsp_factory_upload_calls\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryUploadCalls.started:0)
        <<",\"native_state_dsp_factory_upload_returns\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryUploadCalls.completed:0)
        <<",\"native_state_dsp_factory_upload_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryUploadCalls.initialActive:0)
        <<",\"native_state_dsp_factory_upload_active\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryUploadCalls.activeCalls():0)
        <<",\"native_state_dsp_factory_upload_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->stateDspFactoryUploadCalls.partialEntries:0)
        <<",\"native_timing_tables_calls\":"<<(board->liveMidiController()?board->liveMidiController()->timingTablesCalls.started:0)
        <<",\"native_timing_tables_returns\":"<<(board->liveMidiController()?board->liveMidiController()->timingTablesCalls.completed:0)
        <<",\"native_timing_tables_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->timingTablesCalls.initialActive:0)
        <<",\"native_timing_tables_active\":"<<(board->liveMidiController()?board->liveMidiController()->timingTablesCalls.activeCalls():0)
        <<",\"native_timing_tables_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->timingTablesCalls.partialEntries:0)
        <<",\"native_midi_file_event_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileEventCalls.started:0)
        <<",\"native_midi_file_event_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileEventCalls.completed:0)
        <<",\"native_midi_file_event_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileEventCalls.initialActive:0)
        <<",\"native_midi_file_event_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileEventCalls.activeCalls():0)
        <<",\"native_midi_file_event_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileEventCalls.partialEntries:0)
        <<",\"native_midi_file_event_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileEventCalls.handedOff:0)
        <<",\"native_mono_arp_calls\":"<<(board->liveMidiController()?board->liveMidiController()->monoArpCalls.started:0)
        <<",\"native_mono_arp_returns\":"<<(board->liveMidiController()?board->liveMidiController()->monoArpCalls.completed:0)
        <<",\"native_mono_arp_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->monoArpCalls.initialActive:0)
        <<",\"native_mono_arp_active\":"<<(board->liveMidiController()?board->liveMidiController()->monoArpCalls.activeCalls():0)
        <<",\"native_mono_arp_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->monoArpCalls.partialEntries:0)
        <<",\"native_mono_arp_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->monoArpCalls.handedOff:0)
        <<",\"native_midi_file_transport_calls\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileTransportCalls.started:0)
        <<",\"native_midi_file_transport_returns\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileTransportCalls.completed:0)
        <<",\"native_midi_file_transport_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileTransportCalls.initialActive:0)
        <<",\"native_midi_file_transport_active\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileTransportCalls.activeCalls():0)
        <<",\"native_midi_file_transport_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->midiFileTransportCalls.partialEntries:0)
        <<",\"native_edit_address_calls\":"<<(board->liveMidiController()?board->liveMidiController()->editAddressCalls.started:0)
        <<",\"native_edit_address_returns\":"<<(board->liveMidiController()?board->liveMidiController()->editAddressCalls.completed:0)
        <<",\"native_edit_address_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->editAddressCalls.initialActive:0)
        <<",\"native_edit_address_active\":"<<(board->liveMidiController()?board->liveMidiController()->editAddressCalls.activeCalls():0)
        <<",\"native_edit_address_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->editAddressCalls.partialEntries:0)
        <<",\"native_edit_address_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->editAddressCalls.handedOff:0)
        <<",\"native_edit_scene_calls\":"<<(board->liveMidiController()?board->liveMidiController()->editSceneCalls.started:0)
        <<",\"native_edit_scene_returns\":"<<(board->liveMidiController()?board->liveMidiController()->editSceneCalls.completed:0)
        <<",\"native_edit_scene_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->editSceneCalls.initialActive:0)
        <<",\"native_edit_scene_active\":"<<(board->liveMidiController()?board->liveMidiController()->editSceneCalls.activeCalls():0)
        <<",\"native_edit_scene_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->editSceneCalls.partialEntries:0)
        <<",\"native_edit_decision_calls\":"<<(board->liveMidiController()?board->liveMidiController()->editDecisionCalls.started:0)
        <<",\"native_edit_decision_returns\":"<<(board->liveMidiController()?board->liveMidiController()->editDecisionCalls.completed:0)
        <<",\"native_edit_decision_initial_active\":"<<(board->liveMidiController()?board->liveMidiController()->editDecisionCalls.initialActive:0)
        <<",\"native_edit_decision_active\":"<<(board->liveMidiController()?board->liveMidiController()->editDecisionCalls.activeCalls():0)
        <<",\"native_edit_decision_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->editDecisionCalls.partialEntries:0)
        <<",\"native_edit_decision_handoffs\":"<<(board->liveMidiController()?board->liveMidiController()->editDecisionCalls.handedOff:0)
        <<",\"native_note_on_state_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteOnStateCalls.started:0)
        <<",\"native_note_on_state_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteOnStateCalls.completed:0)
        <<",\"native_note_on_state_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.noteOnStateCalls.partialEntries:0)
        <<",\"native_key_activity_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyActivityCalls.started:0)
        <<",\"native_key_activity_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyActivityCalls.completed:0)
        <<",\"native_key_activity_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyActivityCalls.partialEntries:0)
        <<",\"native_key_reset_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyResetCalls.started:0)
        <<",\"native_key_reset_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyResetCalls.completed:0)
        <<",\"native_key_reset_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.keyResetCalls.partialEntries:0)
        <<",\"native_voice_population_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.populationCalls.started:0)
        <<",\"native_voice_population_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.populationCalls.completed:0)
        <<",\"native_population_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.populationCalls.partialEntries:0)
        <<",\"native_voice_selection_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.selectionCalls.started:0)
        <<",\"native_voice_selection_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.selectionCalls.completed:0)
        <<",\"native_gate_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.selectionCalls.gateStarted:0)
        <<",\"native_gate_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.selectionCalls.gateCompleted:0)
        <<",\"native_selection_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.selectionCalls.partialEntries:0)
        <<",\"native_velocity_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.velocityCurveCalls.started:0)
        <<",\"native_velocity_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.velocityCurveCalls.completed:0)
        <<",\"native_velocity_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.velocityCurveCalls.partialEntries:0)
        <<",\"native_part_lookup_calls\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.partControlCalls.lookupStarted:0)
        <<",\"native_part_lookup_returns\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.partControlCalls.lookupCompleted:0)
        <<",\"native_part_mirror_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partControlCalls.mirrorStarted:0)
        <<",\"native_part_mirror_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.partControlCalls.mirrorCompleted:0)
        <<",\"native_part_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->noteActions.partControlCalls.partialEntries+board->liveMidiController()->voiceActions.partControlCalls.partialEntries:0)
        <<",\"native_routing_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.routingCalls.started:0)
        <<",\"native_routing_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.routingCalls.completed:0)
        <<",\"native_routing_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.routingCalls.partialEntries:0)
        <<",\"native_select_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.selectCalls.started:0)
        <<",\"native_select_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.selectCalls.completed:0)
        <<",\"native_select_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.selectCalls.partialEntries:0)
        <<",\"native_boot_setup_calls_before_metrics\":"<<bootSetupStarts
        <<",\"native_boot_setup_returns_before_metrics\":"<<bootSetupReturns
        <<",\"native_boot_setup_test_partial_before_metrics\":"<<bootSetupPartial
        <<",\"native_boot_setup_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->bootSetupCalls.partialEntries:0)
        <<",\"native_boot_stream_calls_before_metrics\":"<<bootStreamStarts
        <<",\"native_boot_stream_returns_before_metrics\":"<<bootStreamReturns
        <<",\"native_boot_stream_test_partial_before_metrics\":"<<bootStreamPartial
        <<",\"native_boot_stream_calls\":"<<(board->liveMidiController()?board->liveMidiController()->bootStreamCalls.started:0)
        <<",\"native_boot_stream_returns\":"<<(board->liveMidiController()?board->liveMidiController()->bootStreamCalls.completed:0)
        <<",\"native_boot_stream_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->bootStreamCalls.partialEntries:0)
        <<",\"native_dsp_fault_calls\":"<<(board->liveMidiController()?board->liveMidiController()->faultCalls.started:0)
        <<",\"native_dsp_fault_returns\":"<<(board->liveMidiController()?board->liveMidiController()->faultCalls.completed:0)
        <<",\"native_dsp_fault_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->faultCalls.partialEntries:0)
        <<",\"native_dsp_address_calls\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.addressCalls.started:0)
        <<",\"native_dsp_address_returns\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.addressCalls.completed:0)
        <<",\"native_dsp_address_test_partial\":"<<(board->liveMidiController()?board->liveMidiController()->voiceActions.addressCalls.partialEntries:0)
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
    if(const auto* runtime=board->nativeController())report<<",\"native_runtime_steps\":"<<runtime->steps<<",\"native_interrupt_entries\":"<<runtime->interruptEntries
        <<",\"native_serial_foreground_calls\":"<<runtime->serialForeground.started
        <<",\"native_serial_foreground_returns\":"<<runtime->serialForeground.completed
        <<",\"native_serial_foreground_phases\":"<<runtime->serialForeground.phases
        <<",\"native_serial_foreground_initial_active\":"<<runtime->serialForeground.initial
        <<",\"native_serial_foreground_active\":"<<runtime->serialForeground.active()
        <<",\"native_serial_foreground_reset_rx\":"<<runtime->serialForeground.kinds[0]
        <<",\"native_serial_foreground_pop_rx\":"<<runtime->serialForeground.kinds[1]
        <<",\"native_serial_foreground_receive\":"<<runtime->serialForeground.kinds[2]
        <<",\"native_serial_foreground_reset_tx\":"<<runtime->serialForeground.kinds[3]
        <<",\"native_serial_foreground_transmit\":"<<runtime->serialForeground.kinds[4]
        <<",\"native_state_reset_calls\":"<<runtime->stateResetForeground.started
        <<",\"native_state_reset_returns\":"<<runtime->stateResetForeground.completed
        <<",\"native_state_reset_phases\":"<<runtime->stateResetForeground.phases
        <<",\"native_state_reset_initial_active\":"<<runtime->stateResetForeground.initialActive
        <<",\"native_state_reset_active\":"<<runtime->stateResetForeground.active()
        <<",\"native_state_reset_components\":"<<runtime->stateResetForeground.components
        <<",\"native_state_reset_state\":"<<runtime->stateResetForeground.kinds[0]
        <<",\"native_state_reset_buttons\":"<<runtime->stateResetForeground.kinds[1]
        <<",\"native_state_reset_controls\":"<<runtime->stateResetForeground.kinds[2]
        <<",\"native_state_reset_keyboard\":"<<runtime->stateResetForeground.kinds[3]
        <<",\"native_panel_scan_calls\":"<<runtime->panelScanForeground.started
        <<",\"native_panel_scan_returns\":"<<runtime->panelScanForeground.completed
        <<",\"native_panel_scan_phases\":"<<runtime->panelScanForeground.phases
        <<",\"native_panel_scan_initial_active\":"<<runtime->panelScanForeground.initial
        <<",\"native_panel_scan_active\":"<<runtime->panelScanForeground.active()
        <<",\"native_panel_scan_components\":"<<runtime->panelScanForeground.components
        <<",\"native_panel_scan_initialize\":"<<runtime->panelScanForeground.kinds[0]
        <<",\"native_panel_scan_scan\":"<<runtime->panelScanForeground.kinds[1]
        <<",\"native_panel_scan_button_push\":"<<runtime->panelScanForeground.kinds[2]
        <<",\"native_panel_controls_calls\":"<<runtime->panelControlsForeground.started
        <<",\"native_panel_controls_returns\":"<<runtime->panelControlsForeground.completed
        <<",\"native_panel_controls_phases\":"<<runtime->panelControlsForeground.phases
        <<",\"native_panel_controls_initial_active\":"<<runtime->panelControlsForeground.initialActive
        <<",\"native_panel_controls_active\":"<<runtime->panelControlsForeground.active()
        <<",\"native_panel_controls_components\":"<<runtime->panelControlsForeground.components
        <<",\"native_panel_controls_switches\":"<<runtime->panelControlsForeground.kinds[0]
        <<",\"native_panel_controls_pedal_reset\":"<<runtime->panelControlsForeground.kinds[1]
        <<",\"native_panel_controls_pedal\":"<<runtime->panelControlsForeground.kinds[2]
        <<",\"native_panel_controls_encoder\":"<<runtime->panelControlsForeground.kinds[3]
        <<",\"native_panel_controls_adc_start\":"<<runtime->panelControlsForeground.kinds[4]
        <<",\"native_panel_controls_adc_mux\":"<<runtime->panelControlsForeground.kinds[5]
        <<",\"native_panel_controls_adc_reset_last\":"<<runtime->panelControlsForeground.kinds[6]
        <<",\"native_state_events_calls\":"<<runtime->stateEventsForeground.started
        <<",\"native_state_events_returns\":"<<runtime->stateEventsForeground.completed
        <<",\"native_state_events_phases\":"<<runtime->stateEventsForeground.phases
        <<",\"native_state_events_initial_active\":"<<runtime->stateEventsForeground.initial
        <<",\"native_state_events_active\":"<<runtime->stateEventsForeground.active()
        <<",\"native_state_events_components\":"<<runtime->stateEventsForeground.components
        <<",\"native_state_events_button_pop\":"<<runtime->stateEventsForeground.kinds[0]
        <<",\"native_state_events_control_pop\":"<<runtime->stateEventsForeground.kinds[1]
        <<",\"native_state_events_control_peek\":"<<runtime->stateEventsForeground.kinds[2]
        <<",\"native_state_events_keyboard_pop\":"<<runtime->stateEventsForeground.kinds[3]
        <<",\"native_state_events_convert\":"<<runtime->stateEventsForeground.kinds[4]
        <<",\"native_state_events_scale\":"<<runtime->stateEventsForeground.kinds[5]
        <<",\"native_state_events_square\":"<<runtime->stateEventsForeground.kinds[6]
        <<",\"native_state_events_coefficients\":"<<runtime->stateEventsForeground.kinds[7]
        <<",\"native_state_events_range\":"<<runtime->stateEventsForeground.kinds[8]
        <<",\"native_state_panel_calls\":"<<runtime->statePanelForeground.started
        <<",\"native_state_panel_returns\":"<<runtime->statePanelForeground.completed
        <<",\"native_state_panel_phases\":"<<runtime->statePanelForeground.phases
        <<",\"native_state_panel_initial_active\":"<<runtime->statePanelForeground.initial
        <<",\"native_state_panel_active\":"<<runtime->statePanelForeground.active()
        <<",\"native_state_panel_components\":"<<runtime->statePanelForeground.components
        <<",\"native_state_panel_controls\":"<<runtime->statePanelForeground.kinds[0]
        <<",\"native_state_menu_calls\":"<<runtime->stateMenuForeground.started
        <<",\"native_state_menu_returns\":"<<runtime->stateMenuForeground.completed
        <<",\"native_state_menu_phases\":"<<runtime->stateMenuForeground.phases
        <<",\"native_state_menu_initial_active\":"<<runtime->stateMenuForeground.initial
        <<",\"native_state_menu_active\":"<<runtime->stateMenuForeground.active()
        <<",\"native_state_menu_components\":"<<runtime->stateMenuForeground.components
        <<",\"native_state_menu_toggle\":"<<runtime->stateMenuForeground.kinds[0]
        <<",\"native_state_menu_auxiliary\":"<<runtime->stateMenuForeground.kinds[1]
        <<",\"native_state_menu_previous\":"<<runtime->stateMenuForeground.kinds[2]
        <<",\"native_state_menu_repeat_previous\":"<<runtime->stateMenuForeground.kinds[3]
        <<",\"native_state_menu_next\":"<<runtime->stateMenuForeground.kinds[4]
        <<",\"native_state_menu_repeat_next\":"<<runtime->stateMenuForeground.kinds[5]
        <<",\"native_state_menu_edit_active\":"<<runtime->stateMenuForeground.kinds[6]
        <<",\"native_state_menu_repeat_arm\":"<<runtime->stateMenuForeground.kinds[7]
        <<",\"native_state_menu_clock\":"<<runtime->stateMenuForeground.kinds[8]
        <<",\"native_state_menu_bind_context\":"<<runtime->stateMenuForeground.kinds[9]
        <<",\"native_state_menu_clear_context\":"<<runtime->stateMenuForeground.kinds[10]
        <<",\"native_state_menu_indexed_address\":"<<runtime->stateMenuForeground.kinds[11]
        <<",\"native_state_menu_direct_address\":"<<runtime->stateMenuForeground.kinds[12]
        <<",\"native_state_menu_global_address\":"<<runtime->stateMenuForeground.kinds[13]
        <<",\"native_state_store_calls\":"<<runtime->stateStoreForeground.started
        <<",\"native_state_store_returns\":"<<runtime->stateStoreForeground.completed
        <<",\"native_state_store_phases\":"<<runtime->stateStoreForeground.phases
        <<",\"native_state_store_initial_active\":"<<runtime->stateStoreForeground.initial
        <<",\"native_state_store_active\":"<<runtime->stateStoreForeground.active()
        <<",\"native_state_store_components\":"<<runtime->stateStoreForeground.components
        <<",\"native_state_store_multi_serialize\":"<<runtime->stateStoreForeground.kinds[0]
        <<",\"native_state_store_pack_all\":"<<runtime->stateStoreForeground.kinds[1]
        <<",\"native_state_store_pack_one\":"<<runtime->stateStoreForeground.kinds[2]
        <<",\"native_state_store_oscillators\":"<<runtime->stateStoreForeground.kinds[3]
        <<",\"native_state_store_unison\":"<<runtime->stateStoreForeground.kinds[4]
        <<",\"native_state_store_lfo\":"<<runtime->stateStoreForeground.kinds[5]
        <<",\"native_state_store_filter\":"<<runtime->stateStoreForeground.kinds[6]
        <<",\"native_state_store_envelope\":"<<runtime->stateStoreForeground.kinds[7]
        <<",\"native_state_store_program_mode\":"<<runtime->stateStoreForeground.kinds[8]
        <<",\"native_state_store_read_patch_name\":"<<runtime->stateStoreForeground.kinds[9]
        <<",\"native_state_store_read_multi_name\":"<<runtime->stateStoreForeground.kinds[10]
        <<",\"native_state_store_read_provided_name\":"<<runtime->stateStoreForeground.kinds[11]
        <<",\"native_state_store_write_patch_name\":"<<runtime->stateStoreForeground.kinds[12]
        <<",\"native_state_store_write_multi_name\":"<<runtime->stateStoreForeground.kinds[13]
        <<",\"native_state_store_write_provided_name\":"<<runtime->stateStoreForeground.kinds[14]
        <<",\"native_state_store_write_selected_name\":"<<runtime->stateStoreForeground.kinds[15]
        <<",\"native_state_store_read_selected_name\":"<<runtime->stateStoreForeground.kinds[16]
        <<",\"native_state_display_calls\":"<<runtime->stateDisplayForeground.started
        <<",\"native_state_display_returns\":"<<runtime->stateDisplayForeground.completed
        <<",\"native_state_display_phases\":"<<runtime->stateDisplayForeground.phases
        <<",\"native_state_display_initial_active\":"<<runtime->stateDisplayForeground.initial
        <<",\"native_state_display_active\":"<<runtime->stateDisplayForeground.active()
        <<",\"native_state_display_components\":"<<runtime->stateDisplayForeground.components
        <<",\"native_state_display_format\":"<<runtime->stateDisplayForeground.kinds[0]
        <<",\"native_state_display_off\":"<<runtime->stateDisplayForeground.kinds[1]
        <<",\"native_state_display_text\":"<<runtime->stateDisplayForeground.kinds[2]
        <<",\"native_state_display_character\":"<<runtime->stateDisplayForeground.kinds[3]
        <<",\"native_state_display_command\":"<<runtime->stateDisplayForeground.kinds[4]
        <<",\"native_state_apply_calls\":"<<runtime->stateApplyForeground.started
        <<",\"native_state_apply_returns\":"<<runtime->stateApplyForeground.completed
        <<",\"native_state_apply_phases\":"<<runtime->stateApplyForeground.phases
        <<",\"native_state_apply_initial_active\":"<<runtime->stateApplyForeground.initialActive
        <<",\"native_state_apply_active\":"<<runtime->stateApplyForeground.active()
        <<",\"native_state_apply_components\":"<<runtime->stateApplyForeground.components
        <<",\"native_state_apply_apply\":"<<runtime->stateApplyForeground.kinds[0]
        <<",\"native_state_apply_post\":"<<runtime->stateApplyForeground.kinds[1]
        <<",\"native_state_apply_stored_value\":"<<runtime->stateApplyForeground.kinds[2]
        <<",\"native_state_apply_classify_stored\":"<<runtime->stateApplyForeground.kinds[3]
        <<",\"native_state_apply_current_preset\":"<<runtime->stateApplyForeground.kinds[4]
        <<",\"native_state_apply_overlay_preset\":"<<runtime->stateApplyForeground.kinds[5]
        <<",\"native_state_apply_program_index\":"<<runtime->stateApplyForeground.kinds[6]
        <<",\"native_state_save_calls\":"<<runtime->stateSaveForeground.started
        <<",\"native_state_save_returns\":"<<runtime->stateSaveForeground.completed
        <<",\"native_state_save_phases\":"<<runtime->stateSaveForeground.phases
        <<",\"native_state_save_initial_active\":"<<runtime->stateSaveForeground.initial
        <<",\"native_state_save_active\":"<<runtime->stateSaveForeground.active()
        <<",\"native_state_save_components\":"<<runtime->stateSaveForeground.components
        <<",\"native_state_save_commit_multi\":"<<runtime->stateSaveForeground.kinds[0]
        <<",\"native_state_save_commit_single\":"<<runtime->stateSaveForeground.kinds[1]
        <<",\"native_state_save_preset_save\":"<<runtime->stateSaveForeground.kinds[2]
        <<",\"native_state_save_format_direct\":"<<runtime->stateSaveForeground.kinds[3]
        <<",\"native_state_save_format_part\":"<<runtime->stateSaveForeground.kinds[4]
        <<",\"native_state_save_offset\":"<<runtime->stateSaveForeground.kinds[5]
        <<",\"native_state_save_check\":"<<runtime->stateSaveForeground.kinds[6]
        <<",\"native_state_save_message\":"<<runtime->stateSaveForeground.kinds[7]
        <<",\"native_state_save_flash_commit\":"<<runtime->stateSaveForeground.kinds[8]
        <<",\"native_state_save_global_save\":"<<runtime->stateSaveForeground.kinds[9]
        <<",\"native_state_save_multi_current\":"<<runtime->stateSaveForeground.kinds[10]
        <<",\"native_state_save_multi_provided\":"<<runtime->stateSaveForeground.kinds[11]
        <<",\"native_state_save_flash_program\":"<<runtime->stateSaveForeground.kinds[12]
        <<",\"native_state_save_flash_compare\":"<<runtime->stateSaveForeground.kinds[13]
        <<",\"native_state_save_notice\":"<<runtime->stateSaveForeground.kinds[14]
        <<",\"native_panel_settings_service_calls\":"<<runtime->panelSettingsForeground.started
        <<",\"native_panel_settings_service_returns\":"<<runtime->panelSettingsForeground.completed
        <<",\"native_panel_settings_service_phases\":"<<runtime->panelSettingsForeground.phases
        <<",\"native_panel_settings_service_initial_active\":"<<runtime->panelSettingsForeground.initial
        <<",\"native_panel_settings_service_active\":"<<runtime->panelSettingsForeground.active()
        <<",\"native_panel_settings_service_components\":"<<runtime->panelSettingsForeground.components
        <<",\"native_panel_settings_service_Reset\":"<<runtime->panelSettingsForeground.kinds[0]
        <<",\"native_panel_settings_service_Select\":"<<runtime->panelSettingsForeground.kinds[1]
        <<",\"native_panel_settings_service_Show\":"<<runtime->panelSettingsForeground.kinds[2]
        <<",\"native_panel_settings_service_Poll\":"<<runtime->panelSettingsForeground.kinds[3]
        <<",\"native_panel_settings_service_Coefficients\":"<<runtime->panelSettingsForeground.kinds[4]
        <<",\"native_panel_settings_service_First\":"<<runtime->panelSettingsForeground.kinds[5]
        <<",\"native_panel_settings_service_Second\":"<<runtime->panelSettingsForeground.kinds[6]
        <<",\"native_panel_settings_service_Load\":"<<runtime->panelSettingsForeground.kinds[7]
        <<",\"native_panel_settings_service_Save\":"<<runtime->panelSettingsForeground.kinds[8]
        <<",\"native_panel_settings_service_Warmup\":"<<runtime->panelSettingsForeground.kinds[9]
        <<",\"native_panel_settings_service_AnalogInitialize\":"<<runtime->panelSettingsForeground.kinds[10]
        <<",\"native_panel_settings_service_AnalogDuty\":"<<runtime->panelSettingsForeground.kinds[11]
        <<",\"native_panel_settings_service_AnalogLevel\":"<<runtime->panelSettingsForeground.kinds[12]
        <<",\"native_state_screen_service_calls\":"<<runtime->stateScreenForeground.started
        <<",\"native_state_screen_service_returns\":"<<runtime->stateScreenForeground.completed
        <<",\"native_state_screen_service_phases\":"<<runtime->stateScreenForeground.phases
        <<",\"native_state_screen_service_initial_active\":"<<runtime->stateScreenForeground.initial
        <<",\"native_state_screen_service_active\":"<<runtime->stateScreenForeground.active()
        <<",\"native_state_screen_service_components\":"<<runtime->stateScreenForeground.components
        <<",\"native_state_screen_service_Descriptor\":"<<runtime->stateScreenForeground.kinds[0]
        <<",\"native_state_screen_service_Set\":"<<runtime->stateScreenForeground.kinds[1]
        <<",\"native_state_screen_service_Next\":"<<runtime->stateScreenForeground.kinds[2]
        <<",\"native_state_screen_service_Previous\":"<<runtime->stateScreenForeground.kinds[3]
        <<",\"native_state_screen_service_Refresh\":"<<runtime->stateScreenForeground.kinds[4]
        <<",\"native_state_screen_service_Gate\":"<<runtime->stateScreenForeground.kinds[5]
        <<",\"native_state_screen_service_Render\":"<<runtime->stateScreenForeground.kinds[6]
        <<",\"native_state_screen_service_Label\":"<<runtime->stateScreenForeground.kinds[7]
        <<",\"native_state_screen_service_Edit\":"<<runtime->stateScreenForeground.kinds[8]
        <<",\"native_state_screen_service_Value\":"<<runtime->stateScreenForeground.kinds[9]
        <<",\"native_state_field_service_calls\":"<<runtime->stateFieldForeground.started
        <<",\"native_state_field_service_returns\":"<<runtime->stateFieldForeground.completed
        <<",\"native_state_field_service_phases\":"<<runtime->stateFieldForeground.phases
        <<",\"native_state_field_service_initial_active\":"<<runtime->stateFieldForeground.initial
        <<",\"native_state_field_service_active\":"<<runtime->stateFieldForeground.active()
        <<",\"native_state_field_service_components\":"<<runtime->stateFieldForeground.components
        <<",\"native_state_field_service_automatic_edit\":"<<runtime->stateFieldForeground.kinds[0]
        <<",\"native_state_field_service_automatic_event\":"<<runtime->stateFieldForeground.kinds[1]
        <<",\"native_state_field_service_manual_edit\":"<<runtime->stateFieldForeground.kinds[2]
        <<",\"native_state_field_service_direct_edit\":"<<runtime->stateFieldForeground.kinds[3]
        <<",\"native_state_field_service_publish\":"<<runtime->stateFieldForeground.kinds[4]
        <<",\"native_state_field_service_global\":"<<runtime->stateFieldForeground.kinds[5]
        <<",\"native_state_field_service_selector\":"<<runtime->stateFieldForeground.kinds[6]
        <<",\"native_state_field_service_indexed\":"<<runtime->stateFieldForeground.kinds[7]
        <<",\"native_state_field_service_clear\":"<<runtime->stateFieldForeground.kinds[8]
        <<",\"native_state_field_service_direct_step\":"<<runtime->stateFieldForeground.kinds[9]
        <<",\"native_state_field_service_indexed_step\":"<<runtime->stateFieldForeground.kinds[10]
        <<",\"native_state_field_service_bit\":"<<runtime->stateFieldForeground.kinds[11]
        <<",\"native_state_field_service_pair\":"<<runtime->stateFieldForeground.kinds[12]
        <<",\"native_state_field_service_return\":"<<runtime->stateFieldForeground.kinds[13]
        <<",\"native_state_field_service_current\":"<<runtime->stateFieldForeground.kinds[14]
        <<",\"native_state_field_service_cycle\":"<<runtime->stateFieldForeground.kinds[15]
        <<",\"native_state_field_service_control\":"<<runtime->stateFieldForeground.kinds[16]
        <<",\"native_state_field_service_label\":"<<runtime->stateFieldForeground.kinds[17]
        <<",\"native_state_field_service_bounds\":"<<runtime->stateFieldForeground.kinds[18]
        <<",\"native_state_field_service_indexed_address\":"<<runtime->stateFieldForeground.kinds[19]
        <<",\"native_state_field_service_direct_address\":"<<runtime->stateFieldForeground.kinds[20]
        <<",\"native_state_field_service_global_address\":"<<runtime->stateFieldForeground.kinds[21]
        <<",\"native_state_field_service_edit_active\":"<<runtime->stateFieldForeground.kinds[22]
        <<",\"native_state_field_service_apply_velocity\":"<<runtime->stateFieldForeground.kinds[23]
        <<",\"native_state_field_service_velocity\":"<<runtime->stateFieldForeground.kinds[24]
        <<",\"native_state_store_menu_service_calls\":"<<runtime->stateStoreMenuForeground.started
        <<",\"native_state_store_menu_service_returns\":"<<runtime->stateStoreMenuForeground.completed
        <<",\"native_state_store_menu_service_phases\":"<<runtime->stateStoreMenuForeground.phases
        <<",\"native_state_store_menu_service_initial_active\":"<<runtime->stateStoreMenuForeground.initial
        <<",\"native_state_store_menu_service_active\":"<<runtime->stateStoreMenuForeground.active()
        <<",\"native_state_store_menu_service_components\":"<<runtime->stateStoreMenuForeground.components
        <<",\"native_state_store_menu_service_menu\":"<<runtime->stateStoreMenuForeground.kinds[0]
        <<",\"native_state_store_menu_service_begin_program\":"<<runtime->stateStoreMenuForeground.kinds[1]
        <<",\"native_state_store_menu_service_begin_name\":"<<runtime->stateStoreMenuForeground.kinds[2]
        <<",\"native_state_store_menu_service_begin_store\":"<<runtime->stateStoreMenuForeground.kinds[3]
        <<",\"native_state_program_browser_service_calls\":"<<runtime->stateProgramBrowserForeground.started
        <<",\"native_state_program_browser_service_returns\":"<<runtime->stateProgramBrowserForeground.completed
        <<",\"native_state_program_browser_service_phases\":"<<runtime->stateProgramBrowserForeground.phases
        <<",\"native_state_program_browser_service_initial_active\":"<<runtime->stateProgramBrowserForeground.initial
        <<",\"native_state_program_browser_service_active\":"<<runtime->stateProgramBrowserForeground.active()
        <<",\"native_state_program_browser_service_components\":"<<runtime->stateProgramBrowserForeground.components
        <<",\"native_state_program_browser_service_browse\":"<<runtime->stateProgramBrowserForeground.kinds[0]
        <<",\"native_state_program_browser_service_repeat\":"<<runtime->stateProgramBrowserForeground.kinds[1]
        <<",\"native_state_program_browser_service_slow\":"<<runtime->stateProgramBrowserForeground.kinds[2]
        <<",\"native_state_program_browser_service_fast\":"<<runtime->stateProgramBrowserForeground.kinds[3]
        <<",\"native_state_program_browser_service_stop\":"<<runtime->stateProgramBrowserForeground.kinds[4]
        <<",\"native_state_program_browser_service_stopped\":"<<runtime->stateProgramBrowserForeground.kinds[5]
        <<",\"native_state_program_browser_service_open\":"<<runtime->stateProgramBrowserForeground.kinds[6]
        <<",\"native_state_program_browser_service_close\":"<<runtime->stateProgramBrowserForeground.kinds[7]
        <<",\"native_state_program_browser_service_save\":"<<runtime->stateProgramBrowserForeground.kinds[8]
        <<",\"native_state_program_browser_service_restore\":"<<runtime->stateProgramBrowserForeground.kinds[9]
        <<",\"native_state_program_browser_service_category\":"<<runtime->stateProgramBrowserForeground.kinds[10]
        <<",\"native_state_program_browser_service_step\":"<<runtime->stateProgramBrowserForeground.kinds[11]
        <<",\"native_state_program_browser_service_entry\":"<<runtime->stateProgramBrowserForeground.kinds[12]
        <<",\"native_state_program_browser_service_clear\":"<<runtime->stateProgramBrowserForeground.kinds[13]
        <<",\"native_clock_sync_service_calls\":"<<runtime->clockSyncForeground.started
        <<",\"native_clock_sync_service_returns\":"<<runtime->clockSyncForeground.completed
        <<",\"native_clock_sync_service_phases\":"<<runtime->clockSyncForeground.phases
        <<",\"native_clock_sync_service_initial_active\":"<<runtime->clockSyncForeground.initial
        <<",\"native_clock_sync_service_active\":"<<runtime->clockSyncForeground.active()
        <<",\"native_clock_sync_service_components\":"<<runtime->clockSyncForeground.components
        <<",\"native_clock_sync_service_update\":"<<runtime->clockSyncForeground.kinds[0]
        <<",\"native_state_deferred_parts_service_calls\":"<<runtime->stateDeferredPartsForeground.started
        <<",\"native_state_deferred_parts_service_returns\":"<<runtime->stateDeferredPartsForeground.completed
        <<",\"native_state_deferred_parts_service_phases\":"<<runtime->stateDeferredPartsForeground.phases
        <<",\"native_state_deferred_parts_service_initial_active\":"<<runtime->stateDeferredPartsForeground.initial
        <<",\"native_state_deferred_parts_service_active\":"<<runtime->stateDeferredPartsForeground.active()
        <<",\"native_state_deferred_parts_service_components\":"<<runtime->stateDeferredPartsForeground.components
        <<",\"native_state_deferred_parts_service_deferred\":"<<runtime->stateDeferredPartsForeground.kinds[0]
        <<",\"native_state_deferred_parts_service_processed\":"<<runtime->stateDeferredPartsForeground.processed
        <<",\"native_state_deferred_parts_service_voice_uploads\":"<<runtime->stateDeferredPartsForeground.voiceUploads
        <<",\"native_state_deferred_parts_service_part_mask\":"<<runtime->stateDeferredPartsForeground.partMask
        <<",\"native_state_factory_service_calls\":"<<runtime->stateFactoryForeground.started
        <<",\"native_state_factory_service_returns\":"<<runtime->stateFactoryForeground.completed
        <<",\"native_state_factory_service_phases\":"<<runtime->stateFactoryForeground.phases
        <<",\"native_state_factory_service_initial_active\":"<<runtime->stateFactoryForeground.initial
        <<",\"native_state_factory_service_active\":"<<runtime->stateFactoryForeground.active()
        <<",\"native_state_factory_service_components\":"<<runtime->stateFactoryForeground.components
        <<",\"native_state_factory_service_provided\":"<<runtime->stateFactoryForeground.kinds[0]
        <<",\"native_state_factory_service_factory\":"<<runtime->stateFactoryForeground.kinds[1]
        <<",\"native_state_factory_service_global_load\":"<<runtime->stateFactoryForeground.kinds[2]
        <<",\"native_state_factory_service_reset\":"<<runtime->stateFactoryForeground.kinds[3]
        <<",\"native_state_factory_service_upload\":"<<runtime->stateFactoryForeground.kinds[4]
        <<",\"native_state_factory_service_poll\":"<<runtime->stateFactoryForeground.kinds[5]
        <<",\"native_state_factory_service_stream\":"<<runtime->stateFactoryForeground.kinds[6]
        <<",\"native_state_program_load_service_calls\":"<<runtime->stateProgramLoadForeground.started
        <<",\"native_state_program_load_service_returns\":"<<runtime->stateProgramLoadForeground.completed
        <<",\"native_state_program_load_service_phases\":"<<runtime->stateProgramLoadForeground.phases
        <<",\"native_state_program_load_service_initial_active\":"<<runtime->stateProgramLoadForeground.initial
        <<",\"native_state_program_load_service_active\":"<<runtime->stateProgramLoadForeground.active()
        <<",\"native_state_program_load_service_components\":"<<runtime->stateProgramLoadForeground.components
        <<",\"native_state_program_load_service_commit_single\":"<<runtime->stateProgramLoadForeground.kinds[0]
        <<",\"native_state_program_load_service_commit_multi\":"<<runtime->stateProgramLoadForeground.kinds[1]
        <<",\"native_state_program_load_service_load_single\":"<<runtime->stateProgramLoadForeground.kinds[2]
        <<",\"native_state_playback_mode_service_calls\":"<<runtime->statePlaybackModeForeground.started
        <<",\"native_state_playback_mode_service_returns\":"<<runtime->statePlaybackModeForeground.completed
        <<",\"native_state_playback_mode_service_phases\":"<<runtime->statePlaybackModeForeground.phases
        <<",\"native_state_playback_mode_service_initial_active\":"<<runtime->statePlaybackModeForeground.initial
        <<",\"native_state_playback_mode_service_active\":"<<runtime->statePlaybackModeForeground.active()
        <<",\"native_state_playback_mode_service_components\":"<<runtime->statePlaybackModeForeground.components
        <<",\"native_state_playback_mode_service_single\":"<<runtime->statePlaybackModeForeground.kinds[0]
        <<",\"native_state_playback_mode_service_multi\":"<<runtime->statePlaybackModeForeground.kinds[1]
        <<",\"native_state_playback_mode_service_playback\":"<<runtime->statePlaybackModeForeground.kinds[2]
        <<",\"native_state_playback_mode_service_reset_playback\":"<<runtime->statePlaybackModeForeground.kinds[3]
        <<",\"native_state_playback_mode_service_clear_ui\":"<<runtime->statePlaybackModeForeground.kinds[4]
        <<",\"native_state_playback_mode_service_reset_voices\":"<<runtime->statePlaybackModeForeground.kinds[5]
        <<",\"native_state_playback_mode_service_routing\":"<<runtime->statePlaybackModeForeground.kinds[6]
        <<",\"native_state_playback_mode_service_resolve_parts\":"<<runtime->statePlaybackModeForeground.kinds[7]
        <<",\"native_state_playback_mode_service_bind_part\":"<<runtime->statePlaybackModeForeground.kinds[8]
        <<",\"native_state_playback_mode_service_resolve_edit_part\":"<<runtime->statePlaybackModeForeground.kinds[9]
        <<",\"native_state_part_navigation_service_calls\":"<<runtime->statePartNavigationForeground.started
        <<",\"native_state_part_navigation_service_returns\":"<<runtime->statePartNavigationForeground.completed
        <<",\"native_state_part_navigation_service_phases\":"<<runtime->statePartNavigationForeground.phases
        <<",\"native_state_part_navigation_service_initial_active\":"<<runtime->statePartNavigationForeground.initial
        <<",\"native_state_part_navigation_service_active\":"<<runtime->statePartNavigationForeground.active()
        <<",\"native_state_part_navigation_service_components\":"<<runtime->statePartNavigationForeground.components
        <<",\"native_state_part_navigation_service_down\":"<<runtime->statePartNavigationForeground.kinds[0]
        <<",\"native_state_part_navigation_service_up\":"<<runtime->statePartNavigationForeground.kinds[1]
        <<",\"native_state_part_navigation_service_step\":"<<runtime->statePartNavigationForeground.kinds[2]
        <<",\"native_state_part_navigation_service_part_step\":"<<runtime->statePartNavigationForeground.kinds[3]
        <<",\"native_state_part_navigation_service_return\":"<<runtime->statePartNavigationForeground.kinds[4]
        <<",\"native_state_edit_action_service_calls\":"<<runtime->stateEditActionForeground.started
        <<",\"native_state_edit_action_service_returns\":"<<runtime->stateEditActionForeground.completed
        <<",\"native_state_edit_action_service_phases\":"<<runtime->stateEditActionForeground.phases
        <<",\"native_state_edit_action_service_initial_active\":"<<runtime->stateEditActionForeground.initial
        <<",\"native_state_edit_action_service_active\":"<<runtime->stateEditActionForeground.active()
        <<",\"native_state_edit_action_service_components\":"<<runtime->stateEditActionForeground.components
        <<",\"native_state_edit_action_service_toggle_edit\":"<<runtime->stateEditActionForeground.kinds[0]
        <<",\"native_state_edit_action_service_select_edit\":"<<runtime->stateEditActionForeground.kinds[1]
        <<",\"native_state_edit_action_service_toggle_one\":"<<runtime->stateEditActionForeground.kinds[2]
        <<",\"native_state_edit_action_service_toggle_two\":"<<runtime->stateEditActionForeground.kinds[3]
        <<",\"native_state_edit_action_service_clear_digits\":"<<runtime->stateEditActionForeground.kinds[4]
        <<",\"native_state_edit_action_service_clear_edit\":"<<runtime->stateEditActionForeground.kinds[5]
        <<",\"native_state_edit_action_service_release\":"<<runtime->stateEditActionForeground.kinds[6]
        <<",\"native_state_scene_toggle_service_calls\":"<<runtime->stateSceneToggleForeground.started
        <<",\"native_state_scene_toggle_service_returns\":"<<runtime->stateSceneToggleForeground.completed
        <<",\"native_state_scene_toggle_service_phases\":"<<runtime->stateSceneToggleForeground.phases
        <<",\"native_state_scene_toggle_service_initial_active\":"<<runtime->stateSceneToggleForeground.initial
        <<",\"native_state_scene_toggle_service_active\":"<<runtime->stateSceneToggleForeground.active()
        <<",\"native_state_scene_toggle_service_components\":"<<runtime->stateSceneToggleForeground.components
        <<",\"native_state_scene_toggle_service_toggle\":"<<runtime->stateSceneToggleForeground.kinds[0]
        <<",\"native_state_scene_toggle_service_commit\":"<<runtime->stateSceneToggleForeground.kinds[1]
        <<",\"native_state_scene_service_calls\":"<<runtime->stateSceneForeground.started
        <<",\"native_state_scene_service_returns\":"<<runtime->stateSceneForeground.completed
        <<",\"native_state_scene_service_phases\":"<<runtime->stateSceneForeground.phases
        <<",\"native_state_scene_service_initial_active\":"<<runtime->stateSceneForeground.initial
        <<",\"native_state_scene_service_active\":"<<runtime->stateSceneForeground.active()
        <<",\"native_state_scene_service_components\":"<<runtime->stateSceneForeground.components
        <<",\"native_state_scene_service_activate\":"<<runtime->stateSceneForeground.kinds[0]
        <<",\"native_state_scene_service_apply\":"<<runtime->stateSceneForeground.kinds[1]
        <<",\"native_state_scene_service_restore\":"<<runtime->stateSceneForeground.kinds[2]
        <<",\"native_state_scene_service_backup\":"<<runtime->stateSceneForeground.kinds[3]
        <<",\"native_state_scene_service_copy\":"<<runtime->stateSceneForeground.kinds[4]
        <<",\"native_state_scene_service_map\":"<<runtime->stateSceneForeground.kinds[5]
        <<",\"native_state_scene_service_load\":"<<runtime->stateSceneForeground.kinds[6]
        <<",\"native_state_scene_service_bank\":"<<runtime->stateSceneForeground.kinds[7]
        <<",\"native_state_scene_service_notify\":"<<runtime->stateSceneForeground.kinds[8]
        <<",\"native_state_scene_service_knob\":"<<runtime->stateSceneForeground.kinds[9]
        <<",\"native_state_scene_service_select_bank\":"<<runtime->stateSceneForeground.kinds[10]
        <<",\"native_parameter_upload_calls\":"<<runtime->parameterUploadForeground.started
        <<",\"native_parameter_upload_returns\":"<<runtime->parameterUploadForeground.completed
        <<",\"native_parameter_upload_phases\":"<<runtime->parameterUploadForeground.phases
        <<",\"native_parameter_upload_initial_active\":"<<runtime->parameterUploadForeground.initial
        <<",\"native_parameter_upload_active\":"<<runtime->parameterUploadForeground.active()
        <<",\"native_parameter_upload_components\":"<<runtime->parameterUploadForeground.components
        <<",\"native_global_update_calls\":"<<runtime->globalUpdateForeground.started
        <<",\"native_global_update_returns\":"<<runtime->globalUpdateForeground.completed
        <<",\"native_global_update_phases\":"<<runtime->globalUpdateForeground.phases
        <<",\"native_global_update_initial_active\":"<<runtime->globalUpdateForeground.initial
        <<",\"native_global_update_active\":"<<runtime->globalUpdateForeground.active()
        <<",\"native_global_update_components\":"<<runtime->globalUpdateForeground.components
        <<",\"native_state_voice_refresh_calls\":"<<runtime->stateVoiceRefreshForeground.started
        <<",\"native_state_voice_refresh_returns\":"<<runtime->stateVoiceRefreshForeground.completed
        <<",\"native_state_voice_refresh_phases\":"<<runtime->stateVoiceRefreshForeground.phases
        <<",\"native_state_voice_refresh_initial_active\":"<<runtime->stateVoiceRefreshForeground.initial
        <<",\"native_state_voice_refresh_active\":"<<runtime->stateVoiceRefreshForeground.active()
        <<",\"native_state_voice_refresh_components\":"<<runtime->stateVoiceRefreshForeground.components
        <<",\"native_state_voice_refresh_all\":"<<runtime->stateVoiceRefreshForeground.kinds[0]
        <<",\"native_state_scene_edit_service_calls\":"<<runtime->stateSceneEditForeground.started
        <<",\"native_state_scene_edit_service_returns\":"<<runtime->stateSceneEditForeground.completed
        <<",\"native_state_scene_edit_service_phases\":"<<runtime->stateSceneEditForeground.phases
        <<",\"native_state_scene_edit_service_initial_active\":"<<runtime->stateSceneEditForeground.initial
        <<",\"native_state_scene_edit_service_active\":"<<runtime->stateSceneEditForeground.active()
        <<",\"native_state_scene_edit_service_components\":"<<runtime->stateSceneEditForeground.components
        <<",\"native_state_scene_edit_service_increment\":"<<runtime->stateSceneEditForeground.kinds[0]
        <<",\"native_state_scene_edit_service_decrement\":"<<runtime->stateSceneEditForeground.kinds[1]
        <<",\"native_state_scene_edit_service_sub\":"<<runtime->stateSceneEditForeground.kinds[2]
        <<",\"native_state_scene_edit_service_adjust\":"<<runtime->stateSceneEditForeground.kinds[3]
        <<",\"native_state_scene_edit_service_record\":"<<runtime->stateSceneEditForeground.kinds[4]
        <<",\"native_state_scene_edit_service_field\":"<<runtime->stateSceneEditForeground.kinds[5]
        <<",\"native_state_scene_edit_service_clamp\":"<<runtime->stateSceneEditForeground.kinds[6]
        <<",\"native_state_scene_edit_service_clear_suffix\":"<<runtime->stateSceneEditForeground.kinds[7]
        <<",\"native_state_scene_edit_service_move_name\":"<<runtime->stateSceneEditForeground.kinds[8]
        <<",\"native_state_scene_edit_service_insert_space\":"<<runtime->stateSceneEditForeground.kinds[9]
        <<",\"native_state_scene_edit_service_adjust_name\":"<<runtime->stateSceneEditForeground.kinds[10]
        <<",\"native_state_scene_edit_service_commit_name\":"<<runtime->stateSceneEditForeground.kinds[11]
        <<",\"native_state_scene_edit_service_read_name\":"<<runtime->stateSceneEditForeground.kinds[12]
        <<",\"native_state_scene_edit_service_decode_name\":"<<runtime->stateSceneEditForeground.kinds[13]
        <<",\"native_state_scene_edit_service_encode_name\":"<<runtime->stateSceneEditForeground.kinds[14]
        <<",\"native_state_scene_edit_service_category\":"<<runtime->stateSceneEditForeground.kinds[15]
        <<",\"native_state_scene_edit_service_begin_name\":"<<runtime->stateSceneEditForeground.kinds[16]
        <<",\"native_state_scene_edit_service_insert_digit\":"<<runtime->stateSceneEditForeground.kinds[17]
        <<",\"native_state_scene_edit_service_insert_raw\":"<<runtime->stateSceneEditForeground.kinds[18]
        <<",\"native_state_scene_edit_service_toggle_case\":"<<runtime->stateSceneEditForeground.kinds[19]
        <<",\"native_interrupt_service_calls\":"<<runtime->nativeInterruptStarts
        <<",\"native_interrupt_service_returns\":"<<runtime->nativeInterruptReturns
        <<",\"native_interrupt_service_phases\":"<<runtime->nativeInterruptPhases
        <<",\"native_interrupt_service_initial\":"<<runtime->nativeInterruptInitial
        <<",\"native_interrupt_service_active\":"<<runtime->activeNativeInterrupts()
        <<",\"native_interrupt_serial_error\":"<<runtime->nativeInterruptKinds[0]
        <<",\"native_interrupt_serial_transmit\":"<<runtime->nativeInterruptKinds[1]
        <<",\"native_interrupt_unused\":"<<runtime->nativeInterruptKinds[2]
        <<",\"native_interrupt_tpu0\":"<<runtime->nativeInterruptKinds[3]
        <<",\"native_interrupt_tpu0_components\":"<<runtime->nativeTpuComponents
        <<",\"native_interrupt_adc\":"<<runtime->nativeInterruptKinds[4]
        <<",\"native_interrupt_adc_components\":"<<runtime->nativeAdcComponents
        <<",\"native_interrupt_serial_receive\":"<<runtime->nativeInterruptKinds[5]
        <<",\"native_interrupt_serial_receive_components\":"<<runtime->nativeSerialReceiveComponents;
#if defined(SILVER_KS_CONTROLLER_PROFILING)
    if(const auto* runtime=board->nativeController();runtime && !runtime->boundaryProfile.empty()){
        report<<",\"native_boundary_profile\":[";bool first=true;
        for(unsigned pc=0;pc<runtime->boundaryProfile.size();++pc)if(runtime->boundaryProfile[pc]){
            if(!first)report<<',';first=false;report<<'['<<pc<<','<<runtime->boundaryProfile[pc]<<']';
        }report<<']';
    }
#endif
    // PROGRAM_MENU_REPORT_BEGIN
    if(programMenuFixture){
        report<<",\"program_menu_event_count\":"<<programMenuInputCount<<",\"program_menu_inputs\":[";
        for(unsigned i=0;i<programMenuInputs.size();++i){if(i)report<<',';const auto&e=programMenuInputs[i];report<<'['<<e.frame<<','<<e.handler<<','<<e.logical<<','<<unsigned(e.pressed)<<']';}
        report<<"],\"program_menu_ui_mutations\":[";
        for(unsigned i=0;i<programMenuWrites.size();++i){if(i)report<<',';const auto&w=programMenuWrites[i];report<<'['<<w.frame<<','<<w.address<<','<<unsigned(w.value)<<']';}
        report<<"],\"program_menu_selected_source\":"<<programMenuSelectedSource<<",\"program_menu_final_ui\":[";
        for(unsigned i=0;i<programMenuFinalUi.size();++i){if(i)report<<',';report<<unsigned(programMenuFinalUi[i]);}report<<']';
    }
    if(programStoreMenuFixture){
        report<<",\"store_menu_final_ui\":[";for(unsigned i=0;i<storeMenuFinalUi.size();++i){if(i)report<<',';report<<unsigned(storeMenuFinalUi[i]);}report<<']';
        report<<",\"store_menu_ui_addresses\":[92036,92037,92039,92048,92050,92062,92063,92090,92641,16772116]";
        report<<",\"store_menu_globals_ram\":[";for(unsigned i=0;i<storeMenuGlobalsRam.size();++i){if(i)report<<',';report<<unsigned(storeMenuGlobalsRam[i]);}report<<']';
        report<<",\"store_menu_globals_flash\":[";for(unsigned i=0;i<storeMenuGlobalsFlash.size();++i){if(i)report<<',';report<<unsigned(storeMenuGlobalsFlash[i]);}report<<']';
        report<<",\"store_menu_uart_output\":[";for(unsigned i=0;i<midiOut.size();++i){if(i)report<<',';report<<unsigned(midiOut[i]);}report<<']';
    }
    if(nativeMode){const auto&w=storeMenuWitness;
        report<<",\"native_store_menu_witness_samples\":"<<w.samples<<",\"native_store_menu_witness_sampled_jobs\":"<<w.sampledJobs<<",\"native_store_menu_witness_returned_jobs\":"<<w.returnedJobs
            <<",\"native_store_menu_save_kind_mask\":"<<w.sampled.kinds<<",\"native_store_menu_save_observed_complete_kind_mask\":"<<w.sampled.observedComplete<<",\"native_store_menu_save_returned_parent_kind_mask\":"<<w.returned.kinds;
        report<<",\"native_store_menu_save_sampled_components_by_kind\":[";for(unsigned i=0;i<w.sampled.components.size();++i){if(i)report<<',';report<<w.sampled.components[i];}report<<']';
        report<<",\"native_store_menu_save_returned_parent_components_by_kind\":[";for(unsigned i=0;i<w.returned.components.size();++i){if(i)report<<',';report<<w.returned.components[i];}report<<']';
        report<<",\"native_store_menu_save_returned_parent_facts_by_kind\":[";for(unsigned i=0;i<w.returned.facts.size();++i){if(i)report<<',';report<<w.returned.facts[i];}report<<']';
        report<<",\"native_store_menu_factory_components\":"<<w.factory<<",\"native_store_menu_factory_returned_parent_components\":"<<w.returnedFactory
            <<",\"native_store_menu_dump_components\":"<<w.dump<<",\"native_store_menu_dump_returned_parent_components\":"<<w.returnedDump
            <<",\"native_store_menu_output_components\":"<<w.output<<",\"native_store_menu_output_returned_parent_components\":"<<w.returnedOutput
            <<",\"native_store_menu_playback_returned_parent_components\":"<<w.returnedPlayback
            <<",\"native_store_menu_dump_returned_parent_payloads\":"<<w.returnedPayloads<<",\"native_store_menu_dump_returned_parent_send_bytes\":"<<w.returnedSendBytes
            <<",\"native_store_menu_dump_returned_parent_multi\":"<<unsigned(w.returnedMulti);
    }
    // PROGRAM_MENU_REPORT_END
    if(panelStore){report<<",\"panel_button_pages\":[";for(unsigned i=0;i<panelEvents;++i){if(i)report<<',';report<<unsigned(panelPages[i]);}report<<']';}
    if(panelIo){
        report<<",\"panel_io_reads\":["<<ioFixture->matrixReads<<','<<ioFixture->pedalReads<<','<<ioFixture->encoderReads<<','<<ioFixture->adcReads<<']'
              <<",\"panel_io_transitions\":["<<ioFixture->buttonTransitions<<','<<ioFixture->pedalTransitions<<','<<ioFixture->encoderTransitions<<','<<ioFixture->adcTransitions<<']'
              <<",\"panel_io_pages\":[";
        for(unsigned i=0;i<ioPageCount;++i){if(i)report<<',';report<<unsigned(ioPages[i]);}report<<']';
    }
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
