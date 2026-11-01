#pragma once
#include "kstationControllerPeriodicDisplayRuntime.h"
namespace kstation {
class ControllerPeriodicDisplayServices final {
public:
 bool bindAll(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_)return false;if(!runtime_.bindVerifiedFirmware(d,image,size)||!mode_.bindVerifiedFirmware(d,image,size)||!clock_.bindVerifiedFirmware(d,image,size)||!bar_.bindVerifiedFirmware(d,image,size))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return runtime_.active();}bool start(){return bound_&&runtime_.start();}void reset(){runtime_.reset();mode_.reset();clock_.reset();bar_.reset();}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerLcdRuntime&lcd,const ControllerNumericDisplayRuntime&numeric,const ControllerDspTransferRuntime&dsp)const{return runtime_.nextClocks(d,irq,mode_,clock_,bar_,lcd,numeric,dsp);}
 ControllerPeriodicDisplayRuntime::Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerLcdRuntime&lcd,ControllerNumericDisplayRuntime&numeric,ControllerDspTransferRuntime&dsp,unsigned budget){return runtime_.advanceBefore(d,irq,mode_,clock_,bar_,lcd,numeric,dsp,budget);}
private:ControllerDeviceState*bound_=nullptr;ControllerPeriodicDisplayRuntime runtime_{};ControllerUiModeRuntime mode_{};ControllerClockTextRuntime clock_{};ControllerBarGraphRuntime bar_{};
};
}
