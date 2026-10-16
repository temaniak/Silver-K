#include "ksControllerState.h"
#include "ksDevice.h"
#include "ksKeySearchCalls.h"
#include "ksParameterScaleCalls.h"
#include "ksControllerJobs.h"
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
    state->interpreterAllowed=false;rejected=false;
    try{state->step();}catch(const std::logic_error& e){rejected=std::string(e.what()).find("after native KS handoff")!=std::string::npos;}
    if(!rejected || state->interpretedSteps)throw std::runtime_error("Native interpreter lock failed");
    std::cout<<"PASS: production KS has no comparison adapter, defaults to C++, rejects interpreter work without mutation; storage="<<sizeof(ks::ControllerCpu)<<" bytes, no generic device-map table\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
