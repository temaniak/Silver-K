#pragma once
#include <cstdint>
namespace kstation {
class ScriptTimerTask final {
public:enum class Kind:uint8_t{Advance,Compare};enum class Stage:uint8_t{Delta,BindShift,Shift,Decrement,Repeat,ReadTarget,Add,WriteTarget,ReadElapsed,ReadDeadline,Compare,Return,AwaitDelta,Complete};explicit ScriptTimerTask(Kind k):kind_(k),stage_(k==Kind::Advance?Stage::Delta:Stage::ReadElapsed){}Kind kind()const{return kind_;}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingDelta()const{return stage_==Stage::AwaitDelta;}bool due()const{return elapsed_>=deadline_;}bool resumeDelta(uint32_t v){if(!awaitingDelta())return false;delta_=v;stage_=Stage::BindShift;return true;}
 template<class R,class W>bool advance(R&&read,W&&write){switch(stage_){case Stage::Delta:stage_=Stage::AwaitDelta;break;case Stage::BindShift:count_=12;stage_=Stage::Shift;break;case Stage::Shift:delta_<<=1;stage_=Stage::Decrement;break;case Stage::Decrement:--count_;stage_=Stage::Repeat;break;case Stage::Repeat:stage_=count_?Stage::Shift:Stage::ReadTarget;break;case Stage::ReadTarget:deadline_=read(false);stage_=Stage::Add;break;case Stage::Add:deadline_+=delta_;stage_=Stage::WriteTarget;break;case Stage::WriteTarget:write(deadline_);stage_=Stage::Return;break;case Stage::ReadElapsed:elapsed_=read(true);stage_=Stage::ReadDeadline;break;case Stage::ReadDeadline:deadline_=read(false);stage_=Stage::Compare;break;case Stage::Compare:stage_=Stage::Return;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitDelta:case Stage::Complete:return false;}return true;}
private:Kind kind_;Stage stage_;uint32_t delta_=0,deadline_=0,elapsed_=0;uint8_t count_=0;
};
}
