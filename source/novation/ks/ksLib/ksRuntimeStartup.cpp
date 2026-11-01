#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"
namespace ks {
// Reset and image/overlay lifecycle use explicit native state and service jobs.
// Original CPU projections remain temporarily in adapters for exact comparison.
bool RuntimeController::startup(ControllerCpu&h){
 if(HardwareSetupCalls::candidate(unsigned(h.getPC()))&&shared.hardwareSetupCalls.step(h))return true;
 return ResetImageCalls::candidate(unsigned(h.getPC()))&&shared.resetImageCalls.step(h);
}}
