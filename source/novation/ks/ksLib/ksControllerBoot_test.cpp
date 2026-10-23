#include "ksBoard.h"
#include "ksRom.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
namespace {
uint64_t slice=0;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
std::vector<uint8_t> bytes(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);check(bool(f),"Cannot read fixture");return {std::istreambuf_iterator<char>(f),{}};}
void same(ks::Board& a,ks::Board& b){
    auto& x=static_cast<ks::ControllerCpu&>(a.cpu());auto& y=static_cast<ks::ControllerCpu&>(b.cpu());
    const bool nativeIrq=b.nativeController()&&b.nativeController()->activeNativeInterrupts();
    if(x.cycles!=y.cycles || (!nativeIrq&&x.getPC()!=y.getPC())){
        std::cerr<<"reference pc="<<std::hex<<x.getPC()<<" candidate pc="<<y.getPC()<<std::dec<<" cycles "<<x.cycles<<" / "<<y.cycles<<'\n';
        throw std::runtime_error("Reset cycle/PC mismatch");
    }
    check((nativeIrq||x.ccr==y.ccr) && x.exr==y.exr,"Reset condition/mask mismatch");
    for(unsigned n=0;n<8;++n)if(!nativeIrq&&x.regs[n].er!=y.regs[n].er){std::cerr<<"register "<<n<<" values "<<std::hex<<x.regs[n].er<<" / "<<y.regs[n].er<<std::dec<<'\n';throw std::runtime_error("Reset ABI mismatch");}
    check((nativeIrq||(x.lastread==y.lastread && x.lastwrite==y.lastwrite)) && (nativeIrq||x.indent==y.indent),"Reset bus/stack history mismatch");
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
    check(b->liveMidiController()->panelAdcScanCalls.started>0&&b->nativeController()->nativeInterruptKinds[4]>0,"Cold start did not exercise native ADC scan/IRQ");
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
    for(unsigned n=0;n<24000;++n){
        if(n==8000){const uint8_t off[]{0x80,60,0};check(a->enqueueMidi(off,3)&&b->enqueueMidi(off,3),"Release rejected");}
        const auto x=a->renderSample(),y=b->renderSample();check(x==y,"Post-reset audio mismatch");same(*a,*b);
        for(unsigned c=0;c<4;++c)peak=std::max(peak,std::abs(int64_t(x[c])));
    }
    check(peak>1000,"Boot oracle not admitted as audible");
    const auto x=a->finishControllerTrace(),y=b->finishControllerTrace();x->validate();y->validate();
    x->save((out/"h8.kst").string());y->save((out/"cpp.kst").string());
    check(bytes(out/"h8.kst")==bytes(out/"cpp.kst"),"Full reset DSP bus/slice mismatch");
    check(a->saveUserFlash()==b->saveUserFlash(),"Reset Flash mismatch");
    check(ks::stateMessage(a->state())==ks::stateMessage(b->state()),"Reset musical state mismatch");
    check(sameResources(),"Post-reset resource memory mismatch");
    check(a->serialDepth()==b->serialDepth() && a->serialUnderruns==b->serialUnderruns && a->serialOverflows==b->serialOverflows,"Reset serial state mismatch");
    uint8_t v=0,w=0;while(a->popMidiOut(v))check(b->popMidiOut(w)&&v==w,"Reset MIDI output mismatch");check(!b->popMidiOut(w),"Trailing reset MIDI output");
    std::cout<<"PASS: "<<variant<<" cold reset, "<<slice<<" original coordinator slices, "<<x->events.size()<<" exact DSP port events, all resource memory (CPU scratch stack excluded)/state/Flash/IO/IRQ, 24000 exact 24-stream audible frames; reference H8 steps="<<a->h8InterpretedSteps()<<", candidate H8 steps="<<b->h8InterpretedSteps()<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"slice="<<slice<<": "<<e.what()<<'\n';return 1;}}
