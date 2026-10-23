#pragma once
#include "ksMidiDescriptorTask.h"
#include <cstdint>
namespace ks {
// Receive/internal queue service and the independent internal MIDI parser.
// The immediate shared handler remains owned by this job until its handoff.
class InternalMidiServiceTask final {
public:
 enum class Kind { Initialize, Pump, Byte, Reset };enum class Stage { ResetReceive, ResetTransmit, ResetQueue, ResetParser, ReturnInit, PopReceive, CompareReceiveByte, SelectReceiveByte, ParseReceive, RepeatReceive, CompareReceiveEmpty, SelectReceiveReset, ClearReceive, PopQueue, CompareQueueByte, SelectQueueByte, ParseQueue, RepeatQueue, CompareQueueEmpty, SelectQueueReset, ClearQueue, ReturnPump, SetPreview, WritePreview, ReadInput, SelectInput, WriteReceived, LookupStatus, ReadLength, WriteExpected, ReadFlags, MaskImmediate, SelectImmediate, WriteRemaining, SelectLength, WriteRunningStatus, CaptureChannel, WriteChannel, SelectPayload, WritePayloadCursor, ReturnStatus, ReadRemaining, SelectRemaining, ReadCursor, WriteData, DecrementRemaining, WriteDataRemaining, SelectCompleteMessage, AdvanceCursor, WriteDataCursor, ReturnData, ReadRunningStatus, WriteDispatchStatus, LookupRunning, ReadRunningFlags, MaskRunning, SelectRunning, ReadRunningLength, WriteRunningRemaining, SelectRunningPayload, WriteRunningCursor, ReadHandler, BindPayload, ReadChannel, WriteDispatchChannel, DispatchMessage, ClearReset, ResetReceiveRemaining, ResetReceiveExpected, ResetInternalRemaining, ResetInternalExpected, ResetExclusive, ResetTransfer, ResetActivity, ResetFirstWord, ResetSecondWord, ResetSystem, ResetSysEx, ReturnReset, ReadImmediateHandler, DispatchImmediate, AwaitChild, Complete };
 enum class Read { Input, Length, Flags, Remaining, Cursor, RunningStatus, Handler, Channel };enum class Write { Preview, Received, Expected, Remaining, RunningStatus, Channel, Cursor, Data, DispatchStatus, DispatchChannel, ResetReceiveRemaining, ResetReceiveExpected, ResetInternalRemaining, ResetInternalExpected, ResetExclusive, ResetTransfer, ResetActivity, ResetFirstWord, ResetSecondWord };
 explicit InternalMidiServiceTask(Kind kind){switch(kind){case Kind::Initialize:stage_=Stage::ResetReceive;break;case Kind::Pump:stage_=Stage::PopReceive;break;case Kind::Byte:stage_=Stage::SetPreview;break;case Kind::Reset:stage_=Stage::ClearReset;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint16_t event){if(!awaitingChild())return false;status_=uint8_t(event>>8);stage_=resume_;return true;}
 uint8_t input()const{return input_;}uint8_t statusByte()const{return statusByte_;}uint8_t length()const{return length_;}uint8_t flags()const{return flags_;}uint8_t remaining()const{return remaining_;}uint8_t channel()const{return channel_;}uint8_t data()const{return data_;}uint32_t cursor()const{return cursor_;}uint32_t handler()const{return handler_;}
 MidiDescriptorKey descriptor()const{return MidiDescriptorKey::forStatus(statusByte_);}
 bool publication()const{switch(stage_){case Stage::WritePreview:case Stage::WriteReceived:case Stage::WriteExpected:case Stage::WriteRemaining:case Stage::WriteRunningStatus:case Stage::WriteChannel:case Stage::WritePayloadCursor:case Stage::WriteData:case Stage::WriteDataRemaining:case Stage::WriteDataCursor:case Stage::WriteDispatchStatus:case Stage::WriteRunningRemaining:case Stage::WriteRunningCursor:case Stage::WriteDispatchChannel:case Stage::ResetReceiveRemaining:case Stage::ResetReceiveExpected:case Stage::ResetInternalRemaining:case Stage::ResetInternalExpected:case Stage::ResetExclusive:case Stage::ResetTransfer:case Stage::ResetActivity:case Stage::ResetFirstWord:case Stage::ResetSecondWord:return true; default:return false;}}
 Write publicationKind()const{switch(stage_){case Stage::WritePreview:return Write::Preview;case Stage::WriteReceived:return Write::Received;case Stage::WriteExpected:return Write::Expected;case Stage::WriteRemaining:return Write::Remaining;case Stage::WriteRunningStatus:return Write::RunningStatus;case Stage::WriteChannel:return Write::Channel;case Stage::WritePayloadCursor:return Write::Cursor;case Stage::WriteData:return Write::Data;case Stage::WriteDataRemaining:return Write::Remaining;case Stage::WriteDataCursor:return Write::Cursor;case Stage::WriteDispatchStatus:return Write::DispatchStatus;case Stage::WriteRunningRemaining:return Write::Remaining;case Stage::WriteRunningCursor:return Write::Cursor;case Stage::WriteDispatchChannel:return Write::DispatchChannel;case Stage::ResetReceiveRemaining:return Write::ResetReceiveRemaining;case Stage::ResetReceiveExpected:return Write::ResetReceiveExpected;case Stage::ResetInternalRemaining:return Write::ResetInternalRemaining;case Stage::ResetInternalExpected:return Write::ResetInternalExpected;case Stage::ResetExclusive:return Write::ResetExclusive;case Stage::ResetTransfer:return Write::ResetTransfer;case Stage::ResetActivity:return Write::ResetActivity;case Stage::ResetFirstWord:return Write::ResetFirstWord;case Stage::ResetSecondWord:return Write::ResetSecondWord; default:return Write::Preview;}}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::SelectImmediate:case Stage::SelectLength:case Stage::DispatchMessage:return 8; case Stage::SelectPayload:case Stage::SelectRunningPayload:case Stage::BindPayload:return 9; case Stage::ReadRemaining:case Stage::ReadRunningStatus:case Stage::ReadChannel:case Stage::ResetFirstWord:case Stage::ResetSecondWord:return 12; case Stage::ReadLength:case Stage::ReadFlags:case Stage::ReadRunningFlags:case Stage::ReadRunningLength:return 15; case Stage::ReturnInit:case Stage::ReturnPump:case Stage::ReturnStatus:case Stage::ReturnData:case Stage::ReturnReset:return 17; case Stage::WritePayloadCursor:case Stage::ReadCursor:case Stage::WriteRunningCursor:return 18; case Stage::ResetReceive:case Stage::ResetTransmit:case Stage::ResetQueue:case Stage::ResetParser:case Stage::PopReceive:case Stage::ParseReceive:case Stage::ClearReceive:case Stage::PopQueue:case Stage::ParseQueue:case Stage::ClearQueue:case Stage::LookupStatus:case Stage::LookupRunning:case Stage::ResetSystem:case Stage::ResetSysEx:return 20; case Stage::ReadHandler:case Stage::ReadImmediateHandler:return 21; case Stage::WritePreview:return 9+store; case Stage::WriteReceived:return 9+store; case Stage::WriteExpected:return 9+store; case Stage::WriteRemaining:return 9+store; case Stage::WriteRunningStatus:return 9+store; case Stage::WriteChannel:return 9+store; case Stage::WriteData:return 3+store; case Stage::WriteDataRemaining:return 9+store; case Stage::WriteDataCursor:return 12+store; case Stage::WriteDispatchStatus:return 9+store; case Stage::WriteRunningRemaining:return 9+store; case Stage::WriteDispatchChannel:return 9+store; case Stage::ResetReceiveRemaining:return 9+store; case Stage::ResetReceiveExpected:return 9+store; case Stage::ResetInternalRemaining:return 9+store; case Stage::ResetInternalExpected:return 9+store; case Stage::ResetExclusive:return 9+store; case Stage::ResetTransfer:return 9+store; case Stage::ResetActivity:return 9+store; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ResetReceive:await(Stage::ResetTransmit);break;
case Stage::ResetTransmit:await(Stage::ResetQueue);break;
case Stage::ResetQueue:await(Stage::ResetParser);break;
case Stage::ResetParser:await(Stage::ReturnInit);break;
case Stage::ReturnInit:stage_=Stage::Complete;break;
case Stage::PopReceive:await(Stage::CompareReceiveByte);break;
case Stage::ParseReceive:await(Stage::RepeatReceive);break;
case Stage::RepeatReceive:stage_=Stage::PopReceive;break;
case Stage::ClearReceive:await(Stage::PopQueue);break;
case Stage::PopQueue:await(Stage::CompareQueueByte);break;
case Stage::ParseQueue:await(Stage::RepeatQueue);break;
case Stage::RepeatQueue:stage_=Stage::PopQueue;break;
case Stage::ClearQueue:await(Stage::ReturnPump);break;
case Stage::ReturnPump:stage_=Stage::Complete;break;
case Stage::LookupStatus:await(Stage::ReadLength);break;
case Stage::ReturnStatus:stage_=Stage::Complete;break;
case Stage::ReturnData:stage_=Stage::Complete;break;
case Stage::LookupRunning:await(Stage::ReadRunningFlags);break;
case Stage::DispatchMessage:stage_=Stage::Complete;break;
case Stage::ResetSystem:await(Stage::ResetSysEx);break;
case Stage::ResetSysEx:await(Stage::ReturnReset);break;
case Stage::ReturnReset:stage_=Stage::Complete;break;
case Stage::SelectReceiveByte:stage_=status_==1?Stage::ParseReceive:Stage::CompareReceiveEmpty;break;
case Stage::SelectReceiveReset:stage_=status_==0?Stage::PopQueue:Stage::ClearReceive;break;
case Stage::SelectQueueByte:stage_=status_==1?Stage::ParseQueue:Stage::CompareQueueEmpty;break;
case Stage::SelectQueueReset:stage_=status_==0?Stage::ReturnPump:Stage::ClearQueue;break;
case Stage::WritePreview:write(Write::Preview,1);next();break;
case Stage::ReadInput:input_=uint8_t(read(Read::Input));next();break;
case Stage::SelectInput:stage_=input_&128?Stage::WriteReceived:Stage::ReadRemaining;break;
case Stage::WriteReceived:statusByte_=uint8_t(read(Read::Input));write(Write::Received,statusByte_);next();break;
case Stage::ReadLength:length_=uint8_t(read(Read::Length));next();break;
case Stage::WriteExpected:write(Write::Expected,length_);next();break;
case Stage::ReadFlags:flags_=uint8_t(read(Read::Flags));next();break;
case Stage::SelectImmediate:stage_=flags_&2?Stage::ReadImmediateHandler:Stage::WriteRemaining;break;
case Stage::WriteRemaining:remaining_=length_;write(Write::Remaining,remaining_);next();break;
case Stage::SelectLength:stage_=remaining_?Stage::WriteRunningStatus:Stage::ReadImmediateHandler;break;
case Stage::WriteRunningStatus:statusByte_=uint8_t(read(Read::Input));write(Write::RunningStatus,statusByte_);next();break;
case Stage::CaptureChannel:channel_=uint8_t(read(Read::Input))&15;next();break;
case Stage::WriteChannel:write(Write::Channel,channel_);next();break;
case Stage::SelectPayload:cursor_=0x15c13;next();break;
case Stage::WritePayloadCursor:write(Write::Cursor,cursor_);next();break;
case Stage::ReadRemaining:remaining_=uint8_t(read(Read::Remaining));next();break;
case Stage::SelectRemaining:stage_=remaining_?Stage::ReadCursor:Stage::ReturnData;break;
case Stage::ReadCursor:cursor_=read(Read::Cursor);next();break;
case Stage::WriteData:data_=uint8_t(read(Read::Input));write(Write::Data,data_);next();break;
case Stage::DecrementRemaining:--remaining_;next();break;
case Stage::WriteDataRemaining:write(Write::Remaining,remaining_);next();break;
case Stage::SelectCompleteMessage:stage_=remaining_?Stage::AdvanceCursor:Stage::ReadRunningStatus;break;
case Stage::AdvanceCursor:++cursor_;next();break;
case Stage::WriteDataCursor:write(Write::Cursor,cursor_);next();break;
case Stage::ReadRunningStatus:statusByte_=uint8_t(read(Read::RunningStatus));next();break;
case Stage::WriteDispatchStatus:write(Write::DispatchStatus,statusByte_);next();break;
case Stage::ReadRunningFlags:flags_=uint8_t(read(Read::Flags));next();break;
case Stage::SelectRunning:stage_=flags_&1?Stage::ReadRunningLength:Stage::ReadHandler;break;
case Stage::ReadRunningLength:length_=uint8_t(read(Read::Length));next();break;
case Stage::WriteRunningRemaining:remaining_=length_;write(Write::Remaining,remaining_);next();break;
case Stage::SelectRunningPayload:cursor_=0x15c13;next();break;
case Stage::WriteRunningCursor:write(Write::Cursor,cursor_);next();break;
case Stage::ReadHandler:handler_=read(Read::Handler);next();break;
case Stage::ReadChannel:channel_=uint8_t(read(Read::Channel));next();break;
case Stage::WriteDispatchChannel:write(Write::DispatchChannel,channel_);next();break;
case Stage::ReadImmediateHandler:handler_=read(Read::Handler);next();break;
case Stage::DispatchImmediate:stage_=Stage::Complete;break;
case Stage::ResetReceiveRemaining:write(Write::ResetReceiveRemaining,0);next();break;
case Stage::ResetReceiveExpected:write(Write::ResetReceiveExpected,0);next();break;
case Stage::ResetInternalRemaining:write(Write::ResetInternalRemaining,0);next();break;
case Stage::ResetInternalExpected:write(Write::ResetInternalExpected,0);next();break;
case Stage::ResetExclusive:write(Write::ResetExclusive,0);next();break;
case Stage::ResetTransfer:write(Write::ResetTransfer,0);next();break;
case Stage::ResetActivity:write(Write::ResetActivity,0);next();break;
case Stage::ResetFirstWord:write(Write::ResetFirstWord,0);next();break;
case Stage::ResetSecondWord:write(Write::ResetSecondWord,0);next();break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint8_t input_=0,status_=0,statusByte_=0,length_=0,flags_=0,remaining_=0,channel_=0,data_=0;uint32_t cursor_=0,handler_=0;
};
}
