#pragma once
#include "ksNativeMidiControlService.h"
#include "ksNativeMidiAuxService.h"
#include "ksFieldParameterContextBindings.h"
namespace ks {namespace nativeMidiParameterContextBindings {
inline uint16_t dsp(const ControllerDspService&s,uint16_t prior){return s.summary().wordTransmitted?uint16_t(s.summary().word>>8):prior;}
inline uint16_t modulation(const NativeVoiceModulationService&s,uint16_t prior){const auto&v=s.summary();if(v.mirrorObserved)prior=1;if(const auto*out=s.parameterOutput()){if(const auto*t=s.modulation();t&&t->complete())prior=uint16_t(t->converted()>>16);else if(const auto*t=s.pitch();t&&t->complete())prior=uint16_t(t->pitchProduct()>>16);prior=dsp(*out,prior);}if(s.global()&&s.global()->complete())prior=uint16_t(s.lastOutput()->summary().word>>8);return prior;}
inline uint16_t sweep(const NativeVoiceSweepService&s,uint16_t prior){if(s.summary().mirrorObserved)prior=1;if(s.lastSelect())prior=dsp(*s.lastSelect(),prior);if(s.lastCut()){prior=s.sweep().retune()?uint16_t(s.lastCut()->summary().word>>8):uint16_t(0x1000);prior=dsp(*s.lastCut(),prior);}if(s.lastGate())prior=dsp(*s.lastGate(),0);return prior;}
inline uint16_t release(const NativeVoiceReleaseService&s,uint16_t prior){return s.releasedAny()?0:prior;}
inline uint16_t edit(const NativeParameterEditService&s,uint16_t prior){const auto&f=s.summary();if(f.entry==NativeParameterEditService::Entry::Internal)prior=0;else if(f.map){const auto kind=f.map->summary().map->outcome();if(kind==StateFieldMapTask::Outcome::Unmapped||kind==StateFieldMapTask::Outcome::Packed)prior=0;}if(f.displayed)prior=fieldUiParameterContextUpperAfter(*s.displayOutcome(),prior);return prior;}
inline uint16_t pedal(const NativePedalControlService&s,uint16_t prior){if(!s.summary().parameter)prior=uint16_t(s.pedalTask()->patchReference()>>16);if(s.summary().edited)prior=edit(*s.editOutcome(),prior);if(s.summary().released)prior=release(*s.summary().release,prior);return prior;}
inline uint16_t control(const NativeMidiControlService&s,uint16_t prior){if(s.summary().modulated)prior=modulation(*s.modulationOutcome(),prior);if(s.summary().swept)prior=sweep(*s.sweepOutcome(),prior);if(s.summary().released)prior=release(*s.releaseOutcome(),prior);if(s.summary().pedaled)prior=pedal(*s.pedalOutcome(),prior);return prior;}
inline uint16_t global(const NativeMidiAuxService&s,uint16_t prior){if(s.summary().kind!=NativeMidiAuxService::Kind::GlobalControl)throw std::logic_error("Unexpected packed-field global route child");return s.summary().parameter?pedal(*s.parameterOutcome(),prior):prior;}
}
}
