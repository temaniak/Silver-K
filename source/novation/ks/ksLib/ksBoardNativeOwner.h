#pragma once
#include "ksControllerDeviceState.h"
#include <memory>
namespace ks {
class NativeStartupController;class NativeControllerExecutor;
// Physical Board edge composes real typed startup and Main/IRQ owners.
// Private candidate: actual full-cold fidelity remains an explicit pending gate.
class BoardNativeOwner final {
public:
 BoardNativeOwner(ControllerDeviceState&,const uint8_t*checksumAppendedOs,unsigned size);
 ~BoardNativeOwner();
 BoardNativeOwner(const BoardNativeOwner&)=delete;
 BoardNativeOwner&operator=(const BoardNativeOwner&)=delete;
 unsigned nextClocks(const ControllerDeviceState&)const;
 unsigned advanceBefore(ControllerDeviceState&,unsigned available);
 bool mainEntered()const;
 bool ready()const;
 bool protectedCode()const;
 void resetMetrics();
 const NativeStartupController&startup()const;
 const NativeControllerExecutor&playback()const;
 bool playbackPublished()const;
 uint64_t startupPhasesSinceReset()const;
private:
 struct Impl;std::unique_ptr<Impl>impl_;
};
}
