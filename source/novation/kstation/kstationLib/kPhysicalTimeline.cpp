#include "kPhysicalTimeline.h"
#include "command/kFeedbackProtocol.h"
#include <limits>
#include <stdexcept>

namespace kstation::command {
namespace {
using novation::controller::command::KFeedbackProtocol;
using novation::controller::command::VoiceSlot;
constexpr uint32_t VoiceDispatchCounter=0x17;
}
PhysicalTimeline::PhysicalTimeline():dsp_(std::make_unique<DSP>()) {
    auto& host=dsp_->peripherals().getHDI08();
    host.setReadRxCallback([this]{++receivedWords_;record(PhysicalEvent::DspReceive,0,pendingWord_);});
    host.setWriteTxCallback([this,&host]{record(PhysicalEvent::DspReply,0,host.txData().front());});
}
PhysicalTimeline::~PhysicalTimeline()=default;
void PhysicalTimeline::record(PhysicalEvent event,uint32_t offset,uint32_t value,bool accepted) {
    if(observer_)observer_(context_,event,cycle_,dsp_->cycles(),frames(),offset,value,accepted);
}
uint8_t PhysicalTimeline::read(uint32_t offset) {
    const auto value=dsp_->hostRead(offset);record(PhysicalEvent::HostRead,offset,value);return value;
}
bool PhysicalTimeline::write(uint32_t offset,uint8_t value) {
    const auto accepted=dsp_->hostWrite(offset,value);
    if(accepted && offset==14 && dsp_->bootFinished()) {
        const auto& queue=dsp_->peripherals().getHDI08().rxData();
        if(!queue.empty())pendingWord_=queue.front();
    }
    record(PhysicalEvent::HostWrite,offset,value,accepted);return accepted;
}
bool PhysicalTimeline::pushAudioInput(int32_t mono24) {
    return dsp_->pushAudioInput(mono24);
}
void PhysicalTimeline::reset(bool asserted) {
    if(asserted && !reset_){dsp_->reset();started_=false;pendingWord_=0;++resetGeneration_;}
    reset_=asserted;
}
std::optional<PhysicalTimeline::VoiceTrigger> PhysicalTimeline::watchVoiceTrigger(uint8_t slot,uint32_t word) const noexcept {
    const auto& rx=dsp_->peripherals().getHDI08().rxData();
    if(!running() || slot>=KFeedbackProtocol::VoiceCount || word>0xffffff ||
       !(word&KFeedbackProtocol::TriggerPendingBit) || rx.empty() || rx.front()!=word)return std::nullopt;
    return VoiceTrigger{receivedWords_+1,resetGeneration_,KFeedbackProtocol::triggerAddress(VoiceSlot(slot)),
                        word&~KFeedbackProtocol::TriggerPendingBit,{}};
}
bool PhysicalTimeline::voiceActivated(VoiceTrigger& trigger) const noexcept {
    if(!running() || trigger.resetGeneration!=resetGeneration_ || receivedWords_<trigger.reception)return false;
    auto& memory=dsp_->memory();
    if(memory.get(dsp56k::MemArea_X,trigger.address)!=trigger.appliedWord)return false;
    const auto dispatch=memory.get(dsp56k::MemArea_X,VoiceDispatchCounter);
    // The trigger flag clears before installation. The next dispatch counter
    // increment is downstream of the entire previous voice-control pass.
    if(trigger.dispatchAtClear)return dispatch!=*trigger.dispatchAtClear;
    trigger.dispatchAtClear=dispatch;
    return false;
}
void PhysicalTimeline::advanceTo(uint64_t now) {
    if(now<cycle_ || now>uint64_t(std::numeric_limits<int64_t>::max()/7))
        throw std::runtime_error("K physical clock outside monotonic range");
    cycle_=now;
    if(!dsp_->bootFinished() || reset_)return;
    if(!started_){epoch_=int64_t(now*7)-int64_t(dsp_->cycles());started_=true;}
    const auto target=int64_t(now*7)-epoch_;
    for(size_t steps=0;int64_t(dsp_->cycles())<target;++steps) {
        if(steps==10000 || !dsp_->step(16))throw std::runtime_error("K DSP failed to advance");
    }
}
}
