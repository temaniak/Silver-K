#include "ksLiveVoiceController.h"
#include <stdexcept>

namespace ks {
void LiveVoiceController::verify(const ControllerCpu& h){
    uint64_t hash=14695981039346656037ull;
    for(auto range:{std::pair<unsigned,unsigned>{0x2416,0x25fe},{0x77dc,0x7872},
                   {0x8fc0,0x9374},{0xa1b0,0xa338},{0xa338,0xa656},{0xa67e,0xa69e}})
        for(unsigned n=range.first;n<range.second;++n){hash^=h.memory[n];hash*=1099511628211ull;}
    if(hash!=0x418737a410bf4738ull)throw std::runtime_error("Native KS voice lifecycle requires verified OS 2.1.02");
}
bool LiveVoiceController::step(ControllerCpu& h){
    const unsigned pc=h.getPC();
    if(pc>=0xa67e && PartControlCalls::candidate(pc)){if(!partControlCalls.step(h))return false;++steps;return true;}
    if(VoiceSelectionCalls::candidate(pc)){if(!selectionCalls.step(h))return false;++steps;if(pc==0xa338)++allocationCalls;if(pc==0x8fc0)++releaseCalls;return true;}
    if(VelocityModulationCalls::candidate(pc)&&modulationCalls.step(h)){++steps;return true;}
    if(GlideCalls::candidate(pc)&&glideCalls.step(h)){++steps;return true;}
    if(VoiceStartCalls::candidate(pc)){if(!startCallsNative.step(h))return false;++steps;return true;}
    if(VoiceTrackingCalls::candidate(pc)){if(!trackingCalls.step(h))return false;++steps;return true;}
    if(UnisonCalls::candidate(pc)){if(!unisonCalls.step(h))return false;++steps;if(pc==0x8fcc)++startCalls;return true;}
    if(VoicePopulationCalls::candidate(pc)){if(!populationCalls.step(h))return false;++steps;return true;}
    if(VelocityCurveCalls::candidate(pc)){if(!velocityCurveCalls.step(h))return false;++steps;return true;}
    if(DspRoutingCalls::candidate(pc)){
        if(!routingCalls.step(h))return false;
        ++steps;if(pc<0x25fe)++transportSteps;return true;
    }
    if(DspSelectCalls::candidate(pc)){
        if(!selectCalls.step(h))return false;
        ++steps;++transportSteps;return true;
    }
    if(DspAddressCalls::candidate(pc)){
        if(!addressCalls.step(h))return false;
        ++steps;++transportSteps;return true;
    }
    if(DspWordCalls::candidate(pc)){
        if(!wordCalls.step(h))return false;
        ++steps;++transportSteps;return true;
    }
    return false;
}
}
