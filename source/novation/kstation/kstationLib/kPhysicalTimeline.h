#pragma once

#include "kstationDSP.h"
#include <memory>
#include <optional>

#if defined(_WIN32) && !defined(SILVER_K_PHYSICAL_STATIC)
# ifdef SILVER_K_PHYSICAL_EXPORTS
#  define SILVER_K_PHYSICAL_API __declspec(dllexport)
# else
#  define SILVER_K_PHYSICAL_API __declspec(dllimport)
# endif
#else
# define SILVER_K_PHYSICAL_API
#endif

namespace kstation::command {
enum class PhysicalEvent : uint8_t { HostRead, HostWrite, DspReceive, DspReply };
using PhysicalObserver = void(*)(void*,PhysicalEvent,uint64_t,uint64_t,uint64_t,uint32_t,uint32_t,bool);

// The single K DSP and its physical clock survive a change of controller.
// No controller callback, guest RAM or exec() owner is retained here.
class SILVER_K_PHYSICAL_API PhysicalTimeline final {
public:
    struct VoiceTrigger {
        uint64_t reception;
        uint64_t resetGeneration;
        uint32_t address;
        uint32_t appliedWord;
        std::optional<uint32_t> dispatchAtClear;
    };
    PhysicalTimeline();
    ~PhysicalTimeline();
    PhysicalTimeline(const PhysicalTimeline&) = delete;
    PhysicalTimeline& operator=(const PhysicalTimeline&) = delete;
    uint8_t read(uint32_t offset);
    bool write(uint32_t offset,uint8_t value);
    void reset(bool asserted);
    void advanceTo(uint64_t controllerCycle);
    uint64_t cycle() const noexcept { return cycle_; }
    uint64_t frames() const noexcept { return dsp_->audioFramesProduced(); }
    uint64_t dspCycles() const noexcept { return dsp_->cycles(); }
    bool bootFinished() const noexcept { return dsp_->bootFinished(); }
    bool running() const noexcept { return started_ && !reset_; }
    bool popAudio(DSP::AudioFrame& frame) { return dsp_->popAudioFrame(frame); }
    bool pushAudioInput(int32_t mono24);
    void discardAudio() { dsp_->discardAudio(); }
    DSP& dsp() noexcept { return *dsp_; }
    // Arm immediately after the trigger's final accepted low-byte strobe.
    std::optional<VoiceTrigger> watchVoiceTrigger(uint8_t slot,uint32_t word) const noexcept;
    bool voiceActivated(VoiceTrigger&) const noexcept;
    void observe(PhysicalObserver observer,void* context) noexcept { observer_=observer;context_=context; }
private:
    void record(PhysicalEvent,uint32_t offset,uint32_t value,bool accepted=true);
    std::unique_ptr<DSP> dsp_;
    uint64_t cycle_=0;
    int64_t epoch_=0;
    bool started_=false,reset_=true;
    uint32_t pendingWord_=0;
    uint64_t receivedWords_=0,resetGeneration_=0;
    PhysicalObserver observer_=nullptr;
    void* context_=nullptr;
};
}
