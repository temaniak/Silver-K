#pragma once
#include "ksNativeVoiceModulationService.h"
#include "ksNativeVoiceSweepService.h"
namespace ks {
inline uint32_t voiceModulationExpressionSourceAfter(const NativeVoiceModulationService&s,uint32_t prior){if(!s.complete())throw std::logic_error("Incomplete modulation expression binding");if(const auto*out=s.parameterOutput();out&&out->summary().selectedPort)prior=out->summary().port==DspPort::A?0x400000u:0x600000u;if(s.lastOutput())prior=s.summary().port==DspPort::A?0x400000u:0x600000u;return prior;}
inline uint32_t voiceSweepExpressionSourceAfter(const NativeVoiceSweepService&s,uint32_t prior){if(!s.complete())throw std::logic_error("Incomplete sweep expression binding");for(const auto*out:{s.lastSelect(),s.lastCut(),s.lastGate()})if(out&&out->summary().selectedPort)prior=out->summary().port==DspPort::A?0x400000u:0x600000u;if(s.lastCut())prior=s.summary().port==DspPort::A?0x400000u:0x600000u;return prior;}
inline uint8_t voiceModulationActivationAfter(const NativeVoiceModulationService&s,uint8_t prior){if(!s.complete())throw std::logic_error("Incomplete modulation activation binding");if(s.parameterOutput()){if(const auto*t=s.modulation();t&&t->complete())prior=uint8_t(uint16_t(t->factor())>>8);else if(const auto*t=s.pitch();t&&t->complete()&&t->unisonCount())prior=int8_t(t->spreadValue())<0?255:0;}if(s.global()&&s.global()->complete())prior=uint8_t(uint16_t(s.lastOutput()->summary().input.address-0x1b)>>8);return prior;}
inline uint8_t voiceSweepActivationAfter(const NativeVoiceSweepService&s,uint8_t prior){if(!s.complete())throw std::logic_error("Incomplete sweep activation binding");if(!s.sweep().retune())prior=uint8_t((s.summary().lastInspectionReleased?0xffffu:s.summary().lastInspectedAge)>>8);return prior;}
}
