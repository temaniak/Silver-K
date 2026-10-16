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
    state->interpreterAllowed=false;rejected=false;
    try{state->step();}catch(const std::logic_error& e){rejected=std::string(e.what()).find("after native KS handoff")!=std::string::npos;}
    if(!rejected || state->interpretedSteps)throw std::runtime_error("Native interpreter lock failed");
    std::cout<<"PASS: production KS has no comparison adapter, defaults to C++, rejects interpreter work without mutation; storage="<<sizeof(ks::ControllerCpu)<<" bytes, no generic device-map table\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
