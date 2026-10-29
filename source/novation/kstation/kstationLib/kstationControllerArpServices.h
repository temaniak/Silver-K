#pragma once
#include "kstationControllerArpRuntime.h"
#include "kstationControllerArpStepRuntime.h"
#include "kstationControllerArpPitchRuntime.h"
#include "kstationControllerArpEmitRuntime.h"
#include "kstationControllerArpScheduleRuntime.h"
#include "kstationControllerArpGateRuntime.h"
#include "kstationControllerArpOutputRuntime.h"
#include "kstationControllerArpTriggerRuntime.h"
#include "kstationControllerSustainReleaseRuntime.h"
namespace kstation {
// Bounded composition for one foreground 3446 invocation. The DSP and IRQ
// owners remain shared with the controller and are never emulated here.
class ControllerArpServices final {
public:
 enum class Route:uint8_t{Ready,Invalid};
 bool bindAll(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_)return false;if(!root_.bindVerifiedFirmware(d,image,size)||!step_.bindVerifiedFirmware(d,image,size)||!pitch_.bindVerifiedFirmware(d,image,size)||!emit_.bindVerifiedFirmware(d,image,size)||!schedule_.bindVerifiedFirmware(d,image,size)||!gate_.bindVerifiedFirmware(d,image,size)||!output_.bindVerifiedFirmware(d,image,size)||!trigger_.bindVerifiedFirmware(d,image,size)||!voice_.bindVerifiedFirmware(d,image,size)||!release_.bindVerifiedFirmware(d,image,size)||!voices_.bindVerifiedFirmware(d,image,size)||!dspTrigger_.bindVerifiedFirmware(d,image,size))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return root_.active();}bool start(){return bound_&&root_.start();}void reset(){root_.reset();step_.reset();pitch_.reset();emit_.reset();schedule_.reset();gate_.reset();output_.reset();trigger_.reset();voice_.reset();release_.reset();voices_.reset();dspTrigger_.reset();}
 Route route(ControllerDeviceState&d,ControllerDspTransferRuntime&dsp){if(bound_!=&d||!dsp.boundTo(d)||!active())return Route::Invalid;auto r=root_.request();if(r.waiting){using C=ArpMainTask::Child;if(r.child==C::Gate)return root_.beginGate(gate_,d)?Route::Ready:Route::Invalid;if(r.child==C::Step)return root_.beginStep(step_,d)?Route::Ready:Route::Invalid;if(r.child==C::ReleaseVoices)return root_.beginReleaseVoices(voices_,dsp,d)?Route::Ready:Route::Invalid;return Route::Invalid;}auto st=step_.request();if(st.waiting){return (st.child==ArpStepTask::Child::Emit?step_.beginEmit(emit_,d):step_.beginPitch(pitch_,d))?Route::Ready:Route::Invalid;}if(pitch_.request().waiting)return pitch_.beginEmit(emit_,d)?Route::Ready:Route::Invalid;auto em=emit_.request();if(em.waiting){using C=ArpEmitTask::Child;return (em.child==C::Schedule?emit_.beginSchedule(schedule_,d):em.child==C::SendOutput?emit_.beginOutput(output_,d):em.child==C::StartVoice?emit_.beginTrigger(trigger_,d):false)?Route::Ready:Route::Invalid;}if(schedule_.awaitingGate())return schedule_.beginGate(gate_,d)?Route::Ready:Route::Invalid;auto ga=gate_.request();if(ga.waiting){return (ga.child==ArpGateTask::Child::SendOutput?gate_.beginOutput(output_,d):ga.child==ArpGateTask::Child::Release?gate_.beginRelease(release_,dsp,d):false)?Route::Ready:Route::Invalid;}if(trigger_.awaitingVoice())return trigger_.beginVoice(voice_,dsp,d)?Route::Ready:Route::Invalid;if(trigger_.awaitingTail())return trigger_.beginTail(dspTrigger_,dsp,d)?Route::Ready:Route::Invalid;return Route::Ready;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{return root_.nextClocks(d,irq);}ControllerArpRuntime::Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned budget){return root_.advanceBefore(d,irq,budget);}
private:ControllerDeviceState*bound_=nullptr;ControllerArpRuntime root_{};ControllerArpStepRuntime step_{};ControllerArpPitchRuntime pitch_{};ControllerArpEmitRuntime emit_{};ControllerArpScheduleRuntime schedule_{};ControllerArpGateRuntime gate_{};ControllerArpOutputRuntime output_{};ControllerArpTriggerRuntime trigger_{};ControllerArpVoiceStartRuntime voice_{};ControllerMidiVoiceReleaseRuntime release_{};ControllerProgramVoicesRuntime voices_{};ControllerDspTriggerRuntime dspTrigger_{};
};
}
