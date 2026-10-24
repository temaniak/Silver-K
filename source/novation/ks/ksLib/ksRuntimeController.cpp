#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace ks {
RuntimeController::RuntimeController(const ControllerCpu& h,LiveMidiController& state,Entry entry):shared(state){
    uint64_t hash=14695981039346656037ull;
    for(unsigned n=0;n<87360;++n){hash^=h.memory[n];hash*=1099511628211ull;}
    if(hash!=0xbd1afefba93cb7b9ull)throw std::runtime_error("Native KS runtime requires verified OS 2.1.02");
    if(entry==Entry::PostBoot)verifyImage(h);
#if defined(SILVER_KS_CONTROLLER_PROFILING)
    const auto* profile=std::getenv("SILVER_KS_CONTROLLER_PROFILE");
    if(profile && std::strcmp(profile,"1")==0)boundaryProfile.assign(0x10000,0);
#endif
}
void RuntimeController::verifyImage(const ControllerCpu& h){
    // H8 boots from Flash into SRAM. Verify the actual executable image too;
    // ROM identity alone would not establish an unchanged handoff image.
    for(auto range:{std::pair<unsigned,unsigned>{0x170,0x172},{0x490,0x2a0c},{0x2a18,0xb8e8}})
        for(unsigned pc=range.first;pc<range.second;++pc)
            if(h.memory[0xa00000+pc]!=h.memory[pc])throw std::runtime_error("Native KS handoff code image differs from verified ROM");
}
void RuntimeController::arbitrate(ControllerCpu& h){
    if(h.interruptMode2 && (h.pending_irqs || h.pending_irqs_high)){
        int selected=-1,priority=h.exr&7;
        for(int base=0;base<=64;base+=64){auto bits=base?h.pending_irqs_high:h.pending_irqs;
            while(bits){unsigned n=0;auto word=bits;while(!(word&1)){++n;word>>=1;}
                if(h.interruptPriority[base+n]>priority){selected=int(base+n);priority=h.interruptPriority[selected];}
                bits&=bits-1;
            }
        }
        if(selected>=0){
            if(selected<64)h.pending_irqs&=~(1ull<<selected);else h.pending_irqs_high&=~(1ull<<(selected-64));
            h.pushPC(h.pc,true);h.pushW(uint16_t(h.exr)<<8);h.exr=uint8_t(priority);h.clockI(4);
            h.pc=h.loadPCFrom(unsigned(selected)<<2);++h.indent;
        }
    }
    if(!h.interruptMode2 && h.pending_irqs){
        const bool user=h.read8(0xfffff2)&8;
        if(!(h.ccr&128)){
            unsigned vector=0;while(!(h.pending_irqs&(1ull<<vector)))++vector;
            h.pending_irqs^=1ull<<vector;h.pushPC(h.pc,true);h.clockI(4);h.ccr|=128;if(!user)h.ccr|=16;
            h.pc=h.loadPCFrom(vector<<2);++h.indent;
        }
    }
    if(h.getPC()&1)--h.pc;
}
bool RuntimeController::stepCode(ControllerCpu& h){
    shared.stateGlobalSaveCalls.retireReturns(h);
    shared.stateFactoryServicesCalls.retireReturns(h);
    shared.statePresetFormatCalls.retireReturns(h);
    shared.stateStoreServicesCalls.retireReturns(h);
    shared.stateMultiSaveCalls.retireReturns(h);
    shared.stateAuditionCalls.retireReturns(h);
    shared.stateNameEditorCalls.retireReturns(h);
    shared.stateBankDisplayCalls.retireReturns(h);
    shared.stateManualEditCalls.retireReturns(h);
    shared.stateStoreUiCalls.retireReturns(h);
    const unsigned pc=h.getPC();bool handled=false;
    if(VoiceReleaseCalls::candidate(pc))handled=shared.noteActions.step(h);
    else if(pc<0x2de8)handled=io(h);
    else if(pc<0x4000)handled=midi(h);
    else if(pc<0x77dc)handled=state(h);
    else if(pc<0x9374)handled=parameters(h);
    else if(pc<0xa69e)handled=voices(h);
    else if(pc<0xb8e8)handled=extended(h);
    if(!handled)return false;
    ++steps;
#if defined(SILVER_KS_CONTROLLER_PROFILING)
    if(!boundaryProfile.empty())++boundaryProfile[pc];
#endif
    if(pc>=0x2de8 && pc<0x2efc)++shared.nativeSteps;
    if(pc==0x2de8)++shared.bytes;
    if(pc==0x2ecc){
        ++shared.messages;const auto kind=h.memory[0xa15c1b]&0xf0;
        if(kind==0xb0)++shared.controls;
        else if(kind==0x80 || (kind==0x90 && !h.memory[0xa15c20]))++shared.noteOff;
        else if(kind==0x90)++shared.noteOn;
    }
    return true;
}
void RuntimeController::stepStartup(ControllerCpu& h){
    if(serviceInterrupt(h)){++steps;return;}
    const int depth=h.indent;arbitrate(h);if(h.indent!=depth)++interruptEntries;
    if(serialForeground.step(h)){++steps;return;}
    if(stateResetForeground.step(h)){++steps;return;}
    if(stateProgramLoadForeground.step(h)){++steps;return;}
    // Close a legacy Store UI tail before native playback owns its target.
    if(!statePlaybackModeForeground.active()&&h.getPC()==0x48be)shared.stateStoreUiCalls.retireReturns(h);
    if(statePlaybackModeForeground.step(h)){++steps;return;}
    if(statePartNavigationForeground.step(h)){++steps;return;}
    if(stateEditActionForeground.step(h)){++steps;return;}
    if(stateSceneToggleForeground.step(h)){++steps;return;}
    if(stateSceneEditForeground.step(h)){++steps;return;}
    if(stateVoiceRefreshForeground.step(h)){++steps;return;}
    if(globalUpdateForeground.step(h)){++steps;return;}
    if(parameterUploadForeground.step(h)){++steps;return;}
    if(stateSceneForeground.step(h)){++steps;return;}
    if(stateFieldForeground.step(h)){++steps;return;}
    if(stateScreenForeground.step(h)){++steps;return;}
    if(panelSettingsForeground.step(h)){++steps;return;}
    if(stateSaveForeground.step(h)){++steps;return;}
    if(stateApplyForeground.step(h)){++steps;return;}
    if(stateDisplayForeground.step(h)){++steps;return;}
    if(stateStoreForeground.step(h)){++steps;return;}
    if(stateMenuForeground.step(h)){++steps;return;}
    if(statePanelForeground.step(h)){++steps;return;}
    if(stateEventsForeground.step(h)){++steps;return;}
    if(panelControlsForeground.step(h)){++steps;return;}
    if(panelScanForeground.step(h)){++steps;return;}
    if(startup(h)){++steps;return;}
    if(stepCode(h))return;
    throw std::runtime_error("Unsupported native KS reset state (no H8 fallback)");
}
void RuntimeController::step(ControllerCpu& h){
    if(serviceInterrupt(h)){++steps;return;}
    const int depth=h.indent;arbitrate(h);if(h.indent!=depth)++interruptEntries;
    if(serialForeground.step(h)){++steps;return;}
    if(stateResetForeground.step(h)){++steps;return;}
    if(stateProgramLoadForeground.step(h)){++steps;return;}
    // Close a legacy Store UI tail before native playback owns its target.
    if(!statePlaybackModeForeground.active()&&h.getPC()==0x48be)shared.stateStoreUiCalls.retireReturns(h);
    if(statePlaybackModeForeground.step(h)){++steps;return;}
    if(statePartNavigationForeground.step(h)){++steps;return;}
    if(stateEditActionForeground.step(h)){++steps;return;}
    if(stateSceneToggleForeground.step(h)){++steps;return;}
    if(stateSceneEditForeground.step(h)){++steps;return;}
    if(stateVoiceRefreshForeground.step(h)){++steps;return;}
    if(globalUpdateForeground.step(h)){++steps;return;}
    if(parameterUploadForeground.step(h)){++steps;return;}
    if(stateSceneForeground.step(h)){++steps;return;}
    if(stateFieldForeground.step(h)){++steps;return;}
    if(stateScreenForeground.step(h)){++steps;return;}
    if(panelSettingsForeground.step(h)){++steps;return;}
    if(stateSaveForeground.step(h)){++steps;return;}
    if(stateApplyForeground.step(h)){++steps;return;}
    if(stateDisplayForeground.step(h)){++steps;return;}
    if(stateStoreForeground.step(h)){++steps;return;}
    if(stateMenuForeground.step(h)){++steps;return;}
    if(statePanelForeground.step(h)){++steps;return;}
    if(stateEventsForeground.step(h)){++steps;return;}
    if(panelControlsForeground.step(h)){++steps;return;}
    if(panelScanForeground.step(h)){++steps;return;}
    if(stepCode(h))return;
    std::ostringstream message;message<<"Unsupported native KS controller state 0x"<<std::hex<<h.getPC()
        <<" (no H8 fallback; disable the experiment)";
    throw std::runtime_error(message.str());
}
}
