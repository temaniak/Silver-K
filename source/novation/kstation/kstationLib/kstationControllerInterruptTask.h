#pragma once
#include <cstdint>
namespace kstation {
// Per-phase costs of physical transactions are supplied by the board timing
// model. Context save/restore are semantic scheduler overheads; no CPU frame
// or stack representation is retained by this task.
class ControllerInterruptTask final {
public:
 enum class Kind:uint8_t{Timer3,SerialError,Unused};
 enum class Stage:uint8_t{SaveContext,ReadStatus,ClearStatus,MaskStatus,WriteStatus,ReadCount,IncrementCount,WriteCount,SetError,WriteError,RestoreContext,Return,Complete};
 enum class Resource:uint8_t{TimerStatus,SerialStatus,CounterHigh,CounterLow,SerialErrorFlag};
 struct Credits{unsigned save=0,restore=0,returning=0,statusRead=0,statusWrite=0,countReadFirst=0,countReadSecond=0,countWriteFirst=0,countWriteSecond=0,errorWrite=0;};
 explicit ControllerInterruptTask(Kind kind):kind_(kind),stage_(kind==Kind::Unused?Stage::Return:Stage::SaveContext){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::SaveContext:return c.save;case Stage::ReadStatus:return 3+c.statusRead;case Stage::ClearStatus:case Stage::MaskStatus:case Stage::IncrementCount:case Stage::SetError:return 3;case Stage::WriteStatus:return 3+c.statusWrite;case Stage::ReadCount:return 9+c.countReadFirst+c.countReadSecond;case Stage::WriteCount:return 9+c.countWriteFirst+c.countWriteSecond;case Stage::WriteError:return 9+c.errorWrite;case Stage::RestoreContext:return c.restore;case Stage::Return:return c.returning;case Stage::Complete:return 0;}return 0;}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read,Writer&&write){auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){case Stage::SaveContext:stage_=Stage::ReadStatus;break;case Stage::ReadStatus:status_=uint8_t(read(kind_==Kind::Timer3?Resource::TimerStatus:Resource::SerialStatus,3+c.statusRead));stage_=kind_==Kind::Timer3?Stage::ClearStatus:Stage::MaskStatus;break;case Stage::ClearStatus:status_=0;stage_=Stage::WriteStatus;break;case Stage::MaskStatus:status_&=0xc7;stage_=Stage::WriteStatus;break;case Stage::WriteStatus:write(kind_==Kind::Timer3?Resource::TimerStatus:Resource::SerialStatus,status_,3+c.statusWrite);stage_=kind_==Kind::Timer3?Stage::ReadCount:Stage::SetError;break;case Stage::ReadCount:count_=uint16_t(uint16_t(read(Resource::CounterHigh,9+c.countReadFirst))<<8);count_|=uint8_t(read(Resource::CounterLow,9+c.countReadFirst+c.countReadSecond));stage_=Stage::IncrementCount;break;case Stage::IncrementCount:++count_;stage_=Stage::WriteCount;break;case Stage::WriteCount:write(Resource::CounterHigh,uint8_t(count_>>8),9+c.countWriteFirst);write(Resource::CounterLow,uint8_t(count_),9+c.countWriteFirst+c.countWriteSecond);stage_=Stage::RestoreContext;break;case Stage::SetError:stage_=Stage::WriteError;break;case Stage::WriteError:write(Resource::SerialErrorFlag,2,9+c.errorWrite);stage_=Stage::RestoreContext;break;case Stage::RestoreContext:stage_=Stage::Return;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::Complete:break;}return cost;}
private:Kind kind_;Stage stage_;uint8_t status_=0;uint16_t count_=0;
};
}
