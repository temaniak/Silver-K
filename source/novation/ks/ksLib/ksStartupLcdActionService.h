#pragma once
#include "ksPanelActionTask.h"
#include "ksPanelPollTask.h"
#include "ksPanelByteTask.h"
#include <array>
#include <optional>
#include <stdexcept>
namespace ks {
// Reset-time LCD initialization and glyph data share the real poll/byte wire
// owners. Frames are typed LCD actions, not machine calls or return addresses.
class StartupLcdActionService final {
public:
 using Kind=PanelActionTask::Kind;struct Effect{bool read=false,write=false;};
 explicit StartupLcdActionService(Kind kind,uint8_t value=0,uint8_t count=0){push(kind,value,count);}
 bool complete()const{return !depth_;}unsigned priority()const{return 0;}const Effect&effect()const{return effect_;}
 uint8_t output()const{return output_;}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;if(byte_)return byte_->nextClocks([&](PanelByteTask::Port p){return clock(p,previous);});if(poll_)return poll_->nextClocks();return current().task.nextClocks();}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};
  if(byte_){using T=PanelByteTask;const auto stage=byte_->stage();effect_.write=stage==T::Stage::Hold;effect_.read=stage==T::Stage::RestoreHeld||stage==T::Stage::Finish;
   byte_->advanceBefore(cost,[&](T::Read q){return q==T::Read::Input?byteInput_:uint8_t(read(0xffff51,cost));},[&](T::Port q,uint8_t value){write(address(q),value,cost);},[&](T::Port p){return clock(p,previous);});
   if(byte_->stage()==T::Stage::Complete){const auto reply=byte_->data();const bool reading=byte_->reading();byte_.reset();if(reading)output_=reply;if(byteFromPoll_){if(!poll_->resumeChild(reply))throw std::logic_error("Startup LCD poll byte return");}else if(!current().task.resumeChild(reply))throw std::logic_error("Startup LCD action byte return");}return cost;
  }
  if(poll_){using T=PanelPollTask;const auto stage=poll_->stage();effect_.write=stage==T::Stage::DispatchStatus||stage==T::Stage::DispatchFinal;effect_.read=stage==T::Stage::Finish;poll_->advanceBefore(cost);if(poll_->awaiting()){byte_.emplace(PanelByteTask::Kind::CommandRead);byteFromPoll_=true;byteInput_=0;}else if(poll_->stage()==T::Stage::Complete){const auto reply=poll_->reply();poll_.reset();if(!current().task.resumeChild(reply))throw std::logic_error("Startup LCD poll return");}return cost;
  }
  auto&f=current();using T=PanelActionTask;const auto stage=f.task.stage();effect_.write=stage==T::Stage::Hold;effect_.read=stage==T::Stage::Restore||stage==T::Stage::Finish;
  f.task.advanceBefore(cost,[&](T::Read q){return q==T::Read::Input?f.value:f.count;});if(f.task.stage()==T::Stage::Complete){output_=f.task.input();frames_[--depth_].reset();if(depth_&&!current().task.resumeChild(output_))throw std::logic_error("Startup LCD nested return");return cost;}
  using C=T::Child;const auto child=f.task.child();switch(child){case C::None:break;case C::Poll:poll_.emplace();effect_.write=true;break;
   case C::CommandByte:case C::DataByte:case C::DataRead:byte_.emplace(child==C::CommandByte?PanelByteTask::Kind::CommandWrite:child==C::DataRead?PanelByteTask::Kind::DataRead:PanelByteTask::Kind::DataWrite);byteInput_=f.task.input();byteFromPoll_=false;effect_.write=true;break;
   case C::Command:push(Kind::Command,f.task.input());effect_.write=true;break;case C::Write:push(Kind::WriteData,f.task.input());effect_.write=true;break;case C::Cursor:push(Kind::Cursor,f.task.input());effect_.write=true;break;case C::Spaces:push(Kind::Spaces,f.task.input(),f.task.remaining());effect_.write=true;break;}
  return cost;
 }
private:
 struct Frame{PanelActionTask task;uint8_t value=0,count=0;Frame(Kind k,uint8_t v,uint8_t n):task(k),value(v),count(n){}};
 Frame&current(){return *frames_[depth_-1];}const Frame&current()const{return *frames_[depth_-1];}
 void push(Kind k,uint8_t value,uint8_t count=0){if(depth_==frames_.size())throw std::logic_error("Startup LCD action capacity");frames_[depth_++].emplace(k,value,count);}
 static uint32_t address(PanelByteTask::Port p){return p==PanelByteTask::Port::Gate?0xfffeb1u:p==PanelByteTask::Port::Control?0xffff62u:0xffff61u;}
 static unsigned clock(PanelByteTask::Port p,int previous){const auto a=address(p);return(a&1)&&previous==int(a-1)?0u:1u;}
 std::array<std::optional<Frame>,4>frames_{};unsigned depth_=0;std::optional<PanelPollTask>poll_;std::optional<PanelByteTask>byte_;bool byteFromPoll_=false;uint8_t byteInput_=0,output_=0;Effect effect_{};
};
}
