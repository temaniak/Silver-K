#pragma once
#include "kstationFlashCommitTask.h"
namespace kstation {
// Flash write-protect policy and its notice. Programming is a named child.
class ControllerFlashCommitTask final {
 using T=FlashCommitTask;using S=T::Stage;
public:
 enum class Service:uint8_t{None,Program,Display};
 ControllerFlashCommitTask(uint32_t source,uint32_t target):source_(source),target_(target){}
 bool complete()const{return task_.complete();}
 Service request()const{if(notice_==3)return Service::Display;if(task_.awaitingChild()&&task_.child()==T::Child::Program)return Service::Program;return Service::None;}
 uint32_t source()const{return source_;}uint32_t target()const{return target_;}
 bool resume(Service s){if(s!=request()||s==Service::None)return false;if(s==Service::Program)return task_.resume(T::Child::Program);notice_=0;return task_.resume(T::Child::Notice);}
 bool stackWrites()const{return task_.stage()==S::SaveSource||task_.stage()==S::SaveTarget||task_.stage()==S::Notice;}
 unsigned nextClocks()const{if(request()!=Service::None||complete())return 0;if(notice_)return notice_==1?9:8;switch(task_.stage()){case S::BindProgram:case S::AdvanceSource:case S::AdvanceTarget:return 9;case S::ReadProtect:return 15;case S::SaveSource:case S::SaveTarget:case S::RestoreTarget:case S::RestoreSource:return 10;case S::Notice:return 5;case S::Return:return 7;case S::AwaitChild:case S::Complete:return 0;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;if(notice_){++notice_;return cost;}auto stage=task_.stage();bool ok=task_.advance([&](){return read(0xbb14,1,cost);});if(stage==S::AdvanceSource)source_+=0x100;if(stage==S::AdvanceTarget)target_+=0x100;if(task_.awaitingChild()&&task_.child()==T::Child::Notice)notice_=1;return ok?cost:0;}
private:T task_;uint32_t source_,target_;unsigned notice_=0;
};
}
