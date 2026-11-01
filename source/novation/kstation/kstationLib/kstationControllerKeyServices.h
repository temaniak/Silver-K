#pragma once
#include "kstationControllerKeyRuntime.h"
#include "kstationControllerKeyPlaybackRuntime.h"
namespace kstation {
class ControllerKeyServices final {
public:
 enum class Route:uint8_t{Ready,Invalid};struct Result{unsigned clocks=0;bool returned=false;bool blocked=false;};
 bool bindAll(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_)return false;if(!key_.bindVerifiedFirmware(d,image,size)||!route_.bindVerifiedFirmware(d,image,size)||!byte_.bindVerifiedFirmware(d,image,size)||!playback_.bindVerifiedFirmware(d,image,size))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return key_.active()||playback_.active();}bool startKey(){return bound_&&!active()&&key_.start();}bool startPlayback(){return bound_&&!active()&&playback_.start();}void reset(){key_.reset();route_.reset();byte_.reset();playback_.reset();}
 Route route(ControllerDeviceState&d){if(bound_!=&d||!active())return Route::Invalid;if(key_.request().waiting)return key_.beginRoute(route_,byte_,d)?Route::Ready:Route::Invalid;return Route::Ready;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerDspTransferRuntime&dsp)const{if(bound_!=&d||!dsp.boundTo(d))return 0;return key_.active()?key_.nextClocks(d,irq):playback_.active()?playback_.nextClocks(d,irq,byte_,dsp):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerDspTransferRuntime&dsp,unsigned budget){if(bound_!=&d||!dsp.boundTo(d)||!active())return {0,false,true};if(key_.active()){auto r=key_.advanceBefore(d,irq,budget);return {r.clocks,r.returned,r.blocked!=ControllerKeyRuntime::Block::None};}auto r=playback_.advanceBefore(d,irq,byte_,dsp,budget);return {r.clocks,r.returned,r.blocked!=ControllerKeyPlaybackRuntime::Block::None};}
private:ControllerDeviceState*bound_=nullptr;ControllerKeyRuntime key_{};ControllerKeyMidiRouteRuntime route_{};ControllerKeyMidiByteRuntime byte_{};ControllerKeyPlaybackRuntime playback_{};
};
}
