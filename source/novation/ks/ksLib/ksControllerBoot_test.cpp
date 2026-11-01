// Explicit retained LEGACY comparison test; not a semantic NativeControllerExecutor proof.
#include "ksControllerTraceComparison_test.h"
#include "ksBoardComparison_test.h"
#include "ksRom.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <cstdint>
#include <stdexcept>
namespace {
uint64_t slice=0;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
std::vector<uint8_t> bytes(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);check(bool(f),"Cannot read fixture");return {std::istreambuf_iterator<char>(f),{}};}
void same(ks::Board& a,ks::Board& b){
    auto& x=static_cast<ks::ControllerCpu&>(ks::test::comparisonCpu(a));auto& y=static_cast<ks::ControllerCpu&>(ks::test::comparisonCpu(b));
    const bool nativeWork=ks::test::legacyRuntime(b)&&ks::test::legacyRuntime(b)->activeNativeWork();
    if(x.cycles!=y.cycles || (!nativeWork&&x.getPC()!=y.getPC())){
        std::cerr<<"reference pc="<<std::hex<<x.getPC()<<" candidate pc="<<y.getPC()<<std::dec<<" cycles "<<x.cycles<<" / "<<y.cycles<<'\n';
        throw std::runtime_error("Reset cycle/PC mismatch");
    }
    check((nativeWork||x.ccr==y.ccr) && x.exr==y.exr,"Reset condition/mask mismatch");
    for(unsigned n=0;n<8;++n)if(!nativeWork&&x.regs[n].er!=y.regs[n].er){std::cerr<<"register "<<n<<" values "<<std::hex<<x.regs[n].er<<" / "<<y.regs[n].er<<std::dec<<'\n';throw std::runtime_error("Reset ABI mismatch");}
    check((nativeWork||(x.lastread==y.lastread && x.lastwrite==y.lastwrite)) && (nativeWork||x.indent==y.indent),"Reset bus/stack history mismatch");
    if(x.pending_irqs!=y.pending_irqs || x.pending_irqs_high!=y.pending_irqs_high || x.interruptMode2!=y.interruptMode2){
        std::cerr<<"pc="<<std::hex<<x.getPC()<<" pending="<<x.pending_irqs<<"/"<<y.pending_irqs<<" high="<<x.pending_irqs_high<<"/"<<y.pending_irqs_high<<" mode="<<x.interruptMode2<<"/"<<y.interruptMode2<<std::dec<<'\n';throw std::runtime_error("Reset IRQ mismatch");}

    check(std::equal(std::begin(x.interruptPriority),std::end(x.interruptPriority),std::begin(y.interruptPriority)),"Reset IRQ priority mismatch");
    check(a.overlay==b.overlay && a.mainEntered==b.mainEntered && a.ready==b.ready,"Reset lifecycle mismatch");
    check(b.h8InterpretedSteps()==0,"Native reset called original interpreter");
    check(ks::test::legacyMidi(b)->noteActions.keySearchCalls.partialEntries==0,"Native reset used the test-only partial-key adapter");
    check(!ks::test::legacyMidi(b)->startupCalls.partialEntries && !ks::test::legacyMidi(b)->voiceActions.selectCalls.partialEntries,"Native reset used partial startup/select recovery");
    check(!ks::test::legacyMidi(b)->voiceActions.routingCalls.partialEntries,"Native reset used partial routing recovery");
    check(!ks::test::legacyMidi(b)->noteActions.partControlCalls.partialEntries && !ks::test::legacyMidi(b)->voiceActions.partControlCalls.partialEntries,"Native reset used partial part-control recovery");
    check(!ks::test::legacyMidi(b)->voiceActions.velocityCurveCalls.partialEntries,"Native reset used partial velocity recovery");
    check(!ks::test::legacyMidi(b)->voiceActions.selectionCalls.partialEntries,"Native reset used partial selection/gate recovery");
    check(!ks::test::legacyMidi(b)->voiceActions.populationCalls.partialEntries,"Native reset used partial population recovery");
    check(ks::test::legacyMidi(b)->bootSetupCalls.partialEntries==0,"Native reset used partial bootstrap setup recovery");
    check(ks::test::legacyMidi(b)->bootStreamCalls.partialEntries==0,"Native reset used partial bootstrap-stream recovery");
    check(ks::test::legacyMidi(b)->faultCalls.partialEntries==0,"Native reset used partial fault recovery");
    check(ks::test::legacyMidi(b)->voiceActions.addressCalls.partialEntries==0,"Native reset used partial DSP-address recovery");
    check(ks::test::legacyMidi(b)->voiceActions.wordCalls.partialEntries==0,"Native reset used partial DSP-word recovery");
    check(ks::test::legacyMidi(b)->parameterScaleCalls.partialEntries==0,"Native reset used partial coefficient recovery");
    check(!ks::test::legacyMidi(b)->panelMatrixCalls.partialEntries,"Native reset used partial panel-matrix recovery");
    check(!ks::test::legacyMidi(b)->pedalSenseCalls.partialEntries,"Native reset used partial pedal recovery");
    check(!ks::test::legacyMidi(b)->panelEncoderCalls.partialEntries,"Native reset used partial encoder recovery");
    check(!ks::test::legacyMidi(b)->panelAdcSamplesCalls.partialEntries,"Native reset used partial ADC-samples recovery");
    check(!ks::test::legacyMidi(b)->panelAdcScaleCalls.partialEntries,"Native reset used partial ADC-scale recovery");
    check(!ks::test::legacyMidi(b)->panelAdcCalibrationCalls.partialEntries,"Native reset used partial ADC-calibration recovery");
    check(!ks::test::legacyMidi(b)->panelAdcIrqCalls.partialEntries,"Native reset used partial ADC-IRQ recovery");
    check(!ks::test::legacyMidi(b)->panelAdcScanCalls.partialEntries,"Native reset used partial ADC-scan recovery");
    check(!ks::test::legacyMidi(b)->internalMidiQueueCalls.partialEntries,"Native reset used partial InternalMidiQueue recovery");
    check(!ks::test::legacyMidi(b)->midiOutputCalls.partialEntries,"Native reset used partial MidiOutput recovery");
    check(!ks::test::legacyMidi(b)->internalMidiServiceCalls.partialEntries,"Native reset used partial InternalMidiService recovery");
    check(!ks::test::legacyMidi(b)->stateForegroundCalls.partialEntries,"Native reset used partial StateForeground recovery");
    check(!ks::test::legacyMidi(b)->stateButtonDispatchCalls.partialEntries,"Native reset used partial StateButtonDispatch recovery");
    check(!ks::test::legacyMidi(b)->panelAnalogCalls.partialEntries,"Native reset used partial PanelAnalog recovery");
    check(!ks::test::legacyMidi(b)->clockSyncCalls.partialEntries,"Native reset used partial ClockSync recovery");
    check(!ks::test::legacyMidi(b)->clockPulseCalls.partialEntries,"Native reset used partial ClockPulse recovery");
    check(!ks::test::legacyMidi(b)->clockDisplayCalls.partialEntries,"Native reset used partial ClockDisplay recovery");
    check(!ks::test::legacyMidi(b)->stateScreenCalls.partialEntries,"Native reset used partial StateScreen recovery");
    check(!ks::test::legacyMidi(b)->panelSettingsCalls.partialEntries,"Native reset used partial PanelSettings recovery");
    check(!ks::test::legacyMidi(b)->stateEditContextCalls.partialEntries,"Native reset used partial StateEditContext recovery");
    check(!ks::test::legacyMidi(b)->stateScreenEditCalls.partialEntries,"Native reset used partial StateScreenEdit recovery");
    check(!ks::test::legacyMidi(b)->stateMenuFormatCalls.partialEntries,"Native reset used partial StateMenuFormat recovery");
    check(!ks::test::legacyMidi(b)->systemServicesCalls.partialEntries,"Native reset used partial SystemServices recovery");
    check(!ks::test::legacyMidi(b)->keyboardEventsCalls.partialEntries,"Native reset used partial KeyboardEvents recovery");
    check(!ks::test::legacyMidi(b)->statePartSelectionCalls.partialEntries,"Native reset used partial StatePartSelection recovery");
    check(!ks::test::legacyMidi(b)->stateMenuActionsCalls.partialEntries,"Native reset used partial StateMenuActions recovery");
    check(!ks::test::legacyMidi(b)->keyboardScanCalls.partialEntries,"Native reset used partial KeyboardScan recovery");
    check(!ks::test::legacyMidi(b)->diagnosticCodecCalls.partialEntries,"Native reset used partial DiagnosticCodec recovery");
    check(!ks::test::legacyMidi(b)->diagnosticTransferCalls.partialEntries,"Native reset used partial DiagnosticTransfer recovery");
    check(!ks::test::legacyMidi(b)->memoryDiagnosticsCalls.partialEntries,"Native reset used partial MemoryDiagnostics recovery");
    check(!ks::test::legacyMidi(b)->startupBannerCalls.partialEntries,"Native reset used partial StartupBanner recovery");
    check(!ks::test::legacyMidi(b)->stateScreenActionsCalls.partialEntries,"Native reset used partial StateScreenActions recovery");
    check(!ks::test::legacyMidi(b)->statePanelModesCalls.partialEntries,"Native reset used partial StatePanelModes recovery");
    check(!ks::test::legacyMidi(b)->stateFunctionKeysCalls.partialEntries,"Native reset used partial StateFunctionKeys recovery");
    check(!ks::test::legacyMidi(b)->stateEditWorkflowCalls.partialEntries,"Native reset used partial StateEditWorkflow recovery");
    check(!ks::test::legacyMidi(b)->loaderInputsCalls.partialEntries,"Native reset used partial LoaderInputs recovery");
    check(!ks::test::legacyMidi(b)->loaderCodecCalls.partialEntries,"Native reset used partial LoaderCodec recovery");
    check(!ks::test::legacyMidi(b)->loaderPacketsCalls.partialEntries,"Native reset used partial LoaderPackets recovery");
    check(!ks::test::legacyMidi(b)->loaderProgramCalls.partialEntries,"Native reset used partial LoaderProgram recovery");
    check(!ks::test::legacyMidi(b)->loaderSessionCalls.partialEntries,"Native reset used partial LoaderSession recovery");
    check(!ks::test::legacyMidi(b)->stateValueBoundsCalls.partialEntries,"Native reset used partial StateValueBounds recovery");
    check(!ks::test::legacyMidi(b)->stateEditLabelCalls.partialEntries,"Native reset used partial StateEditLabel recovery");
    check(!ks::test::legacyMidi(b)->stateFieldActionsCalls.partialEntries,"Native reset used partial StateFieldActions recovery");
    check(!ks::test::legacyMidi(b)->stateBankNavigationCalls.partialEntries,"Native reset used partial StateBankNavigation recovery");
    check(!ks::test::legacyMidi(b)->stateStatusDisplayCalls.partialEntries,"Native reset used partial StateStatusDisplay recovery");
    check(!ks::test::legacyMidi(b)->stateStoredValueCalls.partialEntries,"Native reset used partial StateStoredValue recovery");
    check(!ks::test::legacyMidi(b)->stateBrowseWorkflowCalls.partialEntries,"Native reset used partial StateBrowseWorkflow recovery");
    check(!ks::test::legacyMidi(b)->stateProgramBrowseCalls.partialEntries,"Native reset used partial StateProgramBrowse recovery");
    check(!ks::test::legacyMidi(b)->stateProgramDisplayCalls.partialEntries,"Native reset used partial StateProgramDisplay recovery");
    check(!ks::test::legacyMidi(b)->stateProgramEventCalls.partialEntries,"Native reset used partial StateProgramEvent recovery");
    for(unsigned n=0xfffe00;n<=0xffffff;++n)check(a.read(n)==b.read(n),"Reset IO mismatch");
    check(a.waveReads==b.waveReads && a.hostWrites[0]==b.hostWrites[0] && a.hostWrites[1]==b.hostWrites[1],"Reset hardware counters mismatch");
}
}
int main(int argc,char** argv){try{
    if(argc!=5 && argc!=6){std::cout<<"SKIP: KS OS, Flash, Wave and fresh private output directory, optional single|irregular|multi|mapped variant required\n";return 77;}
    const std::filesystem::path out(argv[4]);check(!std::filesystem::exists(out),"Refusing reused output directory");std::filesystem::create_directories(out);
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");auto flash=bytes(argv[2]),wave=bytes(argv[3]);
    const std::string variant=argc==6?argv[5]:"single";
    check(variant=="single" || variant=="irregular" || variant=="multi" || variant=="mapped","Unknown boot variant");
    if(variant=="multi"){flash[0x1ff01]=0;flash[0x1ff04]=1;}
    if(variant=="mapped"){wave.front()=0x12;wave[1]=0x34;wave[wave.size()-2]=0xab;wave.back()=0xcd;}
    auto a=ks::test::makeLegacyBoard(os.data(),wave,flash,true,false);
    auto b=ks::test::makeLegacyBoard(os.data(),wave,flash,true,true);
    same(*a,*b);a->startControllerTrace();b->startControllerTrace();
    // Match production 128-cycle coordination from reset; the oracle owns
    // its original timer/IRQ/core path and independent DSP instances.
    while(!a->ready){
        check(a->controllerCycles()<ks::Board::ControllerClockHz*10ull,"Boot timed out");
        constexpr unsigned budgets[]{128,127,129,97,193,255,1,63};
        const auto budget=variant=="irregular"?budgets[slice%8]:128;
        a->advance(budget);b->advance(budget);++slice;same(*a,*b);a->discardAudio();b->discardAudio();
        if(!(slice%100000))std::cout<<"slices="<<slice<<" pc="<<std::hex<<ks::test::comparisonCpu(*a).getPC()<<std::dec<<" cycle="<<a->controllerCycles()<<std::endl;
    }
    check(a->boot() && b->boot(),"Warm initialization failed");same(*a,*b);
    check(ks::test::legacyMidi(*b)->startupCalls.startupStarted==1 && ks::test::legacyMidi(*b)->startupCalls.startupCompleted==1 && ks::test::legacyMidi(*b)->startupCalls.resetStarted==1 && ks::test::legacyMidi(*b)->startupCalls.resetCompleted==1,"Cold start did not complete native outer startup/reset");
    check(ks::test::legacyMidi(*b)->bootSetupCalls.started==2 && ks::test::legacyMidi(*b)->bootSetupCalls.completed==2,"Cold startup did not complete both native DSP setup jobs");
    check(ks::test::legacyMidi(*b)->bootStreamCalls.started==2 && ks::test::legacyMidi(*b)->bootStreamCalls.completed==2,"Cold startup did not complete both native DSP firmware streams");
    check(ks::test::legacyMidi(*b)->panelAdcCalibrationCalls.started>0&&ks::test::legacyMidi(*b)->panelAdcCalibrationCalls.completed>0,"Cold start did not exercise native ADC calibration");
    check(ks::test::legacyRuntime(*b)->panelSettingsForeground.kinds[unsigned(ks::PanelSettingsService::Kind::Warmup)]>0&&(ks::test::legacyRuntime(*b)->panelSettingsForeground.components&(1u<<unsigned(ks::PanelSettingsService::Component::AdcStart)))&&ks::test::legacyRuntime(*b)->nativeInterruptKinds[4]>0,"Cold start did not exercise CPU-free ADC warmup/start/IRQ");
    // Native IRQs no longer materialize temporary CPU frames. Only the
    // firmware's reserved scratch stack may differ; all other 16 MiB bytes
    // remain part of the independent cold-boot comparison.
    const auto sameResources=[&]{
        const auto*x=ks::test::comparisonCpu(*a).memory;const auto*y=ks::test::comparisonCpu(*b).memory;
        return std::equal(x,x+0xfff000,y)&&std::equal(x+0xfffc00,x+(1<<24),y+0xfffc00);
    };
    check(sameResources(),"Complete boot resource memory mismatch");
    if(variant=="multi")check(a->state().performance && b->state().performance,"Performance startup not exercised");
    if(variant=="mapped")check(!a->zeroBank() && !b->zeroBank(),"Mapped wave bank adaptation not disabled");
    const uint8_t on[]{0x90,60,100};check(a->enqueueMidi(on,3)&&b->enqueueMidi(on,3),"MIDI rejected");
    int64_t peak=0;
    // Test-only retained PCM, allocated before rendering and saved after it.
    // This does not run in an audio callback or change controller/DSP timing.
    std::vector<int32_t> referencePcm,nativePcm;
    referencePcm.reserve(24000*24);nativePcm.reserve(24000*24);
    for(unsigned n=0;n<24000;++n){
        if(n==8000){const uint8_t off[]{0x80,60,0};check(a->enqueueMidi(off,3)&&b->enqueueMidi(off,3),"Release rejected");}
        const auto x=a->renderSample(),y=b->renderSample();check(x==y,"Post-reset audio mismatch");same(*a,*b);
        static_assert(std::tuple_size<ks::Board::AudioFrame>::value==24,"Cold PCM channel count");
        referencePcm.insert(referencePcm.end(),x.begin(),x.end());nativePcm.insert(nativePcm.end(),y.begin(),y.end());
        for(unsigned c=0;c<4;++c)peak=std::max(peak,std::abs(int64_t(x[c])));
    }
    check(peak>1000,"Boot oracle not admitted as audible");
    const auto savePcm=[&](const char* name,const std::vector<int32_t>& samples){
        check(samples.size()==24000*24,"Cold PCM frame bounds");std::ofstream file(out/name,std::ios::binary);
        check(bool(file),"Cannot save cold PCM");for(const auto value:samples)for(unsigned byte=0;byte<4;++byte)file.put(char(uint32_t(value)>>(8*byte)));
        file.close();check(bool(file),"Cold PCM write failed");
    };
    savePcm("h8.pcm",referencePcm);savePcm("cpp.pcm",nativePcm);
    const auto x=a->finishControllerTrace(),y=b->finishControllerTrace();x->validate();y->validate();
    x->save((out/"h8.kst").string());y->save((out/"cpp.kst").string());
    // Keep both raw tapes and checkpoints. Removed internal CPU locations
    // become the verified native graph caller; resource transfers stay exact.
    check(x->checkpoint==y->checkpoint&&x->beginCycle==y->beginCycle&&x->endCycle==y->endCycle&&x->events.size()==y->events.size()&&x->slices.size()==y->slices.size(),"Reset tape checkpoint/bounds mismatch");
    uint64_t nativeCpuLocationChanges=0,unexpectedCpuLocationChanges=0;bool cpuLocationsAcceptable=true;
    for(size_t n=0;n<x->events.size();++n){const auto&p=x->events[n];const auto&q=y->events[n];check(p.cycle==q.cycle&&p.dsp==q.dsp&&p.offset==q.offset&&p.value==q.value&&p.flags==q.flags,"Reset DSP transfer mismatch");
        if(p.pc!=q.pc){if(!ks::tests::nativeDspTraceLocationMatches(p.pc,q.pc)){cpuLocationsAcceptable=false;++unexpectedCpuLocationChanges;}++nativeCpuLocationChanges;}}
    for(size_t n=0;n<x->slices.size();++n){const auto&p=x->slices[n];const auto&q=y->slices[n];check(p.begin==q.begin&&p.end==q.end&&p.first==q.first&&p.count==q.count,"Reset DSP slice mismatch");}
    std::cout<<"Reset diagnostic CPU locations changed="<<nativeCpuLocationChanges<<" (both raw tapes retained)\n";
    check(a->saveUserFlash()==b->saveUserFlash(),"Reset Flash mismatch");
    check(ks::stateMessage(a->state())==ks::stateMessage(b->state()),"Reset musical state mismatch");
    check(sameResources(),"Post-reset resource memory mismatch");
    check(a->serialDepth()==b->serialDepth() && a->serialUnderruns==b->serialUnderruns && a->serialOverflows==b->serialOverflows,"Reset serial state mismatch");
    uint8_t v=0,w=0;while(a->popMidiOut(v))check(b->popMidiOut(w)&&v==w,"Reset MIDI output mismatch");check(!b->popMidiOut(w),"Trailing reset MIDI output");
    // Preserve semantic evidence even when the final diagnostic-PC check fails.
    // Every original non-PC/resource/state assertion above remains strict.
    const auto& factory=ks::test::legacyRuntime(*b)->stateFactoryForeground;
    std::ofstream gates(out/"cold-semantic-gates.json");check(bool(gates),"Cannot save cold semantic evidence");
    gates<<"{\"semantic_gates_complete\":true,\"variant\":\""<<variant<<"\",\"frames\":24000,\"channels\":24,\"encoding\":\"int32le\",\"peak\":"<<peak
         <<",\"events\":"<<x->events.size()<<",\"slices\":"<<x->slices.size()<<",\"controller_cycles\":"<<b->controllerCycles()
         <<",\"checkpoint_exact\":true,\"every_pcm_sample_exact\":true,\"every_nonPC_event_and_slice_exact\":true"
         <<",\"strict_pc_acceptable\":"<<(cpuLocationsAcceptable?"true":"false")<<",\"unexpected_pc_events\":"<<unexpectedCpuLocationChanges
         <<",\"native_clock_sync_service_calls\":"<<ks::test::legacyRuntime(*b)->clockSyncForeground.started
         <<",\"native_clock_sync_service_returns\":"<<ks::test::legacyRuntime(*b)->clockSyncForeground.completed
         <<",\"native_clock_sync_service_phases\":"<<ks::test::legacyRuntime(*b)->clockSyncForeground.phases
         <<",\"native_clock_sync_service_initial_active\":"<<ks::test::legacyRuntime(*b)->clockSyncForeground.initial
         <<",\"native_clock_sync_service_active\":"<<ks::test::legacyRuntime(*b)->clockSyncForeground.active()
         <<",\"native_clock_sync_service_components\":"<<ks::test::legacyRuntime(*b)->clockSyncForeground.components
         <<",\"native_clock_sync_service_update\":"<<ks::test::legacyRuntime(*b)->clockSyncForeground.kinds[0]
         <<",\"native_state_deferred_parts_service_calls\":"<<ks::test::legacyRuntime(*b)->stateDeferredPartsForeground.started
         <<",\"native_state_deferred_parts_service_returns\":"<<ks::test::legacyRuntime(*b)->stateDeferredPartsForeground.completed
         <<",\"native_state_deferred_parts_service_phases\":"<<ks::test::legacyRuntime(*b)->stateDeferredPartsForeground.phases
         <<",\"native_state_deferred_parts_service_initial_active\":"<<ks::test::legacyRuntime(*b)->stateDeferredPartsForeground.initial
         <<",\"native_state_deferred_parts_service_active\":"<<ks::test::legacyRuntime(*b)->stateDeferredPartsForeground.active()
         <<",\"native_state_deferred_parts_service_components\":"<<ks::test::legacyRuntime(*b)->stateDeferredPartsForeground.components
         <<",\"native_state_deferred_parts_service_deferred\":"<<ks::test::legacyRuntime(*b)->stateDeferredPartsForeground.kinds[0]
         <<",\"native_state_deferred_parts_service_processed\":"<<ks::test::legacyRuntime(*b)->stateDeferredPartsForeground.processed
         <<",\"native_state_deferred_parts_service_voice_uploads\":"<<ks::test::legacyRuntime(*b)->stateDeferredPartsForeground.voiceUploads
         <<",\"native_state_deferred_parts_service_part_mask\":"<<ks::test::legacyRuntime(*b)->stateDeferredPartsForeground.partMask
         <<",\"native_state_refresh_service_calls\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.started
         <<",\"native_state_refresh_service_returns\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.completed
         <<",\"native_state_refresh_service_phases\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.phases
         <<",\"native_state_refresh_service_initial_active\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.initial
         <<",\"native_state_refresh_service_active\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.active()
         <<",\"native_state_refresh_service_components\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.components
         <<",\"native_state_refresh_service_refresh\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.kinds[0]
         <<",\"native_state_refresh_service_returned_packet\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.returnedPacket
         <<",\"native_state_refresh_service_returned_panel\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.returnedPanel
         <<",\"native_state_refresh_service_returned_deferred\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.returnedDeferred
         <<",\"native_state_refresh_service_returned_clock\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.returnedClock
         <<",\"native_state_refresh_service_processed_parts\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.processedParts
         <<",\"native_state_refresh_service_returned_coefficient\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.returnedCoefficient
         <<",\"native_state_refresh_service_returned_period_address\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.returnedPeriodAddress
         <<",\"native_state_refresh_service_returned_period_value\":"<<ks::test::legacyRuntime(*b)->stateRefreshForeground.returnedPeriodValue
         <<",\"native_panel_pedal_output_service_calls\":"<<ks::test::legacyRuntime(*b)->panelPedalOutputForeground.started
         <<",\"native_panel_pedal_output_service_returns\":"<<ks::test::legacyRuntime(*b)->panelPedalOutputForeground.completed
         <<",\"native_panel_pedal_output_service_phases\":"<<ks::test::legacyRuntime(*b)->panelPedalOutputForeground.phases
         <<",\"native_panel_pedal_output_service_initial_active\":"<<ks::test::legacyRuntime(*b)->panelPedalOutputForeground.initial
         <<",\"native_panel_pedal_output_service_active\":"<<ks::test::legacyRuntime(*b)->panelPedalOutputForeground.active()
         <<",\"native_panel_pedal_output_service_components\":"<<ks::test::legacyRuntime(*b)->panelPedalOutputForeground.components
         <<",\"native_panel_pedal_output_service_pedal\":"<<ks::test::legacyRuntime(*b)->panelPedalOutputForeground.kinds[0]
         <<",\"native_panel_pedal_output_service_byte\":"<<ks::test::legacyRuntime(*b)->panelPedalOutputForeground.kinds[1]
         <<",\"native_state_encoder_service_calls\":"<<ks::test::legacyRuntime(*b)->stateEncoderForeground.started
         <<",\"native_state_encoder_service_returns\":"<<ks::test::legacyRuntime(*b)->stateEncoderForeground.completed
         <<",\"native_state_encoder_service_phases\":"<<ks::test::legacyRuntime(*b)->stateEncoderForeground.phases
         <<",\"native_state_encoder_service_initial_active\":"<<ks::test::legacyRuntime(*b)->stateEncoderForeground.initial
         <<",\"native_state_encoder_service_active\":"<<ks::test::legacyRuntime(*b)->stateEncoderForeground.active()
         <<",\"native_state_encoder_service_components\":"<<ks::test::legacyRuntime(*b)->stateEncoderForeground.components
         <<",\"native_state_encoder_service_encoder\":"<<ks::test::legacyRuntime(*b)->stateEncoderForeground.kinds[0]
         <<",\"native_panel_masks_service_calls\":"<<ks::test::legacyRuntime(*b)->panelMasksForeground.started
         <<",\"native_panel_masks_service_returns\":"<<ks::test::legacyRuntime(*b)->panelMasksForeground.completed
         <<",\"native_panel_masks_service_phases\":"<<ks::test::legacyRuntime(*b)->panelMasksForeground.phases
         <<",\"native_panel_masks_service_initial_active\":"<<ks::test::legacyRuntime(*b)->panelMasksForeground.initial
         <<",\"native_panel_masks_service_active\":"<<ks::test::legacyRuntime(*b)->panelMasksForeground.active()
         <<",\"native_voice_feedback_service_calls\":"<<ks::test::legacyRuntime(*b)->voiceFeedbackForeground.started
         <<",\"native_voice_feedback_service_returns\":"<<ks::test::legacyRuntime(*b)->voiceFeedbackForeground.completed
         <<",\"native_voice_feedback_service_phases\":"<<ks::test::legacyRuntime(*b)->voiceFeedbackForeground.phases
         <<",\"native_voice_feedback_service_initial_active\":"<<ks::test::legacyRuntime(*b)->voiceFeedbackForeground.initial
         <<",\"native_voice_feedback_service_active\":"<<ks::test::legacyRuntime(*b)->voiceFeedbackForeground.active()
         <<",\"native_part_feedback_service_calls\":"<<ks::test::legacyRuntime(*b)->partFeedbackForeground.started
         <<",\"native_part_feedback_service_returns\":"<<ks::test::legacyRuntime(*b)->partFeedbackForeground.completed
         <<",\"native_part_feedback_service_phases\":"<<ks::test::legacyRuntime(*b)->partFeedbackForeground.phases
         <<",\"native_part_feedback_service_initial_active\":"<<ks::test::legacyRuntime(*b)->partFeedbackForeground.initial
         <<",\"native_part_feedback_service_active\":"<<ks::test::legacyRuntime(*b)->partFeedbackForeground.active()
         <<",\"native_periodic_service_calls\":"<<ks::test::legacyRuntime(*b)->periodicForeground.started
         <<",\"native_periodic_service_returns\":"<<ks::test::legacyRuntime(*b)->periodicForeground.completed
         <<",\"native_periodic_service_phases\":"<<ks::test::legacyRuntime(*b)->periodicForeground.phases
         <<",\"native_periodic_service_initial_active\":"<<ks::test::legacyRuntime(*b)->periodicForeground.initial
         <<",\"native_periodic_service_active\":"<<ks::test::legacyRuntime(*b)->periodicForeground.active()
         <<",\"native_state_channel_service_calls\":"<<ks::test::legacyRuntime(*b)->stateChannelForeground.started
         <<",\"native_state_channel_service_returns\":"<<ks::test::legacyRuntime(*b)->stateChannelForeground.completed
         <<",\"native_state_channel_service_phases\":"<<ks::test::legacyRuntime(*b)->stateChannelForeground.phases
         <<",\"native_state_channel_service_initial_active\":"<<ks::test::legacyRuntime(*b)->stateChannelForeground.initial
         <<",\"native_state_channel_service_active\":"<<ks::test::legacyRuntime(*b)->stateChannelForeground.active()
         <<",\"native_midi_packet_service_calls\":"<<ks::test::legacyRuntime(*b)->midiPacketForeground.started
         <<",\"native_midi_packet_service_returns\":"<<ks::test::legacyRuntime(*b)->midiPacketForeground.completed
         <<",\"native_midi_packet_service_phases\":"<<ks::test::legacyRuntime(*b)->midiPacketForeground.phases
         <<",\"native_midi_packet_service_initial_active\":"<<ks::test::legacyRuntime(*b)->midiPacketForeground.initial
         <<",\"native_midi_packet_service_active\":"<<ks::test::legacyRuntime(*b)->midiPacketForeground.active()
         <<",\"native_native_key_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeKeyForeground.started
         <<",\"native_native_key_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeKeyForeground.completed
         <<",\"native_native_key_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeKeyForeground.phases
         <<",\"native_native_key_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeKeyForeground.initial
         <<",\"native_native_key_service_active\":"<<ks::test::legacyRuntime(*b)->nativeKeyForeground.active()
         <<",\"native_native_arp_reset_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeArpResetForeground.started
         <<",\"native_native_arp_reset_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeArpResetForeground.completed
         <<",\"native_native_arp_reset_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeArpResetForeground.phases
         <<",\"native_native_arp_reset_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeArpResetForeground.initial
         <<",\"native_native_arp_reset_service_active\":"<<ks::test::legacyRuntime(*b)->nativeArpResetForeground.active()
         <<",\"native_native_voice_release_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeVoiceReleaseForeground.started
         <<",\"native_native_voice_release_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeVoiceReleaseForeground.completed
         <<",\"native_native_voice_release_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeVoiceReleaseForeground.phases
         <<",\"native_native_voice_release_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeVoiceReleaseForeground.initial
         <<",\"native_native_voice_release_service_active\":"<<ks::test::legacyRuntime(*b)->nativeVoiceReleaseForeground.active()
         <<",\"native_native_voice_allocation_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeVoiceAllocationForeground.started
         <<",\"native_native_voice_allocation_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeVoiceAllocationForeground.completed
         <<",\"native_native_voice_allocation_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeVoiceAllocationForeground.phases
         <<",\"native_native_voice_allocation_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeVoiceAllocationForeground.initial
         <<",\"native_native_voice_allocation_service_active\":"<<ks::test::legacyRuntime(*b)->nativeVoiceAllocationForeground.active()
         <<",\"native_native_voice_start_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeVoiceStartForeground.started
         <<",\"native_native_voice_start_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeVoiceStartForeground.completed
         <<",\"native_native_voice_start_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeVoiceStartForeground.phases
         <<",\"native_native_voice_start_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeVoiceStartForeground.initial
         <<",\"native_native_voice_start_service_active\":"<<ks::test::legacyRuntime(*b)->nativeVoiceStartForeground.active()
         <<",\"native_native_voice_modulation_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeVoiceModulationForeground.started
         <<",\"native_native_voice_modulation_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeVoiceModulationForeground.completed
         <<",\"native_native_voice_modulation_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeVoiceModulationForeground.phases
         <<",\"native_native_voice_modulation_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeVoiceModulationForeground.initial
         <<",\"native_native_voice_modulation_service_active\":"<<ks::test::legacyRuntime(*b)->nativeVoiceModulationForeground.active()
         <<",\"native_native_voice_sweep_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeVoiceSweepForeground.started
         <<",\"native_native_voice_sweep_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeVoiceSweepForeground.completed
         <<",\"native_native_voice_sweep_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeVoiceSweepForeground.phases
         <<",\"native_native_voice_sweep_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeVoiceSweepForeground.initial
         <<",\"native_native_voice_sweep_service_active\":"<<ks::test::legacyRuntime(*b)->nativeVoiceSweepForeground.active()
         <<",\"native_internal_key_output_service_calls\":"<<ks::test::legacyRuntime(*b)->internalKeyOutputForeground.started
         <<",\"native_internal_key_output_service_returns\":"<<ks::test::legacyRuntime(*b)->internalKeyOutputForeground.completed
         <<",\"native_internal_key_output_service_phases\":"<<ks::test::legacyRuntime(*b)->internalKeyOutputForeground.phases
         <<",\"native_internal_key_output_service_initial_active\":"<<ks::test::legacyRuntime(*b)->internalKeyOutputForeground.initial
         <<",\"native_internal_key_output_service_active\":"<<ks::test::legacyRuntime(*b)->internalKeyOutputForeground.active()
         <<",\"native_native_note_state_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeNoteStateForeground.started
         <<",\"native_native_note_state_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeNoteStateForeground.completed
         <<",\"native_native_note_state_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeNoteStateForeground.phases
         <<",\"native_native_note_state_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeNoteStateForeground.initial
         <<",\"native_native_note_state_service_active\":"<<ks::test::legacyRuntime(*b)->nativeNoteStateForeground.active()
         <<",\"native_native_note_program_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeNoteProgramForeground.started
         <<",\"native_native_note_program_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeNoteProgramForeground.completed
         <<",\"native_native_note_program_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeNoteProgramForeground.phases
         <<",\"native_native_note_program_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeNoteProgramForeground.initial
         <<",\"native_native_note_program_service_active\":"<<ks::test::legacyRuntime(*b)->nativeNoteProgramForeground.active()
         <<",\"native_native_parameter_support_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeParameterSupportForeground.started
         <<",\"native_native_parameter_support_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeParameterSupportForeground.completed
         <<",\"native_native_parameter_support_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeParameterSupportForeground.phases
         <<",\"native_native_parameter_support_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeParameterSupportForeground.initial
         <<",\"native_native_parameter_support_service_active\":"<<ks::test::legacyRuntime(*b)->nativeParameterSupportForeground.active()
         <<",\"native_native_parameter_edit_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeParameterEditForeground.started
         <<",\"native_native_parameter_edit_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeParameterEditForeground.completed
         <<",\"native_native_parameter_edit_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeParameterEditForeground.phases
         <<",\"native_native_parameter_edit_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeParameterEditForeground.initial
         <<",\"native_native_parameter_edit_service_active\":"<<ks::test::legacyRuntime(*b)->nativeParameterEditForeground.active()
         <<",\"native_panel_monitor_service_calls\":"<<ks::test::legacyRuntime(*b)->panelMonitorForeground.started
         <<",\"native_panel_monitor_service_returns\":"<<ks::test::legacyRuntime(*b)->panelMonitorForeground.completed
         <<",\"native_panel_monitor_service_phases\":"<<ks::test::legacyRuntime(*b)->panelMonitorForeground.phases
         <<",\"native_panel_monitor_service_initial_active\":"<<ks::test::legacyRuntime(*b)->panelMonitorForeground.initial
         <<",\"native_panel_monitor_service_active\":"<<ks::test::legacyRuntime(*b)->panelMonitorForeground.active()
         <<",\"native_state_status_service_calls\":"<<ks::test::legacyRuntime(*b)->stateStatusForeground.started
         <<",\"native_state_status_service_returns\":"<<ks::test::legacyRuntime(*b)->stateStatusForeground.completed
         <<",\"native_state_status_service_phases\":"<<ks::test::legacyRuntime(*b)->stateStatusForeground.phases
         <<",\"native_state_status_service_initial_active\":"<<ks::test::legacyRuntime(*b)->stateStatusForeground.initial
         <<",\"native_state_status_service_active\":"<<ks::test::legacyRuntime(*b)->stateStatusForeground.active()
         <<",\"native_diagnostic_codec_service_calls\":"<<ks::test::legacyRuntime(*b)->diagnosticCodecForeground.started
         <<",\"native_diagnostic_codec_service_returns\":"<<ks::test::legacyRuntime(*b)->diagnosticCodecForeground.completed
         <<",\"native_diagnostic_codec_service_phases\":"<<ks::test::legacyRuntime(*b)->diagnosticCodecForeground.phases
         <<",\"native_diagnostic_codec_service_initial_active\":"<<ks::test::legacyRuntime(*b)->diagnosticCodecForeground.initial
         <<",\"native_diagnostic_codec_service_active\":"<<ks::test::legacyRuntime(*b)->diagnosticCodecForeground.active()
         <<",\"native_diagnostic_transfer_service_calls\":"<<ks::test::legacyRuntime(*b)->diagnosticTransferForeground.started
         <<",\"native_diagnostic_transfer_service_returns\":"<<ks::test::legacyRuntime(*b)->diagnosticTransferForeground.completed
         <<",\"native_diagnostic_transfer_service_phases\":"<<ks::test::legacyRuntime(*b)->diagnosticTransferForeground.phases
         <<",\"native_diagnostic_transfer_service_initial_active\":"<<ks::test::legacyRuntime(*b)->diagnosticTransferForeground.initial
         <<",\"native_diagnostic_transfer_service_active\":"<<ks::test::legacyRuntime(*b)->diagnosticTransferForeground.active()
         <<",\"native_midi_file_time_service_calls\":"<<ks::test::legacyRuntime(*b)->midiFileTimeForeground.started
         <<",\"native_midi_file_time_service_returns\":"<<ks::test::legacyRuntime(*b)->midiFileTimeForeground.completed
         <<",\"native_midi_file_time_service_phases\":"<<ks::test::legacyRuntime(*b)->midiFileTimeForeground.phases
         <<",\"native_midi_file_time_service_initial_active\":"<<ks::test::legacyRuntime(*b)->midiFileTimeForeground.initial
         <<",\"native_midi_file_time_service_active\":"<<ks::test::legacyRuntime(*b)->midiFileTimeForeground.active()
         <<",\"native_native_pedal_control_service_calls\":"<<ks::test::legacyRuntime(*b)->nativePedalControlForeground.started
         <<",\"native_native_pedal_control_service_returns\":"<<ks::test::legacyRuntime(*b)->nativePedalControlForeground.completed
         <<",\"native_native_pedal_control_service_phases\":"<<ks::test::legacyRuntime(*b)->nativePedalControlForeground.phases
         <<",\"native_native_pedal_control_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativePedalControlForeground.initial
         <<",\"native_native_pedal_control_service_active\":"<<ks::test::legacyRuntime(*b)->nativePedalControlForeground.active()
         <<",\"native_native_midi_control_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeMidiControlForeground.started
         <<",\"native_native_midi_control_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeMidiControlForeground.completed
         <<",\"native_native_midi_control_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeMidiControlForeground.phases
         <<",\"native_native_midi_control_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeMidiControlForeground.initial
         <<",\"native_native_midi_control_service_active\":"<<ks::test::legacyRuntime(*b)->nativeMidiControlForeground.active()
         <<",\"native_native_part_note_service_calls\":"<<ks::test::legacyRuntime(*b)->nativePartNoteForeground.started
         <<",\"native_native_part_note_service_returns\":"<<ks::test::legacyRuntime(*b)->nativePartNoteForeground.completed
         <<",\"native_native_part_note_service_phases\":"<<ks::test::legacyRuntime(*b)->nativePartNoteForeground.phases
         <<",\"native_native_part_note_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativePartNoteForeground.initial
         <<",\"native_native_part_note_service_active\":"<<ks::test::legacyRuntime(*b)->nativePartNoteForeground.active()
         <<",\"native_native_internal_note_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeInternalNoteForeground.started
         <<",\"native_native_internal_note_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeInternalNoteForeground.completed
         <<",\"native_native_internal_note_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeInternalNoteForeground.phases
         <<",\"native_native_internal_note_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeInternalNoteForeground.initial
         <<",\"native_native_internal_note_service_active\":"<<ks::test::legacyRuntime(*b)->nativeInternalNoteForeground.active()
         <<",\"native_native_program_event_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeProgramEventForeground.started
         <<",\"native_native_program_event_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeProgramEventForeground.completed
         <<",\"native_native_program_event_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeProgramEventForeground.phases
         <<",\"native_native_program_event_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeProgramEventForeground.initial
         <<",\"native_native_program_event_service_active\":"<<ks::test::legacyRuntime(*b)->nativeProgramEventForeground.active()
         <<",\"native_native_arp_emission_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeArpEmissionForeground.started
         <<",\"native_native_arp_emission_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeArpEmissionForeground.completed
         <<",\"native_native_arp_emission_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeArpEmissionForeground.phases
         <<",\"native_native_arp_emission_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeArpEmissionForeground.initial
         <<",\"native_native_arp_emission_service_active\":"<<ks::test::legacyRuntime(*b)->nativeArpEmissionForeground.active()
         <<",\"native_native_mono_arp_bootstrap_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeMonoArpBootstrapForeground.started
         <<",\"native_native_mono_arp_bootstrap_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeMonoArpBootstrapForeground.completed
         <<",\"native_native_mono_arp_bootstrap_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeMonoArpBootstrapForeground.phases
         <<",\"native_native_mono_arp_bootstrap_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeMonoArpBootstrapForeground.initial
         <<",\"native_native_mono_arp_bootstrap_service_active\":"<<ks::test::legacyRuntime(*b)->nativeMonoArpBootstrapForeground.active()
         <<",\"native_native_arp_primitive_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeArpPrimitiveForeground.started
         <<",\"native_native_arp_primitive_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeArpPrimitiveForeground.completed
         <<",\"native_native_arp_primitive_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeArpPrimitiveForeground.phases
         <<",\"native_native_arp_primitive_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeArpPrimitiveForeground.initial
         <<",\"native_native_arp_primitive_service_active\":"<<ks::test::legacyRuntime(*b)->nativeArpPrimitiveForeground.active()
         <<",\"native_native_arp_gate_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeArpGateForeground.started
         <<",\"native_native_arp_gate_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeArpGateForeground.completed
         <<",\"native_native_arp_gate_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeArpGateForeground.phases
         <<",\"native_native_arp_gate_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeArpGateForeground.initial
         <<",\"native_native_arp_gate_service_active\":"<<ks::test::legacyRuntime(*b)->nativeArpGateForeground.active()
         <<",\"native_native_arp_emit_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeArpEmitForeground.started
         <<",\"native_native_arp_emit_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeArpEmitForeground.completed
         <<",\"native_native_arp_emit_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeArpEmitForeground.phases
         <<",\"native_native_arp_emit_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeArpEmitForeground.initial
         <<",\"native_native_arp_emit_service_active\":"<<ks::test::legacyRuntime(*b)->nativeArpEmitForeground.active()
         <<",\"native_native_arp_chord_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeArpChordForeground.started
         <<",\"native_native_arp_chord_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeArpChordForeground.completed
         <<",\"native_native_arp_chord_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeArpChordForeground.phases
         <<",\"native_native_arp_chord_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeArpChordForeground.initial
         <<",\"native_native_arp_chord_service_active\":"<<ks::test::legacyRuntime(*b)->nativeArpChordForeground.active()
         <<",\"native_native_arp_random_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeArpRandomForeground.started
         <<",\"native_native_arp_random_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeArpRandomForeground.completed
         <<",\"native_native_arp_random_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeArpRandomForeground.phases
         <<",\"native_native_arp_random_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeArpRandomForeground.initial
         <<",\"native_native_arp_random_service_active\":"<<ks::test::legacyRuntime(*b)->nativeArpRandomForeground.active()
         <<",\"native_native_arp_search_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeArpSearchForeground.started
         <<",\"native_native_arp_search_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeArpSearchForeground.completed
         <<",\"native_native_arp_search_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeArpSearchForeground.phases
         <<",\"native_native_arp_search_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeArpSearchForeground.initial
         <<",\"native_native_arp_search_service_active\":"<<ks::test::legacyRuntime(*b)->nativeArpSearchForeground.active()
         <<",\"native_scheduled_timer_service_calls\":"<<ks::test::legacyRuntime(*b)->scheduledTimerForeground.started
         <<",\"native_scheduled_timer_service_returns\":"<<ks::test::legacyRuntime(*b)->scheduledTimerForeground.completed
         <<",\"native_scheduled_timer_service_phases\":"<<ks::test::legacyRuntime(*b)->scheduledTimerForeground.phases
         <<",\"native_scheduled_timer_service_initial_active\":"<<ks::test::legacyRuntime(*b)->scheduledTimerForeground.initial
         <<",\"native_scheduled_timer_service_active\":"<<ks::test::legacyRuntime(*b)->scheduledTimerForeground.active()
         <<",\"native_native_midi_aux_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeMidiAuxForeground.started
         <<",\"native_native_midi_aux_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeMidiAuxForeground.completed
         <<",\"native_native_midi_aux_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeMidiAuxForeground.phases
         <<",\"native_native_midi_aux_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeMidiAuxForeground.initial
         <<",\"native_native_midi_aux_service_active\":"<<ks::test::legacyRuntime(*b)->nativeMidiAuxForeground.active()
         <<",\"native_native_arp_ordered_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeArpOrderedForeground.started
         <<",\"native_native_arp_ordered_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeArpOrderedForeground.completed
         <<",\"native_native_arp_ordered_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeArpOrderedForeground.phases
         <<",\"native_native_arp_ordered_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeArpOrderedForeground.initial
         <<",\"native_native_arp_ordered_service_active\":"<<ks::test::legacyRuntime(*b)->nativeArpOrderedForeground.active()
         <<",\"native_native_arp_run_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeArpRunForeground.started
         <<",\"native_native_arp_run_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeArpRunForeground.completed
         <<",\"native_native_arp_run_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeArpRunForeground.phases
         <<",\"native_native_arp_run_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeArpRunForeground.initial
         <<",\"native_native_arp_run_service_active\":"<<ks::test::legacyRuntime(*b)->nativeArpRunForeground.active()
         <<",\"native_main_service_calls\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.started
         <<",\"native_main_service_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.completed
         <<",\"native_main_service_phases\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.phases
         <<",\"native_main_service_initial_active\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.initial
         <<",\"native_main_service_active\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.active()
         <<",\"native_main_legacy_bodies_after_admission\":"<<ks::test::legacyRuntime(*b)->legacyBodiesAfterMain
         <<",\"native_main_service_child0_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[0]
         <<",\"native_main_service_child1_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[1]
         <<",\"native_main_service_child2_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[2]
         <<",\"native_main_service_child3_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[3]
         <<",\"native_main_service_child4_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[4]
         <<",\"native_main_service_child5_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[5]
         <<",\"native_main_service_child6_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[6]
         <<",\"native_main_service_child7_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[7]
         <<",\"native_main_service_child8_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[8]
         <<",\"native_main_service_child9_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[9]
         <<",\"native_main_service_child10_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[10]
         <<",\"native_main_service_child11_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[11]
         <<",\"native_main_service_child12_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[12]
         <<",\"native_main_service_child13_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[13]
         <<",\"native_main_service_child14_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[14]
         <<",\"native_main_service_child15_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[15]
         <<",\"native_main_service_child16_returns\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.children[16]
         <<",\"native_main_tick_initial\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.initial
         <<",\"native_main_tick_started\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.started
         <<",\"native_main_tick_completed\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.completed
         <<",\"native_main_tick_clock\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.clock
         <<",\"native_main_tick_buttons\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.buttons
         <<",\"native_main_tick_adc\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.adc
         <<",\"native_main_tick_encoder\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.encoder
         <<",\"native_main_tick_pedal\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.pedal
         <<",\"native_main_tick_output\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.output
         <<",\"native_main_tick_refresh\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.refresh
         <<",\"native_main_tick_components\":"<<ks::test::legacyRuntime(*b)->serviceCycleForeground.tickReturns.components
         <<",\"native_midi_pipeline_service_calls\":"<<ks::test::legacyRuntime(*b)->nativeMidiPipelineForeground.started
         <<",\"native_midi_pipeline_service_returns\":"<<ks::test::legacyRuntime(*b)->nativeMidiPipelineForeground.completed
         <<",\"native_midi_pipeline_service_phases\":"<<ks::test::legacyRuntime(*b)->nativeMidiPipelineForeground.phases
         <<",\"native_midi_pipeline_service_initial_active\":"<<ks::test::legacyRuntime(*b)->nativeMidiPipelineForeground.initial
         <<",\"native_midi_pipeline_service_active\":"<<ks::test::legacyRuntime(*b)->nativeMidiPipelineForeground.active()
         <<",\"midi_file_event_service_calls\":"<<ks::test::legacyRuntime(*b)->midiFileEventForeground.started
         <<",\"midi_file_event_service_returns\":"<<ks::test::legacyRuntime(*b)->midiFileEventForeground.completed
         <<",\"midi_file_event_service_phases\":"<<ks::test::legacyRuntime(*b)->midiFileEventForeground.phases
         <<",\"midi_file_event_service_initial_active\":"<<ks::test::legacyRuntime(*b)->midiFileEventForeground.initial
         <<",\"midi_file_event_service_active\":"<<ks::test::legacyRuntime(*b)->midiFileEventForeground.active()
         <<",\"native_state_tick_service_calls\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.started
         <<",\"native_state_tick_service_returns\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.completed
         <<",\"native_state_tick_service_phases\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.phases
         <<",\"native_state_tick_service_initial_active\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.initial
         <<",\"native_state_tick_service_active\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.active()
         <<",\"native_state_tick_service_components\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.components
         <<",\"native_state_tick_service_tick\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.kinds[0]
         <<",\"native_state_tick_service_returned_clock\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.returnedClock
         <<",\"native_state_tick_service_returned_buttons\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.returnedButtons
         <<",\"native_state_tick_service_returned_adc\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.returnedAdc
         <<",\"native_state_tick_service_returned_encoder\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.returnedEncoder
         <<",\"native_state_tick_service_returned_pedal\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.returnedPedal
         <<",\"native_state_tick_service_returned_output\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.returnedOutput
         <<",\"native_state_tick_service_returned_refresh\":"<<ks::test::legacyRuntime(*b)->stateTickForeground.returnedRefresh
         <<",\"native_state_factory_service_calls\":"<<factory.started
         <<",\"native_state_factory_service_returns\":"<<factory.completed
         <<",\"native_state_factory_service_phases\":"<<factory.phases
         <<",\"native_state_factory_service_initial_active\":"<<factory.initial
         <<",\"native_state_factory_service_active\":"<<factory.active()
         <<",\"native_state_factory_service_components\":"<<factory.components
         <<",\"native_state_factory_service_provided\":"<<factory.kinds[0]
         <<",\"native_state_factory_service_factory\":"<<factory.kinds[1]
         <<",\"native_state_factory_service_global_load\":"<<factory.kinds[2]
         <<",\"native_state_factory_service_reset\":"<<factory.kinds[3]
         <<",\"native_state_factory_service_upload\":"<<factory.kinds[4]
         <<",\"native_state_factory_service_poll\":"<<factory.kinds[5]
         <<",\"native_state_factory_service_stream\":"<<factory.kinds[6]
         <<"}\n";gates.close();check(bool(gates),"Cold semantic evidence write failed");
    std::cout<<"SEMANTIC_GATES_COMPLETE "<<variant<<" factory_calls="<<factory.started<<" factory_returns="<<factory.completed<<" factory_active="<<factory.active()<<std::endl;
    check(cpuLocationsAcceptable,"Unexpected reset DSP CPU location change");
    std::cout<<"PASS: "<<variant<<" cold reset, "<<slice<<" original coordinator slices, "<<x->events.size()<<" exact DSP port events, all resource memory (CPU scratch stack excluded)/state/Flash/IO/IRQ, 24000 exact 24-stream audible frames; reference H8 steps="<<a->h8InterpretedSteps()<<", candidate H8 steps="<<b->h8InterpretedSteps()<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"slice="<<slice<<": "<<e.what()<<'\n';return 1;}}
