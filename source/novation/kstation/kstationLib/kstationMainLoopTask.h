#pragma once
#include <array>
#include <cstdint>

namespace kstation {
// Semantic scheduler state only. Each call produces one explicit child request;
// the owner must complete that child and acknowledge it before continuing.
class MainLoopTask final {
public:
 enum class Child : uint8_t { Slot0, Slot1, Slot2, Slot3, Slot4, PeriodicPanel, ButtonQueue, ControlQueue, MidiInput, LocalKeys, VoiceFeedback, Arpeggiator, Playback, DspFeedback };
 struct Request { Child child; };
 bool awaitingChild() const { return waiting_; }
 Request request() const { return {static_cast<Child>(index_)}; }
 unsigned index() const { return index_; }
 // One iteration is complete after all children and the original back edge.
 bool atBackEdge() const { return index_ == 14 && !waiting_; }
 bool beginChild() { if(waiting_ || index_ >= 14) return false; waiting_ = true; return true; }
 bool resumeChild(Child child) { if(!waiting_ || unsigned(child)!=index_) return false; waiting_=false; ++index_; return true; }
 bool nextIteration() { if(!atBackEdge()) return false; index_=0; return true; }
private:
 unsigned index_=0; bool waiting_=false;
};

// Deferred slot calculation. Tick is a child service; a due callback is also a
// child and may itself rearm this or another slot. Reads occur at their actual
// phase, including the function pointer after clearing the slot.
class DeferredSlotTask final {
public:
 enum class Phase : uint8_t { SaveDelay, ReadDelay, SelectEnabled, SaveTick, RequestTick, ReadStart, Elapsed, Compare, SelectDue, ClearValue, ClearDelay, ReadCallback, RequestCallback, RestoreTick, RestoreDelay, Return, AwaitChild, Complete };
 enum class Child : uint8_t { Tick, Callback };
 enum class Read : uint8_t { Delay, Start, Callback };
 enum class Write : uint8_t { Delay };
 explicit DeferredSlotTask(unsigned slot) : slot_(slot), phase_(slot<5?Phase::SaveDelay:Phase::Complete) {}
 Phase phase() const { return phase_; } unsigned slot() const { return slot_; }
 bool awaitingChild() const { return phase_==Phase::AwaitChild; }
 bool complete() const { return phase_==Phase::Complete; }
 Child child() const { return child_; }
 uint16_t delay() const { return delay_; } uint16_t start() const { return start_; }
 uint16_t tick() const { return tick_; } uint16_t elapsed() const { return elapsed_; }
 uint32_t callback() const { return callback_; }
 bool resumeTick(uint16_t tick) { if(!awaitingChild()||child_!=Child::Tick)return false;tick_=tick;phase_=Phase::ReadStart;return true; }
 bool resumeCallback() { if(!awaitingChild()||child_!=Child::Callback)return false;phase_=Phase::RestoreTick;return true; }
 template<class Reader,class Writer> bool advance(Reader&& read,Writer&& write) {
  switch(phase_) {
   case Phase::SaveDelay: phase_=Phase::ReadDelay; break;
   case Phase::ReadDelay: delay_=uint16_t(read(Read::Delay));phase_=Phase::SelectEnabled;break;
   case Phase::SelectEnabled: phase_=delay_?Phase::SaveTick:Phase::RestoreDelay;break;
   case Phase::SaveTick:phase_=Phase::RequestTick;break;
   case Phase::RequestTick:child_=Child::Tick;phase_=Phase::AwaitChild;break;
   case Phase::ReadStart:start_=uint16_t(read(Read::Start));phase_=Phase::Elapsed;break;
   case Phase::Elapsed:elapsed_=uint16_t(tick_-start_);phase_=Phase::Compare;break;
   case Phase::Compare:phase_=Phase::SelectDue;break;
   case Phase::SelectDue:phase_=elapsed_<delay_?Phase::RestoreTick:Phase::ClearValue;break;
   case Phase::ClearValue:phase_=Phase::ClearDelay;break;
   case Phase::ClearDelay:write(Write::Delay,0);phase_=Phase::ReadCallback;break;
   case Phase::ReadCallback:callback_=read(Read::Callback);phase_=Phase::RequestCallback;break;
   case Phase::RequestCallback:child_=Child::Callback;phase_=Phase::AwaitChild;break;
   case Phase::RestoreTick:phase_=Phase::RestoreDelay;break;
   case Phase::RestoreDelay:phase_=Phase::Return;break;
   case Phase::Return:phase_=Phase::Complete;break;
   case Phase::AwaitChild:case Phase::Complete:return false;
  }return true;
 }
private:
 unsigned slot_;Phase phase_;Child child_=Child::Tick;uint16_t delay_=0,start_=0,tick_=0,elapsed_=0;uint32_t callback_=0;
};
}
