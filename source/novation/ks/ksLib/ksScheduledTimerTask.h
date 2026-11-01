#pragma once
#include <cstdint>
namespace ks {
// Five firmware delayed actions share modular 16-bit elapsed-time arithmetic.
// Handles identify actions; the native task neither decodes nor executes code.
class ScheduledTimerTask final {
public:
 enum class Kind {Cancel,Arm,Service};
 enum class Field {Interval,Started,Action};
 enum class Stage {Clear,StoreClear,StoreAction,StoreInterval,DispatchClock,AwaitClock,StoreStarted,
 RetainInterval,ReadInterval,AdmitInterval,RetainClock,ReadStarted,Elapsed,Compare,AdmitDeadline,
 ZeroInterval,StoreExpired,ReadAction,DispatchAction,AwaitAction,ReleaseClock,ReleaseInterval,Finish,Complete};
 struct ActionHandle {uint32_t value=0;};
 ScheduledTimerTask(Kind kind,unsigned slot):kind_(kind),slot_(slot),stage_(kind==Kind::Cancel?Stage::Clear:kind==Kind::Arm?Stage::StoreAction:Stage::RetainInterval){}
 Kind kind()const{return kind_;}unsigned slot()const{return slot_;}Stage stage()const{return stage_;}
 uint16_t interval()const{return interval_;}uint16_t observedTime()const{return observedTime_;}uint16_t startedTime()const{return startedTime_;}
 uint16_t elapsed()const{return elapsed_;}ActionHandle action()const{return action_;}
 bool awaitingClock()const{return stage_==Stage::AwaitClock;}
 bool awaitingAction()const{return stage_==Stage::AwaitAction;}
 bool resumeClock(uint16_t now){if(!awaitingClock())return false;observedTime_=now;stage_=kind_==Kind::Arm?Stage::StoreStarted:Stage::ReadStarted;return true;}
 bool resumeAction(){if(!awaitingAction())return false;stage_=Stage::ReleaseClock;return true;}
 bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks()const{switch(stage_){case Stage::StoreClear:case Stage::StoreInterval:case Stage::StoreStarted:case Stage::ReadInterval:case Stage::ReadStarted:case Stage::StoreExpired:return 12;
 case Stage::StoreAction:case Stage::ReadAction:return 18;case Stage::DispatchClock:case Stage::RetainClock:case Stage::ReleaseClock:return 20;
 case Stage::DispatchAction:return 15;case Stage::RetainInterval:case Stage::ReleaseInterval:return 11;case Stage::Finish:return 17;
 case Stage::AwaitClock:case Stage::AwaitAction:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){unsigned used=0;
  while(const auto cost=nextClocks()){if(cost>available-used)break;switch(stage_){
   case Stage::Clear:interval_=0;stage_=Stage::StoreClear;break;case Stage::StoreClear:write(Field::Interval,0);stage_=Stage::Finish;break;
   case Stage::StoreAction:write(Field::Action,read(Field::Action,true));stage_=Stage::StoreInterval;break;
   case Stage::StoreInterval:write(Field::Interval,uint16_t(read(Field::Interval,true)));stage_=Stage::DispatchClock;break;
   case Stage::DispatchClock:stage_=Stage::AwaitClock;break;case Stage::StoreStarted:write(Field::Started,observedTime_);stage_=Stage::Finish;break;
   case Stage::RetainInterval:stage_=Stage::ReadInterval;break;case Stage::ReadInterval:interval_=uint16_t(read(Field::Interval,false));stage_=Stage::AdmitInterval;break;
   case Stage::AdmitInterval:stage_=interval_?Stage::RetainClock:Stage::ReleaseInterval;break;
   case Stage::RetainClock:stage_=Stage::DispatchClock;break;case Stage::ReadStarted:startedTime_=uint16_t(read(Field::Started,false));stage_=Stage::Elapsed;break;
   case Stage::Elapsed:elapsed_=uint16_t(observedTime_-startedTime_);stage_=Stage::Compare;break;case Stage::Compare:stage_=Stage::AdmitDeadline;break;
   case Stage::AdmitDeadline:stage_=elapsed_<interval_?Stage::ReleaseClock:Stage::ZeroInterval;break;
   case Stage::ZeroInterval:startedTime_=0;stage_=Stage::StoreExpired;break;case Stage::StoreExpired:write(Field::Interval,0);stage_=Stage::ReadAction;break;
   case Stage::ReadAction:action_.value=read(Field::Action,false);stage_=Stage::DispatchAction;break;
   case Stage::DispatchAction:stage_=Stage::AwaitAction;break;case Stage::ReleaseClock:stage_=Stage::ReleaseInterval;break;
   case Stage::ReleaseInterval:stage_=Stage::Finish;break;case Stage::Finish:stage_=Stage::Complete;break;default:break;}used+=cost;
  }return used;
 }
private:Kind kind_;unsigned slot_;Stage stage_;uint16_t interval_=0,observedTime_=0,startedTime_=0,elapsed_=0;ActionHandle action_;
};
}
