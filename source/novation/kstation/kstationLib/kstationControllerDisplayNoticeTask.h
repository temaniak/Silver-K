#pragma once
#include "kstationDisplayNoticeTask.h"
#include "kstationStartupServicesTask.h"
#include "kstationControllerForegroundTask.h"
#include <optional>
namespace kstation {
// Display notice and deferred expiry registration. LCD text is an explicit child.
class ControllerDisplayNoticeTask final {
 using T=DisplayNoticeTask;using TS=T::Stage;using S=StartupSchedulerTask;using SS=S::Stage;using Tick=ControllerForegroundTickTask;
public:
 explicit ControllerDisplayNoticeTask(uint32_t source):source_(source&0xffffff){}
 bool complete()const{return notice_.complete()&&!schedule_&&!tick_;}
 bool awaitingString()const{return notice_.awaitingChild()&&notice_.child()==T::Child::Print;}
 uint32_t source()const{return source_;}
 bool resumeString(){return awaitingString()&&notice_.resume(T::Child::Print);}
 bool stackWrites()const{if(tick_)return false;if(schedule_)return schedule_->stage()==SS::ReadTick;auto s=notice_.stage();return s==TS::Print||s==TS::Schedule;}
 unsigned nextClocks(int previous=-1)const{if(awaitingString()||complete())return 0;if(tick_)return tick_->nextClocks();if(schedule_){FlashCredits c;c.write=bus(schedule_->stage()==SS::PublishCallback?0xcd4e:schedule_->stage()==SS::PublishDelay?0xcd4c:0xcd4a,schedule_->stage()==SS::PublishCallback?4:2,previous);return schedule_->nextClocks(c);}switch(notice_.stage()){case TS::Print:case TS::Schedule:return 10;case TS::WriteActive:return 9+bus(0xcd7d,1,previous);case TS::BindDelay:return 6;case TS::BindCallback:return 9;case TS::Return:return 7;case TS::AwaitChild:case TS::Complete:return 0;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;if(tick_){auto n=tick_->advanceBefore(cost,read);if(tick_->complete()){auto value=tick_->result();tick_.reset();schedule_->resume(value);}return n;}if(schedule_){FlashCredits c;c.write=bus(schedule_->stage()==SS::PublishCallback?0xcd4e:schedule_->stage()==SS::PublishDelay?0xcd4c:0xcd4a,schedule_->stage()==SS::PublishCallback?4:2,previous);auto n=schedule_->advanceBefore(cost,c,[&](S::Write f,uint32_t v){write(f==S::Write::Callback?0xcd4e:f==S::Write::Delay?0xcd4c:0xcd4a,v,f==S::Write::Callback?4:2,cost);});if(schedule_->awaiting())tick_.emplace();if(schedule_->complete()){schedule_.reset();notice_.resume(T::Child::Schedule);}return n;}notice_.advance([&](uint8_t v){write(0xcd7d,v,1,cost);});if(notice_.awaitingChild()&&notice_.child()==T::Child::Schedule)schedule_.emplace(1,true,0x43a6,0x2ee);return cost;}
private:static unsigned bus(uint32_t a,unsigned n,int p){unsigned cost=0;for(unsigned i=0;i<n;++i){auto q=(a+i)&0xffffff;if(q<0x400000||(q>=0xfffd10&&q<0xffffa0)){if((p&1)||q!=uint32_t(p|1))cost+=q<0x400000?3:1;p=int(q);}else{cost+=q>=0xffffa0?1:3;p=-1;}}return cost;}
 uint32_t source_;T notice_{T::Kind::Show};std::optional<S>schedule_{};std::optional<Tick>tick_{};
};
}
