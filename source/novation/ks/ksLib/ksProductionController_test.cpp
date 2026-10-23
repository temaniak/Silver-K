#include "ksControllerState.h"
#include "ksDevice.h"
#include "ksKeySearchCalls.h"
#include "ksParameterScaleCalls.h"
#include "ksControllerJobs.h"
#include "ksDspWordCalls.h"
#include "ksDspAddressCalls.h"
#include "ksDspFaultCalls.h"
#include "ksDspBootStreamCalls.h"
#include "ksDspBootSetupCalls.h"
#include "ksDspTransport.h"
#include "ksFlashPageCalls.h"
#include "ksTimerServiceCalls.h"
#include "ksStateChannelCalls.h"
#include "ksPanelInputCalls.h"
#include "ksStateScreenCalls.h"
#include "ksClockDisplayCalls.h"
#include "ksClockPulseCalls.h"
#include "ksClockSyncCalls.h"
#include "ksPanelAnalogCalls.h"
#include "ksStateButtonDispatchCalls.h"
#include "ksStateForegroundCalls.h"
#include "ksInternalMidiServiceCalls.h"
#include "ksMidiOutputCalls.h"
#include "ksInternalMidiQueueCalls.h"
#include "ksPanelAdcScanCalls.h"
#include "ksPanelAdcIrqCalls.h"
#include "ksPanelAdcCalibrationCalls.h"
#include "ksPanelAdcScaleCalls.h"
#include "ksPanelAdcSamplesCalls.h"
#include "ksPanelEncoderCalls.h"
#include "ksPedalSenseCalls.h"
#include "ksPanelMatrixCalls.h"
#include "ksStateStoreUiCalls.h"
#include "ksStateEditAddressCalls.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
int main(){try{
    if(ks::controllerReferenceAvailable())throw std::runtime_error("Production fixture linked the H8 comparison adapter");
    if(ks::Device::environmentControllerMode()!=ks::ControllerMode::Native)throw std::runtime_error("Production KS did not select C++");
    static_assert(sizeof(ks::ControllerCpu)<17u*1024*1024,"Generic device map returned to KS production storage");
    auto state=std::make_unique<ks::ControllerCpu>();
    state->cycles=123;state->pc=0x4334;state->regs[0].er=456;bool rejected=false;
    try{state->step();}catch(const std::logic_error& e){rejected=std::string(e.what()).find("comparison tests")!=std::string::npos;}
    if(!rejected || state->cycles!=123 || state->pc!=0x4334 || state->regs[0].er!=456 || state->interpretedSteps)
        throw std::runtime_error("Production controller exposed a live interpreter or mutated rejected work");
    ks::KeySearchCalls calls;state->pc=0x3190;
    if(calls.step(*state) || calls.partialEntries || state->cycles!=123 || state->pc!=0x3190)
        throw std::runtime_error("Production exposed the test-only partial-key adapter");
    ks::ParameterScaleCalls coefficients;state->pc=0xa6be;
    if(coefficients.step(*state) || coefficients.partialEntries || state->cycles!=123 || state->pc!=0xa6be)
        throw std::runtime_error("Production exposed partial coefficient recovery");
    const uint8_t sentinel=0;state->nativeFetchClocksOnly=true;state->nativeLowBase=&sentinel;state->nativeLowWritable=true;state->interruptMode2=true;
    if(ks::ControllerJobs::candidate(0xa6be) || ks::ControllerJobs::service(*state,1000).steps || state->cycles!=123 || state->pc!=0xa6be)
        throw std::runtime_error("Production exposed the retired register shift job");
    ks::DspWordCalls words;state->pc=0x2544;state->regs[5].er=0x400000;
    if(words.step(*state) || words.partialEntries || state->cycles!=123 || state->pc!=0x2544)
        throw std::runtime_error("Production exposed partial DSP-word recovery");
    if(ks::DspTransport::candidate(0x2520) || ks::DspTransport::candidate(0x2544)
        || ks::DspTransport::service(*state,1000) || state->cycles!=123 || state->pc!=0x2544)
        throw std::runtime_error("Production exposed retired register-driven DSP-word batch");
    ks::DspAddressCalls addresses;state->pc=0x2592;
    if(addresses.step(*state) || addresses.partialEntries || state->cycles!=123 || state->pc!=0x2592)
        throw std::runtime_error("Production exposed partial DSP-address recovery");
    for(unsigned entry:{0x2416u,0x2424u,0x245eu,0x246cu,0x256eu,0x257cu,0x25b6u,0x25c4u,0x243au,0x2482u,0x2592u,0x25dau,0x25e4u}){
        state->pc=entry;
        if(ks::DspTransport::candidate(entry) || ks::DspTransport::service(*state,1000) || state->cycles!=123 || state->pc!=entry)
            throw std::runtime_error("Production exposed retired address/read batch");
    }
    ks::DspFaultCalls faults;state->pc=0x662;
    if(faults.step(*state) || faults.partialEntries || state->cycles!=123 || state->pc!=0x662)
        throw std::runtime_error("Production exposed partial fault recovery");
    ks::DspBootStreamCalls streams;state->pc=0x23f2;
    if(streams.step(*state) || streams.partialEntries || state->cycles!=123 || state->pc!=0x23f2)
        throw std::runtime_error("Production exposed partial bootstrap stream recovery");
    state->pc=0x23ce;state->regs[3].e=1;state->regs[4].er=0xffff62;
    if(streams.step(*state) || streams.partialEntries || state->cycles!=123 || state->pc!=0x23ce)
        throw std::runtime_error("Native firmware source admitted one-clock control IO");
    state->regs[4].er=0x87ffff;
    if(streams.step(*state) || streams.partialEntries || state->cycles!=123 || state->pc!=0x23ce)
        throw std::runtime_error("Native firmware source admitted a crossing data span");
    ks::DspBootSetupCalls setups;state->pc=0x2386;
    if(setups.step(*state) || setups.partialEntries || state->cycles!=123 || state->pc!=0x2386)
        throw std::runtime_error("Production exposed partial bootstrap setup recovery");
    ks::DspStartupCalls startup;state->pc=0x138e;
    if(startup.step(*state)||startup.partialEntries||state->cycles!=123||state->pc!=0x138e)throw std::runtime_error("Production exposed partial DSP reset recovery");
    state->pc=0x2316;
    if(startup.step(*state)||startup.partialEntries||state->cycles!=123||state->pc!=0x2316)throw std::runtime_error("Production exposed partial DSP startup recovery");
    ks::DspSelectCalls selector;state->pc=0x24aa;
    if(selector.step(*state)||selector.partialEntries||state->cycles!=123||state->pc!=0x24aa)throw std::runtime_error("Production exposed partial DSP select recovery");
    ks::DspRoutingCalls routing;state->pc=0x7826;
    if(routing.step(*state)||routing.partialEntries||state->cycles!=123||state->pc!=0x7826)throw std::runtime_error("Production exposed partial DSP routing recovery");
    ks::PartControlCalls parts;state->pc=0xa694;
    if(parts.step(*state)||parts.partialEntries||state->cycles!=123||state->pc!=0xa694)throw std::runtime_error("Production exposed partial part mirror recovery");
    for(unsigned entry:{0x4000u,0x4006u,0xa66au,0xa670u}){state->pc=entry;
        if(ks::ControllerJobs::candidate(entry)||ks::ControllerJobs::service(*state,1000).steps||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed historical part batch");}
    ks::VelocityCurveCalls velocity;state->pc=0xa63e;
    if(velocity.step(*state)||velocity.partialEntries||state->cycles!=123||state->pc!=0xa63e)throw std::runtime_error("Production exposed partial velocity recovery");
    ks::VoiceSelectionCalls selection;state->pc=0xa412;
    if(selection.step(*state)||selection.partialEntries||state->cycles!=123||state->pc!=0xa412)throw std::runtime_error("Production exposed partial voice selection recovery");
    state->pc=0x8fc6;
    if(selection.step(*state)||selection.partialEntries||state->cycles!=123||state->pc!=0x8fc6)throw std::runtime_error("Production exposed partial gate recovery");
    ks::VoicePopulationCalls population;state->pc=0xa5e2;
    if(population.step(*state)||population.partialEntries||state->cycles!=123||state->pc!=0xa5e2)throw std::runtime_error("Production exposed partial population recovery");
    ks::FlashPageCalls flash;
    for(unsigned entry:{0x11b2u,0x1276u}){state->pc=entry;
        if(flash.step(*state)||flash.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial Flash page recovery");}
    ks::TimerServiceCalls timers;
    for(unsigned entry:{0x12ccu,0x1300u,0x27c6u}){state->pc=entry;
        if(timers.step(*state)||timers.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial timer recovery");}
    ks::StateChannelCalls channels;
    for(unsigned entry:{0x43b0u,0x43c8u,0x43f8u}){state->pc=entry;
        if(channels.step(*state)||channels.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial state channel recovery");}
    ks::PanelInputCalls panel;
    for(unsigned entry:{0x1402u,0x1430u,0x14f0u,0x1582u,0x15f6u,0x1854u,0x2218u}){state->pc=entry;
        if(panel.step(*state)||panel.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial panel input recovery");}
    ks::StateEditAddressCalls stateEditAddressCalls;
    for(unsigned entry:{0x52aeu,0x52ccu,0x52feu}){state->pc=entry;
        if(stateEditAddressCalls.step(*state)||stateEditAddressCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial StateEditAddress recovery");}
    ks::StateStoreUiCalls stateStoreUiCalls;
    for(unsigned entry:{0x47ccu,0x47dau,0x4834u}){state->pc=entry;
        if(stateStoreUiCalls.step(*state)||stateStoreUiCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial StateStoreUi recovery");}
    ks::PanelMatrixCalls panelMatrixCalls;
    for(unsigned entry:{0x13fau,0x145au}){state->pc=entry;
        if(panelMatrixCalls.step(*state)||panelMatrixCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial PanelMatrix recovery");}
    ks::PedalSenseCalls pedalSenseCalls;
    for(unsigned entry:{0x13b0u}){state->pc=entry;
        if(pedalSenseCalls.step(*state)||pedalSenseCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial PedalSense recovery");}
    ks::PanelEncoderCalls panelEncoderCalls;
    for(unsigned entry:{0x188au}){state->pc=entry;
        if(panelEncoderCalls.step(*state)||panelEncoderCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial PanelEncoder recovery");}
    ks::PanelAdcSamplesCalls panelAdcSamplesCalls;
    for(unsigned entry:{0x16a8u}){state->pc=entry;
        if(panelAdcSamplesCalls.step(*state)||panelAdcSamplesCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial PanelAdcSamples recovery");}
    ks::PanelAdcScaleCalls panelAdcScaleCalls;
    for(unsigned entry:{0x1772u,0x17aau}){state->pc=entry;
        if(panelAdcScaleCalls.step(*state)||panelAdcScaleCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial PanelAdcScale recovery");}
    ks::PanelAdcCalibrationCalls panelAdcCalibrationCalls;
    for(unsigned entry:{0x1590u}){state->pc=entry;
        if(panelAdcCalibrationCalls.step(*state)||panelAdcCalibrationCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial PanelAdcCalibration recovery");}
    ks::PanelAdcIrqCalls panelAdcIrqCalls;
    for(unsigned entry:{0x165eu,0x16e6u,0x1798u,0x180eu}){state->pc=entry;
        if(panelAdcIrqCalls.step(*state)||panelAdcIrqCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial PanelAdcIrq recovery");}
    ks::PanelAdcScanCalls panelAdcScanCalls;
    for(unsigned entry:{0x1530u,0x1570u,0x1626u,0x164cu}){state->pc=entry;
        if(panelAdcScanCalls.step(*state)||panelAdcScanCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial PanelAdcScan recovery");}
    ks::InternalMidiQueueCalls internalMidiQueueCalls;
    for(unsigned entry:{0x2c32u}){state->pc=entry;
        if(internalMidiQueueCalls.step(*state)||internalMidiQueueCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial InternalMidiQueue recovery");}
    ks::MidiOutputCalls midiOutputCalls;
    for(unsigned entry:{0x2a5cu}){state->pc=entry;
        if(midiOutputCalls.step(*state)||midiOutputCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial MidiOutput recovery");}
    ks::InternalMidiServiceCalls internalMidiServiceCalls;
    for(unsigned entry:{0x2a1cu,0x2cceu}){state->pc=entry;
        if(internalMidiServiceCalls.step(*state)||internalMidiServiceCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial InternalMidiService recovery");}
    ks::StateForegroundCalls stateForegroundCalls;
    for(unsigned entry:{0x437cu,0x440au}){state->pc=entry;
        if(stateForegroundCalls.step(*state)||stateForegroundCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial StateForeground recovery");}
    ks::StateButtonDispatchCalls stateButtonDispatchCalls;
    for(unsigned entry:{0x44d0u}){state->pc=entry;
        if(stateButtonDispatchCalls.step(*state)||stateButtonDispatchCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial StateButtonDispatch recovery");}
    ks::PanelAnalogCalls panelAnalogCalls;
    for(unsigned entry:{0x1512u,0x20cau}){state->pc=entry;
        if(panelAnalogCalls.step(*state)||panelAnalogCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial PanelAnalog recovery");}
    ks::ClockSyncCalls clockSyncCalls;
    for(unsigned entry:{0x2680u}){state->pc=entry;
        if(clockSyncCalls.step(*state)||clockSyncCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial ClockSync recovery");}
    ks::ClockPulseCalls clockPulseCalls;
    for(unsigned entry:{0x284au}){state->pc=entry;
        if(clockPulseCalls.step(*state)||clockPulseCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial ClockPulse recovery");}
    ks::ClockDisplayCalls clockDisplayCalls;
    for(unsigned entry:{0x29aau}){state->pc=entry;
        if(clockDisplayCalls.step(*state)||clockDisplayCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial ClockDisplay recovery");}
    ks::StateScreenCalls stateScreenCalls;
    for(unsigned entry:{0x582cu}){state->pc=entry;
        if(stateScreenCalls.step(*state)||stateScreenCalls.partialEntries||state->cycles!=123||state->pc!=entry)throw std::runtime_error("Production exposed partial StateScreen recovery");}
    state->interpreterAllowed=false;rejected=false;
    try{state->step();}catch(const std::logic_error& e){rejected=std::string(e.what()).find("after native KS handoff")!=std::string::npos;}
    if(!rejected || state->interpretedSteps)throw std::runtime_error("Native interpreter lock failed");
    std::cout<<"PASS: production KS has no comparison adapter, defaults to C++, rejects interpreter work without mutation; storage="<<sizeof(ks::ControllerCpu)<<" bytes, no generic device-map table\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
