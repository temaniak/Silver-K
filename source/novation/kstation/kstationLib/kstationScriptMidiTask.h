#pragma once
#include <cstdint>
namespace kstation {
class ScriptMidiTask final {
public:enum class Stage:uint8_t{RetainCursor,CopyByte,TestStatus,SelectStatus,CompareSystem,SelectSystem,MaskStatus,BindProgram,ReadChannel,MergeChannel,Dispatch,RestoreCursor,Return,AwaitDispatch,Complete};explicit ScriptMidiTask(uint8_t value):value_(value){}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingDispatch()const{return stage_==Stage::AwaitDispatch;}uint8_t value()const{return value_;}bool resume(){if(!awaitingDispatch())return false;stage_=Stage::RestoreCursor;return true;}
 template<class R>bool advance(R&&read){switch(stage_){case Stage::CopyByte:stage_=Stage::SelectStatus;break;case Stage::TestStatus:stage_=Stage::SelectStatus;break;case Stage::SelectStatus:stage_=value_&0x80?Stage::CompareSystem:Stage::Dispatch;break;case Stage::SelectSystem:stage_=value_>0xef?Stage::Dispatch:Stage::MaskStatus;break;case Stage::MaskStatus:value_&=0xf0;stage_=Stage::BindProgram;break;case Stage::ReadChannel:channel_=uint8_t(read());stage_=Stage::MergeChannel;break;case Stage::MergeChannel:value_|=channel_;stage_=Stage::Dispatch;break;case Stage::Dispatch:stage_=Stage::AwaitDispatch;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitDispatch:case Stage::Complete:return false;default:stage_=Stage(unsigned(stage_)+1);break;}return true;}
private:Stage stage_=Stage::RetainCursor;uint8_t value_=0,channel_=0;
};
}
