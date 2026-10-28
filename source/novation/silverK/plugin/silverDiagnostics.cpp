#include "silverProcessor.h"
#include "silverKEngine.h"
#include "kstationLib/kstationDSP.h"

#ifndef SILVER_DIAGNOSTICS_REVISION
#define SILVER_DIAGNOSTICS_REVISION "unknown"
#endif
#ifndef SILVER_DIAGNOSTICS_SOURCE_STATE
#define SILVER_DIAGNOSTICS_SOURCE_STATE "unknown"
#endif
#ifndef SILVER_DIAGNOSTICS_CONFIG
#define SILVER_DIAGNOSTICS_CONFIG "unknown"
#endif

namespace silverK
{
namespace
{
juce::DynamicObject* object(){return new juce::DynamicObject;}
juce::var integer(uint64_t v){return juce::var(static_cast<juce::int64>(v));}
juce::var placementInfo(const ks::WorkerPlacement* p){
    auto* info=object();juce::var value(info);
    info->setProperty("requested",p&&p->requested?"p":"default");
    info->setProperty("status",p?p->status:"not-applicable");
    info->setProperty("win32_error",p?int(p->error):0);
    juce::Array<juce::var> ids;if(p)for(auto id:p->cpuSets)ids.add(int(id));
    info->setProperty("selected_cpu_set_ids",juce::var(ids));return value;
}
}
bool Processor::setPerformanceDiagnostics(int mode)
{
    const auto patchLabel=mode>0?currentName():juce::String{};
    std::lock_guard lock(controlMutex);
    if(mode<=0)
    {
        if(performanceDiagnostics.mode())
        {
            performanceDiagnostics.stop();performanceStoppedMs=juce::Time::getMillisecondCounterHiRes();
            performanceStopReason="user_stopped";
        }
        return true;
    }
    if(!firmwareReady() || isFirmwareLoading())return false;
    auto* info=object();performanceSession=juce::var(info);
    info->setProperty("started_at",juce::Time::getCurrentTime().toISO8601(true));
    info->setProperty("mode",mode==2?"detailed":"summary");
    info->setProperty("model",isKStation()?"K-Station":"KS");
    info->setProperty("firmware_file",osFile.getFileName()); // no full personal paths or ROM contents
    info->setProperty("patch",patchLabel);
    info->setProperty("host_rate_hz",hostRate);
    info->setProperty("prepared_block_frames",hostBlock);
    info->setProperty("buffer_profile",bufferProfile());
    info->setProperty("plugin_latency_host_frames",getLatencySamples());
    info->setProperty("queue_rate_hz",isKStation()?kstation::DSP::SampleRate:hostRate);
    info->setProperty("queue_reserve_frames",ownedK?int(ownedK->device.getInternalLatencyMidiToOutput()):ownedDevice?int(ownedDevice->latency()):0);
    info->setProperty("native_rate_hz",isKStation()?kstation::DSP::SampleRate:ks::DSP::SampleRate);
    info->setProperty("performance_mode",performanceMode());
    info->setProperty("active_parts_mask",int(activeParts()));
    info->setProperty("wave_bank",isKStation()?"not_applicable":placeholder?"zero":"external");
    info->setProperty("worker_wait_backend",ownedK?ownedK->device.workerWaitBackend():ownedDevice->workerWaitBackend());
    info->setProperty("worker_placement",placementInfo(ownedDevice?&ownedDevice->workerPlacement():nullptr));
    info->setProperty("dsp_execution_backend",ownedDevice?ownedDevice->dspExecutionBackend():"not-applicable");
    info->setProperty("controller_backend",ownedDevice?ownedDevice->controllerBackend():"not-applicable");
    info->setProperty("underrun_frames_at_start",integer(ownedK?ownedK->device.getUnderrunCount():ownedDevice->underruns()));
    performanceStartedMs=juce::Time::getMillisecondCounterHiRes();performanceStoppedMs=0;performanceStopReason.clear();
    performanceDiagnostics.start(mode==2);
    return true;
}

juce::String Processor::performanceReport()const
{
    using namespace baseLib::performance;
    std::lock_guard lock(controlMutex);
    auto* root=object();juce::var report(root);
    root->setProperty("schema","silver-k.performance.v1");
    root->setProperty("exported_at",juce::Time::getCurrentTime().toISO8601(true));
    root->setProperty("session",performanceSession);
    root->setProperty("session_id",integer(performanceDiagnostics.generation()));
    root->setProperty("worker_wait_backend",ownedK?ownedK->device.workerWaitBackend():ownedDevice?ownedDevice->workerWaitBackend():"none");
    root->setProperty("worker_placement",placementInfo(ownedDevice?&ownedDevice->workerPlacement():nullptr));
    root->setProperty("dsp_execution_backend",ownedDevice?ownedDevice->dspExecutionBackend():"not-applicable");
    root->setProperty("controller_backend",ownedDevice?ownedDevice->controllerBackend():"not-applicable");
    root->setProperty("recording",performanceDiagnostics.mode()!=0);
    root->setProperty("stop_reason",performanceStopReason);
    root->setProperty("elapsed_ms",performanceStartedMs>0?(performanceStoppedMs>0?performanceStoppedMs:juce::Time::getMillisecondCounterHiRes())-performanceStartedMs:0);
    auto* system=object();root->setProperty("system",juce::var(system));
    system->setProperty("os",juce::SystemStats::getOperatingSystemName());
    system->setProperty("cpu",juce::SystemStats::getCpuModel());
    system->setProperty("logical_cpus",juce::SystemStats::getNumCpus());
    system->setProperty("physical_cpus",juce::SystemStats::getNumPhysicalCpus());
    system->setProperty("memory_mb",juce::SystemStats::getMemorySizeInMegabytes());
    system->setProperty("host",juce::PluginHostType().getHostDescription());
    system->setProperty("wrapper",juce::AudioProcessor::getWrapperTypeDescription(wrapperType));
#if defined(_M_ARM64) || defined(__aarch64__)
    system->setProperty("architecture","arm64");
#elif defined(_M_X64) || defined(__x86_64__)
    system->setProperty("architecture","x64");
#else
    system->setProperty("architecture","other");
#endif
    auto* build=object();root->setProperty("build",juce::var(build));
    build->setProperty("version","0.4.4a");
    build->setProperty("plugin_version",JucePlugin_VersionString);
    build->setProperty("git_revision",SILVER_DIAGNOSTICS_REVISION);
    build->setProperty("source_state_at_configure",SILVER_DIAGNOSTICS_SOURCE_STATE);
    build->setProperty("configuration",SILVER_DIAGNOSTICS_CONFIG);
#if defined(__clang__)
    build->setProperty("compiler","Clang " __clang_version__);
#elif defined(_MSC_VER)
    build->setProperty("compiler","MSVC "+juce::String(_MSC_VER));
#elif defined(__GNUC__)
    build->setProperty("compiler","GCC " __VERSION__);
#endif
    build->setProperty("dsp_revision","44353be2f0156cac970c6ab4f467ea1998b40605");
    auto* metrics=object();root->setProperty("metrics",juce::var(metrics));
    for(size_t i=0;i<size_t(Metric::Count);++i)
    {
        const auto metric=Metric(i);const auto v=performanceDiagnostics.read(metric);
        auto* result=object();metrics->setProperty(Names[i],juce::var(result));
        result->setProperty("unit",i<size_t(Metric::QueueDepth)?"ns":metric==Metric::OfflineCallbacks?"callbacks":"frames");
        result->setProperty("observations",integer(v.count));result->setProperty("total",integer(v.total));
        result->setProperty("mean",v.count?double(v.total)/double(v.count):0);
        result->setProperty("minimum",integer(v.minimum));result->setProperty("maximum",integer(v.maximum));
        result->setProperty("p99_upper_bound",integer(v.percentileUpperBound(99)));
        result->setProperty("work_frames",integer(v.units));result->setProperty("over_budget",integer(v.overBudget));
    }
    const auto worker=performanceDiagnostics.read(Metric::Worker);
    const double queueRate=performanceSession.isObject()?double(performanceSession["queue_rate_hz"]):0;
    root->setProperty("worker_wall_to_audio_ratio",worker.units&&queueRate>0?double(worker.total)*1.0e-9*queueRate/double(worker.units):0);
    root->setProperty("detailed_advance_stride",256);
    root->setProperty("notes","Wall times include OS preemption and inline JIT compilation, not CPU utilization. H8 includes controller peripherals; DSP includes its peripherals and JIT. Detailed stages sample every 256th board advance; do not sum with inclusive worker time. Worker segments exclude intentional sleeps and may lose an unfinished tail at stop. K queue uses native frames; KS queue uses host frames. Histogram percentiles are power-of-two upper bounds. Live atomic snapshots are approximate; an in-flight operation can finish after Stop. Firmware/audio reconfiguration ends the session. Patch/profile/mode changes are not separate sessions: keep the scenario fixed or start a new session. Driver/backend, DAW version, power mode, UI visibility and musical workload must be supplied in the test notes.");
    return juce::JSON::toString(report,false);
}
}
