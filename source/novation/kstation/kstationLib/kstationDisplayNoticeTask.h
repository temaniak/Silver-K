#pragma once
#include <cstdint>
namespace kstation {
class DisplayNoticeTask final {
public:
 enum class Kind:uint8_t {Show,Expire};enum class Stage:uint8_t {Print,SetActive,WriteActive,BindDelay,BindCallback,Schedule,ClearActive,WriteInactive,Refresh,Return,AwaitChild,Complete};enum class Child:uint8_t {Print,Schedule,Refresh};
 explicit DisplayNoticeTask(Kind k):kind_(k),stage_(k==Kind::Show?Stage::Print:Stage::ClearActive){}Kind kind()const{return kind_;}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}Child child()const{return child_;}bool resume(Child c){if(!awaitingChild()||child_!=c)return false;stage_=c==Child::Print?Stage::SetActive:Stage::Return;return true;}
 template<class W>bool advance(W&&w){switch(stage_){case Stage::Print:request(Child::Print);break;case Stage::SetActive:stage_=Stage::WriteActive;break;case Stage::WriteActive:w(1);stage_=Stage::BindDelay;break;case Stage::BindDelay:stage_=Stage::BindCallback;break;case Stage::BindCallback:stage_=Stage::Schedule;break;case Stage::Schedule:request(Child::Schedule);break;case Stage::ClearActive:stage_=Stage::WriteInactive;break;case Stage::WriteInactive:w(0);stage_=Stage::Refresh;break;case Stage::Refresh:request(Child::Refresh);break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitChild:case Stage::Complete:return false;}return true;}
private:void request(Child c){child_=c;stage_=Stage::AwaitChild;}Kind kind_;Stage stage_;Child child_=Child::Print;
};
class DeferredScheduleTask final {
public:
 enum class Stage:uint8_t {WriteCallback,WriteDelay,Tick,WriteStart,Return,AwaitChild,Complete};DeferredScheduleTask(uint32_t callback,uint16_t delay):callback_(callback),delay_(delay){}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool resume(uint16_t tick){if(!awaitingChild())return false;tick_=tick;stage_=Stage::WriteStart;return true;}
 template<class W>bool advance(W&&w){switch(stage_){case Stage::WriteCallback:w(0,callback_);stage_=Stage::WriteDelay;break;case Stage::WriteDelay:w(1,delay_);stage_=Stage::Tick;break;case Stage::Tick:stage_=Stage::AwaitChild;break;case Stage::WriteStart:w(2,tick_);stage_=Stage::Return;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitChild:case Stage::Complete:return false;}return true;}
private:Stage stage_=Stage::WriteCallback;uint32_t callback_;uint16_t delay_,tick_=0;
};
}
