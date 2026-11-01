#pragma once
#include <cstdint>
namespace kstation {
class MidiSendControlTask final {
public:enum class Stage:uint8_t {SavePointer,SaveData,BindPatch,ReadChannel,Status,SendStatus,RestoreData,RestorePointer,SendController,CopyValue,SendValue,Return,AwaitChild,Complete};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool resume(){if(!awaitingChild())return false;stage_=next_;return true;}
 template<class R>bool advance(R&&r){switch(stage_){case Stage::SavePointer:stage_=Stage::SaveData;break;case Stage::SaveData:stage_=Stage::BindPatch;break;case Stage::BindPatch:stage_=Stage::ReadChannel;break;case Stage::ReadChannel:r();stage_=Stage::Status;break;case Stage::Status:stage_=Stage::SendStatus;break;case Stage::SendStatus:request(Stage::RestoreData);break;case Stage::RestoreData:stage_=Stage::RestorePointer;break;case Stage::RestorePointer:stage_=Stage::SendController;break;case Stage::SendController:request(Stage::CopyValue);break;case Stage::CopyValue:stage_=Stage::SendValue;break;case Stage::SendValue:request(Stage::Return);break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitChild:case Stage::Complete:return false;}return true;}
private:void request(Stage next){next_=next;stage_=Stage::AwaitChild;}Stage stage_=Stage::SavePointer,next_=Stage::Return;
};
}
