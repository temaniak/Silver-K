#pragma once
#include <cstdint>
#include "ksMidiDescriptorTask.h"
#include <stdexcept>
namespace ks {
// A finite capability decoded from authenticated firmware descriptor metadata.
// None of these values is an execution address or a generic call target.
enum class NativeMidiHandler{PartOff,PartOn,Control,Program,Pressure,Pitch,BeginSysEx,EndSysEx,ManualPeriodic,Noop,InternalOff,InternalOn,ConditionalProgram,GlobalControl};
inline NativeMidiHandler nativeMidiHandler(uint32_t descriptor,bool internal=false){
 switch(descriptor&0xffffffu){
 case 0x33b8:if(!internal)return NativeMidiHandler::PartOff;break;
 case 0x3426:if(!internal)return NativeMidiHandler::PartOn;break;
 case 0xa894:if(internal)return NativeMidiHandler::InternalOff;break;
 case 0xa882:if(internal)return NativeMidiHandler::InternalOn;break;
 case 0x34bc:return NativeMidiHandler::Control;case 0x3600:return NativeMidiHandler::Program;case 0x34b2:return NativeMidiHandler::Pressure;case 0x349c:return NativeMidiHandler::Pitch;
 case 0x35f4:return NativeMidiHandler::BeginSysEx;case 0x35fa:return NativeMidiHandler::EndSysEx;case 0x363c:return NativeMidiHandler::ManualPeriodic;case 0x3640:return NativeMidiHandler::Noop;
 default:break;
 }throw std::logic_error("Unsupported MIDI descriptor capability");
}
inline uint8_t nativeMidiDescriptorLength(MidiDescriptorKey k){if(k.family==MidiDescriptorKey::Family::Channel){if(k.index>=7)throw std::logic_error("Invalid channel MIDI descriptor");return k.index==4||k.index==5?1:2;}switch(k.index){case 0:case 1:case 3:return 1;case 2:return 2;default:return 0;}}
inline uint8_t nativeMidiDescriptorFlags(MidiDescriptorKey k){return k.family==MidiDescriptorKey::Family::Channel?1:k.index==0?1:k.index>=8?2:0;}
inline NativeMidiHandler nativeMidiDescriptorHandler(MidiDescriptorKey k){using H=NativeMidiHandler;if(k.family==MidiDescriptorKey::Family::Channel){constexpr H kinds[]{H::PartOff,H::PartOn,H::Noop,H::Control,H::Program,H::Pressure,H::Pitch};if(k.index>=7)throw std::logic_error("Invalid channel MIDI capability");return kinds[k.index];}return k.index==0?H::BeginSysEx:k.index==7?H::EndSysEx:k.index==10?H::ManualPeriodic:H::Noop;}

}
