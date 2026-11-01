#pragma once
#include "ksServiceCycleService.h"
#include "ksServiceCycleBindings.h"
#include "ksServiceCycleCompatibility.h"
#include "ksServiceCycleResolver.h"
#include "ksNativeArpOwnedService.h"
#include "ksNativeArpOwnedCompatibility.h"
#include "ksMidiFileTransportService.h"
#include "ksMidiFileTransportCompatibility.h"
namespace ks {
class ControllerCpu;
using NativeServiceCycle=ServiceCycleService<ScheduledTimerService,NativeMidiPumpService,NativeArpOwnedService,MidiFileTransportService>;
using NativeServiceCycleBindings=ServiceCycleBindings<NativeServiceCycle,NativeArpOwnedService,MidiFileTransportService>;
using NativeServiceCycleCompatibility=ServiceCycleCompatibility<NativeServiceCycle,NativeArpOwnedService,NativeArpOwnedCompatibility,MidiFileTransportService,MidiFileTransportCompatibility>;
class ServiceCycleForeground final {
public:
 bool step(ControllerCpu&);
 void bindMetadata(const uint8_t*verified){if(job_)throw std::logic_error("Cannot replace active main metadata");metadata_.assign(verified);metadataReady_=true;}
 bool active()const{return bool(job_);}const NativeServiceCycle*service()const{return job_?&job_->service:nullptr;}
 void resetMetrics(){const bool activeTick=service()&&service()->current<StateTickService>();initial=active()?1:0;started=completed=phases=0;children.fill(0);tickReturns={};tickReturns.initial=activeTick?1:0;}
 // These counters belong to actual Main-owned Tick admissions and returns.
 struct TickReturns{uint64_t initial=0,started=0,completed=0,clock=0,buttons=0,adc=0,encoder=0,pedal=0,output=0,refresh=0;uint32_t components=0;};
 TickReturns tickReturns{};
 uint64_t initial=0,started=0,completed=0,phases=0;std::array<uint64_t,17>children{};
private:
 struct Job final {NativeServiceCycle service;NativeServiceCycleCompatibility external;uint32_t caller;int depth;uint8_t priority;template<class A>Job(NativeServiceCycle::Input input,uint32_t sp,int d,uint8_t p,const A&a):service(input),external(a),caller(sp),depth(d),priority(p){}};
 std::optional<Job>job_;ServiceCycleResolver::Metadata metadata_{};bool metadataReady_=false;
};
}
