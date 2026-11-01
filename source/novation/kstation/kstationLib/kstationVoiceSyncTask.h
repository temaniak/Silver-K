#pragma once
#include <cstdint>
namespace kstation {
// Two fixed voice-global commands used by parameter edits.
class VoiceSyncTask final {
public:
 enum class Kind:uint8_t{First,Second};enum class Stage:uint8_t{SetValue,Jump,SetAddress,ClearHigh,Dispatch,Return,AwaitDsp,Complete};struct Credits{unsigned call=0,ret=0;};
 explicit VoiceSyncTask(Kind kind):kind_(kind){}Kind kind()const{return kind_;}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingDsp()const{return stage_==Stage::AwaitDsp;}uint8_t value()const{return kind_==Kind::First?1:2;}bool resumeDsp(){if(!awaitingDsp())return false;stage_=Stage::Return;return true;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::SetAddress:return 6;case Stage::Dispatch:return c.call;case Stage::Return:return c.ret;case Stage::AwaitDsp:case Stage::Complete:return 0;default:return 3;}}
 unsigned advanceBefore(unsigned available,const Credits&c){auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){case Stage::Dispatch:stage_=Stage::AwaitDsp;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitDsp:case Stage::Complete:break;default:stage_=Stage(unsigned(stage_)+1);break;}return cost;}
private:Kind kind_;Stage stage_=Stage::SetValue;
};
}
