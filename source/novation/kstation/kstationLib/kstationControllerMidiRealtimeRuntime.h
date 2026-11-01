#pragma once
#include "kstationControllerDspTriggerRuntime.h"
namespace kstation {
// System-event handler and ignored MIDI status. Each timed interval follows
// the verified service contract; caller context is represented only as time.
class ControllerMidiRealtimeRuntime final {
 enum class Phase:uint8_t{Inactive,Call,Retain,Mode,Trigger,Return};
public:
 struct Result{unsigned clocks=0;bool returned=false;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(6))return false;for(auto r:{std::pair{0x33ecu,0x33f4u},std::pair{0x825cu,0x8260u}})for(auto a=r.first;a<r.second;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!trigger_.bindVerifiedFirmware(d,image,size))return false;for(auto r:{std::pair{0x33ecu,0x33f4u},std::pair{0x825cu,0x8260u}})if(!d.protectImmutableVerified(r.first,r.second,image+r.first)||!d.protectImmutableVerified(0x400000+r.first,0x400000+r.second,image+r.first))return false;bound_=&d;return true;}
 bool active()const{return phase_!=Phase::Inactive;}bool start(bool ignored=false){if(!bound_||active())return false;phase_=ignored?Phase::Return:Phase::Call;return true;}void reset(){phase_=Phase::Inactive;trigger_.reset();}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerDspTransferRuntime&dsp)const{if(bound_!=&d||!active()||!irq.boundTo(d)||!dsp.boundTo(d)||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;if(phase_==Phase::Trigger)return trigger_.nextClocks(d,irq,dsp);return (phase_==Phase::Call?10:phase_==Phase::Retain?6:phase_==Phase::Mode?3:7)+(d.pendingIrqs?1u:0u);}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerDspTransferRuntime&dsp,unsigned budget){auto n=nextClocks(d,irq,dsp);if(!n||budget<n)return {};if(phase_==Phase::Trigger){auto r=trigger_.advanceBefore(d,irq,dsp,budget);if(r.returned)phase_=Phase::Return;return {r.clocks,false};}if(d.pendingIrqs)(void)d.read8(0xfffff2);d.clockInternal(n-(d.pendingIrqs?1u:0u));d.lastRead=-1;if(phase_==Phase::Call||phase_==Phase::Retain)d.lastWrite=-1;if(phase_==Phase::Mode){if(!trigger_.start(2))throw std::logic_error("K system event trigger admission");phase_=Phase::Trigger;}else if(phase_==Phase::Return)phase_=Phase::Inactive;else phase_=Phase(unsigned(phase_)+1);return {n,!active()};}
private:ControllerDeviceState*bound_=nullptr;Phase phase_=Phase::Inactive;ControllerDspTriggerRuntime trigger_{};
};
}
