#pragma once
#include "kstationControllerForegroundRuntime.h"
#include "kstationControllerPeriodicUiRuntime.h"
#include "kstationControllerMainUiKeyRuntime.h"
#include "kstationControllerMainUiAdcRuntime.h"
namespace kstation {
// Main foreground UI broker. Unsupported tails remain explicit typed requests.
class ControllerUiServices final {
 using F=ControllerForegroundRuntime;using M=MainLoopTask;using P=ControllerPeriodicUiRuntime;using K=ControllerMainUiKeyRuntime;using A=ControllerMainUiAdcRuntime;
public:
 enum class Owner:uint8_t{None,Periodic,Key,Adc};enum class Route:uint8_t{Ready,External,Invalid};enum class Block:uint8_t{None,Inactive,Invalid,NeedsInterrupt,External};struct Request{Owner owner=Owner::None;P::Request periodic{};K::Request key{};A::Request adc{};};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindAll(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active())return false;if(!periodic_.bindVerifiedFirmware(d,image,size)||!key_.bindVerifiedFirmware(d,image,size)||!adc_.bindVerifiedFirmware(d,image,size))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return owner_!=Owner::None;}void reset(){periodic_.reset();key_.reset();adc_.reset();owner_=Owner::None;}
 Route routeForeground(F&fg,ControllerDeviceState&d,ControllerDspTransferRuntime&dsp){if(bound_!=&d||!fg.active()||!dsp.boundTo(d))return Route::Invalid;if(owner_==Owner::None){auto q=fg.request();if(q.kind!=F::RequestKind::MainChild)return Route::External;bool ok=false;switch(q.child){case M::Child::PeriodicPanel:owner_=Owner::Periodic;ok=periodic_.start();break;case M::Child::ButtonQueue:owner_=Owner::Key;ok=key_.start();break;case M::Child::ControlQueue:owner_=Owner::Adc;ok=adc_.start();break;default:return Route::External;}if(!ok){owner_=Owner::None;return Route::Invalid;}}if(owner_==Owner::Periodic&&periodic_.request().service==P::Service::Clock){if(!periodic_.beginClock(d,dsp))return Route::Invalid;}return externalRequest().owner==Owner::None?Route::Ready:Route::External;}
 Request externalRequest()const{if(owner_==Owner::Periodic){auto q=periodic_.request();if(q.service!=P::Service::None)return {owner_,q};}else if(owner_==Owner::Key){auto q=key_.request();if(q.service!=K::Service::None)return {owner_,{},q};}else if(owner_==Owner::Adc){auto q=adc_.request();if(q.service!=A::Service::None)return {owner_,{},{},q};}return {};}
 bool completeExternal(F&fg,const Request&r,uint8_t value=0){if(r.owner!=owner_||owner_==Owner::None)return false;bool ok=owner_==Owner::Periodic?periodic_.completeService(r.periodic,value):owner_==Owner::Key?key_.completeService(r.key.service):adc_.completeService(r.adc.service,value);if(!ok)return false;bool alive=owner_==Owner::Periodic?periodic_.active():owner_==Owner::Key?key_.active():adc_.active();return alive||finish(fg);}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{if(bound_!=&d||owner_==Owner::None)return 0;return owner_==Owner::Periodic?periodic_.nextClocks(d,irq):owner_==Owner::Key?key_.nextClocks(d,irq):adc_.nextClocks(d,irq);}
 Result advanceBefore(F&fg,ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned budget){if(bound_!=&d)return {0,false,Block::Invalid};if(owner_==Owner::None)return {0,false,Block::Inactive};unsigned clocks=0;bool done=false;if(owner_==Owner::Periodic){auto r=periodic_.advanceBefore(d,irq,budget);clocks=r.clocks;done=r.returned;}else if(owner_==Owner::Key){auto r=key_.advanceBefore(d,irq,budget);clocks=r.clocks;done=r.returned;}else{auto r=adc_.advanceBefore(d,irq,budget);clocks=r.clocks;done=r.returned;}if(done){if(!finish(fg))return {clocks,false,Block::Invalid};return {clocks,true,Block::None};}return {clocks,false,Block::None};}
private:bool finish(F&fg){auto child=owner_==Owner::Periodic?M::Child::PeriodicPanel:owner_==Owner::Key?M::Child::ButtonQueue:M::Child::ControlQueue;owner_=Owner::None;return fg.completeMain(child);}ControllerDeviceState*bound_=nullptr;Owner owner_=Owner::None;P periodic_{};K key_{};A adc_{};
};
}
