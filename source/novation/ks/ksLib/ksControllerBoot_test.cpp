#include "ksControllerTraceComparison_test.h"
#include "ksBoard.h"
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
    auto& x=static_cast<ks::ControllerCpu&>(a.cpu());auto& y=static_cast<ks::ControllerCpu&>(b.cpu());
    const bool nativeWork=b.nativeController()&&b.nativeController()->activeNativeWork();
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
    check(b.liveMidiController()->noteActions.keySearchCalls.partialEntries==0,"Native reset used the test-only partial-key adapter");
    check(!b.liveMidiController()->startupCalls.partialEntries && !b.liveMidiController()->voiceActions.selectCalls.partialEntries,"Native reset used partial startup/select recovery");
    check(!b.liveMidiController()->voiceActions.routingCalls.partialEntries,"Native reset used partial routing recovery");
    check(!b.liveMidiController()->noteActions.partControlCalls.partialEntries && !b.liveMidiController()->voiceActions.partControlCalls.partialEntries,"Native reset used partial part-control recovery");
    check(!b.liveMidiController()->voiceActions.velocityCurveCalls.partialEntries,"Native reset used partial velocity recovery");
    check(!b.liveMidiController()->voiceActions.selectionCalls.partialEntries,"Native reset used partial selection/gate recovery");
    check(!b.liveMidiController()->voiceActions.populationCalls.partialEntries,"Native reset used partial population recovery");
    check(b.liveMidiController()->bootSetupCalls.partialEntries==0,"Native reset used partial bootstrap setup recovery");
    check(b.liveMidiController()->bootStreamCalls.partialEntries==0,"Native reset used partial bootstrap-stream recovery");
    check(b.liveMidiController()->faultCalls.partialEntries==0,"Native reset used partial fault recovery");
    check(b.liveMidiController()->voiceActions.addressCalls.partialEntries==0,"Native reset used partial DSP-address recovery");
    check(b.liveMidiController()->voiceActions.wordCalls.partialEntries==0,"Native reset used partial DSP-word recovery");
    check(b.liveMidiController()->parameterScaleCalls.partialEntries==0,"Native reset used partial coefficient recovery");
    check(!b.liveMidiController()->panelMatrixCalls.partialEntries,"Native reset used partial panel-matrix recovery");
    check(!b.liveMidiController()->pedalSenseCalls.partialEntries,"Native reset used partial pedal recovery");
    check(!b.liveMidiController()->panelEncoderCalls.partialEntries,"Native reset used partial encoder recovery");
    check(!b.liveMidiController()->panelAdcSamplesCalls.partialEntries,"Native reset used partial ADC-samples recovery");
    check(!b.liveMidiController()->panelAdcScaleCalls.partialEntries,"Native reset used partial ADC-scale recovery");
    check(!b.liveMidiController()->panelAdcCalibrationCalls.partialEntries,"Native reset used partial ADC-calibration recovery");
    check(!b.liveMidiController()->panelAdcIrqCalls.partialEntries,"Native reset used partial ADC-IRQ recovery");
    check(!b.liveMidiController()->panelAdcScanCalls.partialEntries,"Native reset used partial ADC-scan recovery");
    check(!b.liveMidiController()->internalMidiQueueCalls.partialEntries,"Native reset used partial InternalMidiQueue recovery");
    check(!b.liveMidiController()->midiOutputCalls.partialEntries,"Native reset used partial MidiOutput recovery");
    check(!b.liveMidiController()->internalMidiServiceCalls.partialEntries,"Native reset used partial InternalMidiService recovery");
    check(!b.liveMidiController()->stateForegroundCalls.partialEntries,"Native reset used partial StateForeground recovery");
    check(!b.liveMidiController()->stateButtonDispatchCalls.partialEntries,"Native reset used partial StateButtonDispatch recovery");
    check(!b.liveMidiController()->panelAnalogCalls.partialEntries,"Native reset used partial PanelAnalog recovery");
    check(!b.liveMidiController()->clockSyncCalls.partialEntries,"Native reset used partial ClockSync recovery");
    check(!b.liveMidiController()->clockPulseCalls.partialEntries,"Native reset used partial ClockPulse recovery");
    check(!b.liveMidiController()->clockDisplayCalls.partialEntries,"Native reset used partial ClockDisplay recovery");
    check(!b.liveMidiController()->stateScreenCalls.partialEntries,"Native reset used partial StateScreen recovery");
    check(!b.liveMidiController()->panelSettingsCalls.partialEntries,"Native reset used partial PanelSettings recovery");
    check(!b.liveMidiController()->stateEditContextCalls.partialEntries,"Native reset used partial StateEditContext recovery");
    check(!b.liveMidiController()->stateScreenEditCalls.partialEntries,"Native reset used partial StateScreenEdit recovery");
    check(!b.liveMidiController()->stateMenuFormatCalls.partialEntries,"Native reset used partial StateMenuFormat recovery");
    check(!b.liveMidiController()->systemServicesCalls.partialEntries,"Native reset used partial SystemServices recovery");
    check(!b.liveMidiController()->keyboardEventsCalls.partialEntries,"Native reset used partial KeyboardEvents recovery");
    check(!b.liveMidiController()->statePartSelectionCalls.partialEntries,"Native reset used partial StatePartSelection recovery");
    check(!b.liveMidiController()->stateMenuActionsCalls.partialEntries,"Native reset used partial StateMenuActions recovery");
    check(!b.liveMidiController()->keyboardScanCalls.partialEntries,"Native reset used partial KeyboardScan recovery");
    check(!b.liveMidiController()->diagnosticCodecCalls.partialEntries,"Native reset used partial DiagnosticCodec recovery");
    check(!b.liveMidiController()->diagnosticTransferCalls.partialEntries,"Native reset used partial DiagnosticTransfer recovery");
    check(!b.liveMidiController()->memoryDiagnosticsCalls.partialEntries,"Native reset used partial MemoryDiagnostics recovery");
    check(!b.liveMidiController()->startupBannerCalls.partialEntries,"Native reset used partial StartupBanner recovery");
    check(!b.liveMidiController()->stateScreenActionsCalls.partialEntries,"Native reset used partial StateScreenActions recovery");
    check(!b.liveMidiController()->statePanelModesCalls.partialEntries,"Native reset used partial StatePanelModes recovery");
    check(!b.liveMidiController()->stateFunctionKeysCalls.partialEntries,"Native reset used partial StateFunctionKeys recovery");
    check(!b.liveMidiController()->stateEditWorkflowCalls.partialEntries,"Native reset used partial StateEditWorkflow recovery");
    check(!b.liveMidiController()->loaderInputsCalls.partialEntries,"Native reset used partial LoaderInputs recovery");
    check(!b.liveMidiController()->loaderCodecCalls.partialEntries,"Native reset used partial LoaderCodec recovery");
    check(!b.liveMidiController()->loaderPacketsCalls.partialEntries,"Native reset used partial LoaderPackets recovery");
    check(!b.liveMidiController()->loaderProgramCalls.partialEntries,"Native reset used partial LoaderProgram recovery");
    check(!b.liveMidiController()->loaderSessionCalls.partialEntries,"Native reset used partial LoaderSession recovery");
    check(!b.liveMidiController()->stateValueBoundsCalls.partialEntries,"Native reset used partial StateValueBounds recovery");
    check(!b.liveMidiController()->stateEditLabelCalls.partialEntries,"Native reset used partial StateEditLabel recovery");
    check(!b.liveMidiController()->stateFieldActionsCalls.partialEntries,"Native reset used partial StateFieldActions recovery");
    check(!b.liveMidiController()->stateBankNavigationCalls.partialEntries,"Native reset used partial StateBankNavigation recovery");
    check(!b.liveMidiController()->stateStatusDisplayCalls.partialEntries,"Native reset used partial StateStatusDisplay recovery");
    check(!b.liveMidiController()->stateStoredValueCalls.partialEntries,"Native reset used partial StateStoredValue recovery");
    check(!b.liveMidiController()->stateBrowseWorkflowCalls.partialEntries,"Native reset used partial StateBrowseWorkflow recovery");
    check(!b.liveMidiController()->stateProgramBrowseCalls.partialEntries,"Native reset used partial StateProgramBrowse recovery");
    check(!b.liveMidiController()->stateProgramDisplayCalls.partialEntries,"Native reset used partial StateProgramDisplay recovery");
    check(!b.liveMidiController()->stateProgramEventCalls.partialEntries,"Native reset used partial StateProgramEvent recovery");
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
    auto a=std::make_unique<ks::Board>(os.data(),wave,flash,true,false);
    auto b=std::make_unique<ks::Board>(os.data(),wave,flash,true,true);
    same(*a,*b);a->startControllerTrace();b->startControllerTrace();
    // Match production 128-cycle coordination from reset; the oracle owns
    // its original timer/IRQ/core path and independent DSP instances.
    while(!a->ready){
        check(a->controllerCycles()<ks::Board::ControllerClockHz*10ull,"Boot timed out");
        constexpr unsigned budgets[]{128,127,129,97,193,255,1,63};
        const auto budget=variant=="irregular"?budgets[slice%8]:128;
        a->advance(budget);b->advance(budget);++slice;same(*a,*b);a->discardAudio();b->discardAudio();
        if(!(slice%100000))std::cout<<"slices="<<slice<<" pc="<<std::hex<<a->cpu().getPC()<<std::dec<<" cycle="<<a->controllerCycles()<<std::endl;
    }
    check(a->boot() && b->boot(),"Warm initialization failed");same(*a,*b);
    check(b->liveMidiController()->startupCalls.startupStarted==1 && b->liveMidiController()->startupCalls.startupCompleted==1 && b->liveMidiController()->startupCalls.resetStarted==1 && b->liveMidiController()->startupCalls.resetCompleted==1,"Cold start did not complete native outer startup/reset");
    check(b->liveMidiController()->bootSetupCalls.started==2 && b->liveMidiController()->bootSetupCalls.completed==2,"Cold startup did not complete both native DSP setup jobs");
    check(b->liveMidiController()->bootStreamCalls.started==2 && b->liveMidiController()->bootStreamCalls.completed==2,"Cold startup did not complete both native DSP firmware streams");
    check(b->liveMidiController()->panelAdcCalibrationCalls.started>0&&b->liveMidiController()->panelAdcCalibrationCalls.completed>0,"Cold start did not exercise native ADC calibration");
    check(b->nativeController()->panelSettingsForeground.kinds[unsigned(ks::PanelSettingsService::Kind::Warmup)]>0&&(b->nativeController()->panelSettingsForeground.components&(1u<<unsigned(ks::PanelSettingsService::Component::AdcStart)))&&b->nativeController()->nativeInterruptKinds[4]>0,"Cold start did not exercise CPU-free ADC warmup/start/IRQ");
    // Native IRQs no longer materialize temporary CPU frames. Only the
    // firmware's reserved scratch stack may differ; all other 16 MiB bytes
    // remain part of the independent cold-boot comparison.
    const auto sameResources=[&]{
        const auto*x=a->cpu().memory;const auto*y=b->cpu().memory;
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
    const auto& factory=b->nativeController()->stateFactoryForeground;
    std::ofstream gates(out/"cold-semantic-gates.json");check(bool(gates),"Cannot save cold semantic evidence");
    gates<<"{\"semantic_gates_complete\":true,\"variant\":\""<<variant<<"\",\"frames\":24000,\"channels\":24,\"encoding\":\"int32le\",\"peak\":"<<peak
         <<",\"events\":"<<x->events.size()<<",\"slices\":"<<x->slices.size()<<",\"controller_cycles\":"<<b->controllerCycles()
         <<",\"checkpoint_exact\":true,\"every_pcm_sample_exact\":true,\"every_nonPC_event_and_slice_exact\":true"
         <<",\"strict_pc_acceptable\":"<<(cpuLocationsAcceptable?"true":"false")<<",\"unexpected_pc_events\":"<<unexpectedCpuLocationChanges
         <<",\"native_clock_sync_service_calls\":"<<b->nativeController()->clockSyncForeground.started
         <<",\"native_clock_sync_service_returns\":"<<b->nativeController()->clockSyncForeground.completed
         <<",\"native_clock_sync_service_phases\":"<<b->nativeController()->clockSyncForeground.phases
         <<",\"native_clock_sync_service_initial_active\":"<<b->nativeController()->clockSyncForeground.initial
         <<",\"native_clock_sync_service_active\":"<<b->nativeController()->clockSyncForeground.active()
         <<",\"native_clock_sync_service_components\":"<<b->nativeController()->clockSyncForeground.components
         <<",\"native_clock_sync_service_update\":"<<b->nativeController()->clockSyncForeground.kinds[0]
         <<",\"native_state_deferred_parts_service_calls\":"<<b->nativeController()->stateDeferredPartsForeground.started
         <<",\"native_state_deferred_parts_service_returns\":"<<b->nativeController()->stateDeferredPartsForeground.completed
         <<",\"native_state_deferred_parts_service_phases\":"<<b->nativeController()->stateDeferredPartsForeground.phases
         <<",\"native_state_deferred_parts_service_initial_active\":"<<b->nativeController()->stateDeferredPartsForeground.initial
         <<",\"native_state_deferred_parts_service_active\":"<<b->nativeController()->stateDeferredPartsForeground.active()
         <<",\"native_state_deferred_parts_service_components\":"<<b->nativeController()->stateDeferredPartsForeground.components
         <<",\"native_state_deferred_parts_service_deferred\":"<<b->nativeController()->stateDeferredPartsForeground.kinds[0]
         <<",\"native_state_deferred_parts_service_processed\":"<<b->nativeController()->stateDeferredPartsForeground.processed
         <<",\"native_state_deferred_parts_service_voice_uploads\":"<<b->nativeController()->stateDeferredPartsForeground.voiceUploads
         <<",\"native_state_deferred_parts_service_part_mask\":"<<b->nativeController()->stateDeferredPartsForeground.partMask
         <<",\"native_state_refresh_service_calls\":"<<b->nativeController()->stateRefreshForeground.started
         <<",\"native_state_refresh_service_returns\":"<<b->nativeController()->stateRefreshForeground.completed
         <<",\"native_state_refresh_service_phases\":"<<b->nativeController()->stateRefreshForeground.phases
         <<",\"native_state_refresh_service_initial_active\":"<<b->nativeController()->stateRefreshForeground.initial
         <<",\"native_state_refresh_service_active\":"<<b->nativeController()->stateRefreshForeground.active()
         <<",\"native_state_refresh_service_components\":"<<b->nativeController()->stateRefreshForeground.components
         <<",\"native_state_refresh_service_refresh\":"<<b->nativeController()->stateRefreshForeground.kinds[0]
         <<",\"native_state_refresh_service_returned_packet\":"<<b->nativeController()->stateRefreshForeground.returnedPacket
         <<",\"native_state_refresh_service_returned_panel\":"<<b->nativeController()->stateRefreshForeground.returnedPanel
         <<",\"native_state_refresh_service_returned_deferred\":"<<b->nativeController()->stateRefreshForeground.returnedDeferred
         <<",\"native_state_refresh_service_returned_clock\":"<<b->nativeController()->stateRefreshForeground.returnedClock
         <<",\"native_state_refresh_service_processed_parts\":"<<b->nativeController()->stateRefreshForeground.processedParts
         <<",\"native_state_refresh_service_returned_coefficient\":"<<b->nativeController()->stateRefreshForeground.returnedCoefficient
         <<",\"native_state_refresh_service_returned_period_address\":"<<b->nativeController()->stateRefreshForeground.returnedPeriodAddress
         <<",\"native_state_refresh_service_returned_period_value\":"<<b->nativeController()->stateRefreshForeground.returnedPeriodValue
         <<",\"native_panel_pedal_output_service_calls\":"<<b->nativeController()->panelPedalOutputForeground.started
         <<",\"native_panel_pedal_output_service_returns\":"<<b->nativeController()->panelPedalOutputForeground.completed
         <<",\"native_panel_pedal_output_service_phases\":"<<b->nativeController()->panelPedalOutputForeground.phases
         <<",\"native_panel_pedal_output_service_initial_active\":"<<b->nativeController()->panelPedalOutputForeground.initial
         <<",\"native_panel_pedal_output_service_active\":"<<b->nativeController()->panelPedalOutputForeground.active()
         <<",\"native_panel_pedal_output_service_components\":"<<b->nativeController()->panelPedalOutputForeground.components
         <<",\"native_panel_pedal_output_service_pedal\":"<<b->nativeController()->panelPedalOutputForeground.kinds[0]
         <<",\"native_panel_pedal_output_service_byte\":"<<b->nativeController()->panelPedalOutputForeground.kinds[1]
         <<",\"native_state_encoder_service_calls\":"<<b->nativeController()->stateEncoderForeground.started
         <<",\"native_state_encoder_service_returns\":"<<b->nativeController()->stateEncoderForeground.completed
         <<",\"native_state_encoder_service_phases\":"<<b->nativeController()->stateEncoderForeground.phases
         <<",\"native_state_encoder_service_initial_active\":"<<b->nativeController()->stateEncoderForeground.initial
         <<",\"native_state_encoder_service_active\":"<<b->nativeController()->stateEncoderForeground.active()
         <<",\"native_state_encoder_service_components\":"<<b->nativeController()->stateEncoderForeground.components
         <<",\"native_state_encoder_service_encoder\":"<<b->nativeController()->stateEncoderForeground.kinds[0]
         <<",\"native_panel_masks_service_calls\":"<<b->nativeController()->panelMasksForeground.started
         <<",\"native_panel_masks_service_returns\":"<<b->nativeController()->panelMasksForeground.completed
         <<",\"native_panel_masks_service_phases\":"<<b->nativeController()->panelMasksForeground.phases
         <<",\"native_panel_masks_service_initial_active\":"<<b->nativeController()->panelMasksForeground.initial
         <<",\"native_panel_masks_service_active\":"<<b->nativeController()->panelMasksForeground.active()
         <<",\"native_voice_feedback_service_calls\":"<<b->nativeController()->voiceFeedbackForeground.started
         <<",\"native_voice_feedback_service_returns\":"<<b->nativeController()->voiceFeedbackForeground.completed
         <<",\"native_voice_feedback_service_phases\":"<<b->nativeController()->voiceFeedbackForeground.phases
         <<",\"native_voice_feedback_service_initial_active\":"<<b->nativeController()->voiceFeedbackForeground.initial
         <<",\"native_voice_feedback_service_active\":"<<b->nativeController()->voiceFeedbackForeground.active()
         <<",\"native_part_feedback_service_calls\":"<<b->nativeController()->partFeedbackForeground.started
         <<",\"native_part_feedback_service_returns\":"<<b->nativeController()->partFeedbackForeground.completed
         <<",\"native_part_feedback_service_phases\":"<<b->nativeController()->partFeedbackForeground.phases
         <<",\"native_part_feedback_service_initial_active\":"<<b->nativeController()->partFeedbackForeground.initial
         <<",\"native_part_feedback_service_active\":"<<b->nativeController()->partFeedbackForeground.active()
         <<",\"native_periodic_service_calls\":"<<b->nativeController()->periodicForeground.started
         <<",\"native_periodic_service_returns\":"<<b->nativeController()->periodicForeground.completed
         <<",\"native_periodic_service_phases\":"<<b->nativeController()->periodicForeground.phases
         <<",\"native_periodic_service_initial_active\":"<<b->nativeController()->periodicForeground.initial
         <<",\"native_periodic_service_active\":"<<b->nativeController()->periodicForeground.active()
         <<",\"native_state_channel_service_calls\":"<<b->nativeController()->stateChannelForeground.started
         <<",\"native_state_channel_service_returns\":"<<b->nativeController()->stateChannelForeground.completed
         <<",\"native_state_channel_service_phases\":"<<b->nativeController()->stateChannelForeground.phases
         <<",\"native_state_channel_service_initial_active\":"<<b->nativeController()->stateChannelForeground.initial
         <<",\"native_state_channel_service_active\":"<<b->nativeController()->stateChannelForeground.active()
         <<",\"native_midi_packet_service_calls\":"<<b->nativeController()->midiPacketForeground.started
         <<",\"native_midi_packet_service_returns\":"<<b->nativeController()->midiPacketForeground.completed
         <<",\"native_midi_packet_service_phases\":"<<b->nativeController()->midiPacketForeground.phases
         <<",\"native_midi_packet_service_initial_active\":"<<b->nativeController()->midiPacketForeground.initial
         <<",\"native_midi_packet_service_active\":"<<b->nativeController()->midiPacketForeground.active()
         <<",\"native_native_key_service_calls\":"<<b->nativeController()->nativeKeyForeground.started
         <<",\"native_native_key_service_returns\":"<<b->nativeController()->nativeKeyForeground.completed
         <<",\"native_native_key_service_phases\":"<<b->nativeController()->nativeKeyForeground.phases
         <<",\"native_native_key_service_initial_active\":"<<b->nativeController()->nativeKeyForeground.initial
         <<",\"native_native_key_service_active\":"<<b->nativeController()->nativeKeyForeground.active()
         <<",\"native_native_arp_reset_service_calls\":"<<b->nativeController()->nativeArpResetForeground.started
         <<",\"native_native_arp_reset_service_returns\":"<<b->nativeController()->nativeArpResetForeground.completed
         <<",\"native_native_arp_reset_service_phases\":"<<b->nativeController()->nativeArpResetForeground.phases
         <<",\"native_native_arp_reset_service_initial_active\":"<<b->nativeController()->nativeArpResetForeground.initial
         <<",\"native_native_arp_reset_service_active\":"<<b->nativeController()->nativeArpResetForeground.active()
         <<",\"native_native_voice_release_service_calls\":"<<b->nativeController()->nativeVoiceReleaseForeground.started
         <<",\"native_native_voice_release_service_returns\":"<<b->nativeController()->nativeVoiceReleaseForeground.completed
         <<",\"native_native_voice_release_service_phases\":"<<b->nativeController()->nativeVoiceReleaseForeground.phases
         <<",\"native_native_voice_release_service_initial_active\":"<<b->nativeController()->nativeVoiceReleaseForeground.initial
         <<",\"native_native_voice_release_service_active\":"<<b->nativeController()->nativeVoiceReleaseForeground.active()
         <<",\"native_native_voice_allocation_service_calls\":"<<b->nativeController()->nativeVoiceAllocationForeground.started
         <<",\"native_native_voice_allocation_service_returns\":"<<b->nativeController()->nativeVoiceAllocationForeground.completed
         <<",\"native_native_voice_allocation_service_phases\":"<<b->nativeController()->nativeVoiceAllocationForeground.phases
         <<",\"native_native_voice_allocation_service_initial_active\":"<<b->nativeController()->nativeVoiceAllocationForeground.initial
         <<",\"native_native_voice_allocation_service_active\":"<<b->nativeController()->nativeVoiceAllocationForeground.active()
         <<",\"native_native_voice_start_service_calls\":"<<b->nativeController()->nativeVoiceStartForeground.started
         <<",\"native_native_voice_start_service_returns\":"<<b->nativeController()->nativeVoiceStartForeground.completed
         <<",\"native_native_voice_start_service_phases\":"<<b->nativeController()->nativeVoiceStartForeground.phases
         <<",\"native_native_voice_start_service_initial_active\":"<<b->nativeController()->nativeVoiceStartForeground.initial
         <<",\"native_native_voice_start_service_active\":"<<b->nativeController()->nativeVoiceStartForeground.active()
         <<",\"native_native_voice_modulation_service_calls\":"<<b->nativeController()->nativeVoiceModulationForeground.started
         <<",\"native_native_voice_modulation_service_returns\":"<<b->nativeController()->nativeVoiceModulationForeground.completed
         <<",\"native_native_voice_modulation_service_phases\":"<<b->nativeController()->nativeVoiceModulationForeground.phases
         <<",\"native_native_voice_modulation_service_initial_active\":"<<b->nativeController()->nativeVoiceModulationForeground.initial
         <<",\"native_native_voice_modulation_service_active\":"<<b->nativeController()->nativeVoiceModulationForeground.active()
         <<",\"native_native_voice_sweep_service_calls\":"<<b->nativeController()->nativeVoiceSweepForeground.started
         <<",\"native_native_voice_sweep_service_returns\":"<<b->nativeController()->nativeVoiceSweepForeground.completed
         <<",\"native_native_voice_sweep_service_phases\":"<<b->nativeController()->nativeVoiceSweepForeground.phases
         <<",\"native_native_voice_sweep_service_initial_active\":"<<b->nativeController()->nativeVoiceSweepForeground.initial
         <<",\"native_native_voice_sweep_service_active\":"<<b->nativeController()->nativeVoiceSweepForeground.active()
         <<",\"native_internal_key_output_service_calls\":"<<b->nativeController()->internalKeyOutputForeground.started
         <<",\"native_internal_key_output_service_returns\":"<<b->nativeController()->internalKeyOutputForeground.completed
         <<",\"native_internal_key_output_service_phases\":"<<b->nativeController()->internalKeyOutputForeground.phases
         <<",\"native_internal_key_output_service_initial_active\":"<<b->nativeController()->internalKeyOutputForeground.initial
         <<",\"native_internal_key_output_service_active\":"<<b->nativeController()->internalKeyOutputForeground.active()
         <<",\"native_native_note_state_service_calls\":"<<b->nativeController()->nativeNoteStateForeground.started
         <<",\"native_native_note_state_service_returns\":"<<b->nativeController()->nativeNoteStateForeground.completed
         <<",\"native_native_note_state_service_phases\":"<<b->nativeController()->nativeNoteStateForeground.phases
         <<",\"native_native_note_state_service_initial_active\":"<<b->nativeController()->nativeNoteStateForeground.initial
         <<",\"native_native_note_state_service_active\":"<<b->nativeController()->nativeNoteStateForeground.active()
         <<",\"native_native_note_program_service_calls\":"<<b->nativeController()->nativeNoteProgramForeground.started
         <<",\"native_native_note_program_service_returns\":"<<b->nativeController()->nativeNoteProgramForeground.completed
         <<",\"native_native_note_program_service_phases\":"<<b->nativeController()->nativeNoteProgramForeground.phases
         <<",\"native_native_note_program_service_initial_active\":"<<b->nativeController()->nativeNoteProgramForeground.initial
         <<",\"native_native_note_program_service_active\":"<<b->nativeController()->nativeNoteProgramForeground.active()
         <<",\"native_native_parameter_support_service_calls\":"<<b->nativeController()->nativeParameterSupportForeground.started
         <<",\"native_native_parameter_support_service_returns\":"<<b->nativeController()->nativeParameterSupportForeground.completed
         <<",\"native_native_parameter_support_service_phases\":"<<b->nativeController()->nativeParameterSupportForeground.phases
         <<",\"native_native_parameter_support_service_initial_active\":"<<b->nativeController()->nativeParameterSupportForeground.initial
         <<",\"native_native_parameter_support_service_active\":"<<b->nativeController()->nativeParameterSupportForeground.active()
         <<",\"native_native_parameter_edit_service_calls\":"<<b->nativeController()->nativeParameterEditForeground.started
         <<",\"native_native_parameter_edit_service_returns\":"<<b->nativeController()->nativeParameterEditForeground.completed
         <<",\"native_native_parameter_edit_service_phases\":"<<b->nativeController()->nativeParameterEditForeground.phases
         <<",\"native_native_parameter_edit_service_initial_active\":"<<b->nativeController()->nativeParameterEditForeground.initial
         <<",\"native_native_parameter_edit_service_active\":"<<b->nativeController()->nativeParameterEditForeground.active()
         <<",\"native_panel_monitor_service_calls\":"<<b->nativeController()->panelMonitorForeground.started
         <<",\"native_panel_monitor_service_returns\":"<<b->nativeController()->panelMonitorForeground.completed
         <<",\"native_panel_monitor_service_phases\":"<<b->nativeController()->panelMonitorForeground.phases
         <<",\"native_panel_monitor_service_initial_active\":"<<b->nativeController()->panelMonitorForeground.initial
         <<",\"native_panel_monitor_service_active\":"<<b->nativeController()->panelMonitorForeground.active()
         <<",\"native_state_status_service_calls\":"<<b->nativeController()->stateStatusForeground.started
         <<",\"native_state_status_service_returns\":"<<b->nativeController()->stateStatusForeground.completed
         <<",\"native_state_status_service_phases\":"<<b->nativeController()->stateStatusForeground.phases
         <<",\"native_state_status_service_initial_active\":"<<b->nativeController()->stateStatusForeground.initial
         <<",\"native_state_status_service_active\":"<<b->nativeController()->stateStatusForeground.active()
         <<",\"native_diagnostic_codec_service_calls\":"<<b->nativeController()->diagnosticCodecForeground.started
         <<",\"native_diagnostic_codec_service_returns\":"<<b->nativeController()->diagnosticCodecForeground.completed
         <<",\"native_diagnostic_codec_service_phases\":"<<b->nativeController()->diagnosticCodecForeground.phases
         <<",\"native_diagnostic_codec_service_initial_active\":"<<b->nativeController()->diagnosticCodecForeground.initial
         <<",\"native_diagnostic_codec_service_active\":"<<b->nativeController()->diagnosticCodecForeground.active()
         <<",\"native_diagnostic_transfer_service_calls\":"<<b->nativeController()->diagnosticTransferForeground.started
         <<",\"native_diagnostic_transfer_service_returns\":"<<b->nativeController()->diagnosticTransferForeground.completed
         <<",\"native_diagnostic_transfer_service_phases\":"<<b->nativeController()->diagnosticTransferForeground.phases
         <<",\"native_diagnostic_transfer_service_initial_active\":"<<b->nativeController()->diagnosticTransferForeground.initial
         <<",\"native_diagnostic_transfer_service_active\":"<<b->nativeController()->diagnosticTransferForeground.active()
         <<",\"native_midi_file_time_service_calls\":"<<b->nativeController()->midiFileTimeForeground.started
         <<",\"native_midi_file_time_service_returns\":"<<b->nativeController()->midiFileTimeForeground.completed
         <<",\"native_midi_file_time_service_phases\":"<<b->nativeController()->midiFileTimeForeground.phases
         <<",\"native_midi_file_time_service_initial_active\":"<<b->nativeController()->midiFileTimeForeground.initial
         <<",\"native_midi_file_time_service_active\":"<<b->nativeController()->midiFileTimeForeground.active()
         <<",\"native_state_tick_service_calls\":"<<b->nativeController()->stateTickForeground.started
         <<",\"native_state_tick_service_returns\":"<<b->nativeController()->stateTickForeground.completed
         <<",\"native_state_tick_service_phases\":"<<b->nativeController()->stateTickForeground.phases
         <<",\"native_state_tick_service_initial_active\":"<<b->nativeController()->stateTickForeground.initial
         <<",\"native_state_tick_service_active\":"<<b->nativeController()->stateTickForeground.active()
         <<",\"native_state_tick_service_components\":"<<b->nativeController()->stateTickForeground.components
         <<",\"native_state_tick_service_tick\":"<<b->nativeController()->stateTickForeground.kinds[0]
         <<",\"native_state_tick_service_returned_clock\":"<<b->nativeController()->stateTickForeground.returnedClock
         <<",\"native_state_tick_service_returned_buttons\":"<<b->nativeController()->stateTickForeground.returnedButtons
         <<",\"native_state_tick_service_returned_adc\":"<<b->nativeController()->stateTickForeground.returnedAdc
         <<",\"native_state_tick_service_returned_encoder\":"<<b->nativeController()->stateTickForeground.returnedEncoder
         <<",\"native_state_tick_service_returned_pedal\":"<<b->nativeController()->stateTickForeground.returnedPedal
         <<",\"native_state_tick_service_returned_output\":"<<b->nativeController()->stateTickForeground.returnedOutput
         <<",\"native_state_tick_service_returned_refresh\":"<<b->nativeController()->stateTickForeground.returnedRefresh
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
