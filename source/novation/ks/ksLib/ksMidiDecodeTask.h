#pragma once
#include "ksMidiDescriptorTask.h"
#include <cstdint>
namespace ks {
// Incremental MIDI input. Descriptor flags select immediate/system handling or
// sized/running-status collection. The descriptor handler is a typed request;
// only the migration bridge projects its original table function reference.
class MidiDecodeTask final {
public:
 enum class Stage {ClearPreview,PublishPreview,ReadInput,AdmitStatus,PublishReceived,ClearActivity,PublishActivity,DispatchStatusDescriptor,AwaitStatusDescriptor,
 ReadStatusLength,PublishExpected,ReadStatusFlags,MaskImmediate,AdmitImmediate,PublishStatusRemaining,AdmitStatusLength,PublishRunningStatus,MaskChannel,PublishChannel,
 SetDataCursor,PublishStatusCursor,FinishStatus,ReadImmediateHandler,TailImmediate,
 ReadRemaining,AdmitRemaining,ReadCursor,PublishData,DecrementRemaining,PublishDataRemaining,AdmitMessage,AdvanceCursor,PublishDataCursor,FinishData,
 ReadRunningStatus,PublishDispatchStatus,DispatchMessageDescriptor,AwaitMessageDescriptor,ReadRunningFlags,MaskRunning,AdmitRunning,ReadRunningLength,PublishRunningRemaining,
 ResetDataCursor,PublishRunningCursor,ReadMessageHandler,BindPayload,ReadChannel,PublishDispatchChannel,DispatchMessage,AwaitMessage,ReturnToLoop,HandoffImmediate,HandoffLoop,Complete};
 enum class Read {Input,Length,Flags,Remaining,Cursor,RunningStatus,Handler,Channel};
 enum class Write {Preview,ReceivedStatus,Activity,Expected,Remaining,RunningStatus,Channel,Cursor,Data,DispatchStatus,DispatchChannel};
 Stage stage()const{return stage_;}uint8_t input()const{return input_;}uint8_t received()const{return received_;}uint8_t status()const{return status_;}uint8_t channel()const{return channel_;}
 uint8_t length()const{return length_;}uint8_t flags()const{return flags_;}uint8_t remaining()const{return remaining_;}uint8_t data()const{return data_;}uint32_t cursor()const{return cursor_;}
 MidiDescriptorKey descriptor()const{return MidiDescriptorKey::forStatus(status_);}
 bool awaitingDescriptor()const{return stage_==Stage::AwaitStatusDescriptor||stage_==Stage::AwaitMessageDescriptor;}
 bool resumeDescriptor(){if(!awaitingDescriptor())return false;stage_=stage_==Stage::AwaitStatusDescriptor?Stage::ReadStatusLength:Stage::ReadRunningFlags;return true;}
 bool awaitingMessage()const{return stage_==Stage::AwaitMessage;}bool resumeMessage(){if(!awaitingMessage())return false;stage_=Stage::ReturnToLoop;return true;}
 bool handoff()const{return stage_==Stage::HandoffImmediate||stage_==Stage::HandoffLoop;}bool complete()const{return stage_==Stage::Complete;}
 bool publication()const{switch(stage_){case Stage::PublishPreview:case Stage::PublishReceived:case Stage::PublishActivity:case Stage::PublishExpected:case Stage::PublishStatusRemaining:case Stage::PublishRunningStatus:case Stage::PublishChannel:case Stage::PublishData:case Stage::PublishDataRemaining:case Stage::PublishDispatchStatus:case Stage::PublishRunningRemaining:case Stage::PublishDispatchChannel:return true;default:return false;}}
 Write publicationKind()const{switch(stage_){case Stage::PublishPreview:return Write::Preview;case Stage::PublishReceived:return Write::ReceivedStatus;case Stage::PublishActivity:return Write::Activity;case Stage::PublishExpected:return Write::Expected;case Stage::PublishStatusRemaining:case Stage::PublishDataRemaining:case Stage::PublishRunningRemaining:return Write::Remaining;case Stage::PublishRunningStatus:return Write::RunningStatus;case Stage::PublishChannel:return Write::Channel;case Stage::PublishData:return Write::Data;case Stage::PublishDispatchStatus:return Write::DispatchStatus;default:return Write::DispatchChannel;}}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){
 case Stage::PublishPreview:case Stage::PublishReceived:case Stage::PublishActivity:case Stage::PublishExpected:case Stage::PublishStatusRemaining:case Stage::PublishRunningStatus:case Stage::PublishChannel:case Stage::PublishDataRemaining:case Stage::PublishDispatchStatus:case Stage::PublishRunningRemaining:case Stage::PublishDispatchChannel:return 9+store;
 case Stage::PublishData:return 3+store;case Stage::ReadStatusLength:case Stage::ReadStatusFlags:case Stage::ReadRunningFlags:case Stage::ReadRunningLength:return 15;
 case Stage::DispatchStatusDescriptor:case Stage::DispatchMessage:return 20;case Stage::DispatchMessageDescriptor:return 15;
 case Stage::ReadImmediateHandler:case Stage::ReadMessageHandler:return 21;case Stage::ReadCursor:return 18;
 case Stage::PublishStatusCursor:case Stage::PublishDataCursor:case Stage::PublishRunningCursor:return 18;
 case Stage::SetDataCursor:case Stage::ResetDataCursor:case Stage::BindPayload:return 9;
 case Stage::ReadRemaining:case Stage::ReadRunningStatus:case Stage::ReadChannel:return 12;
 case Stage::FinishStatus:case Stage::FinishData:return 17;case Stage::ReturnToLoop:return 8;
 case Stage::AwaitStatusDescriptor:case Stage::AwaitMessageDescriptor:case Stage::AwaitMessage:case Stage::HandoffImmediate:case Stage::HandoffLoop:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationKind()):3)){
  if(cost>available-used)break;
  switch(stage_){
  case Stage::ClearPreview:stage_=Stage::PublishPreview;break;case Stage::PublishPreview:write(Write::Preview,0);stage_=Stage::ReadInput;break;
  case Stage::ReadInput:input_=status_=uint8_t(read(Read::Input));stage_=Stage::AdmitStatus;break;case Stage::AdmitStatus:stage_=input_>=128?Stage::PublishReceived:Stage::ReadRemaining;break;
  case Stage::PublishReceived:received_=uint8_t(read(Read::Input));write(Write::ReceivedStatus,received_);stage_=Stage::ClearActivity;break;
  case Stage::ClearActivity:stage_=Stage::PublishActivity;break;case Stage::PublishActivity:write(Write::Activity,0);stage_=Stage::DispatchStatusDescriptor;break;
  case Stage::DispatchStatusDescriptor:stage_=Stage::AwaitStatusDescriptor;break;
  case Stage::ReadStatusLength:length_=uint8_t(read(Read::Length));stage_=Stage::PublishExpected;break;case Stage::PublishExpected:write(Write::Expected,length_);stage_=Stage::ReadStatusFlags;break;
  case Stage::ReadStatusFlags:flags_=uint8_t(read(Read::Flags));stage_=Stage::MaskImmediate;break;case Stage::MaskImmediate:stage_=Stage::AdmitImmediate;break;
  case Stage::AdmitImmediate:stage_=(flags_&2)?Stage::ReadImmediateHandler:Stage::PublishStatusRemaining;break;
  case Stage::PublishStatusRemaining:remaining_=length_;write(Write::Remaining,remaining_);stage_=Stage::AdmitStatusLength;break;
  case Stage::AdmitStatusLength:stage_=remaining_?Stage::PublishRunningStatus:Stage::ReadImmediateHandler;break;
  case Stage::PublishRunningStatus:status_=uint8_t(read(Read::Input));write(Write::RunningStatus,status_);stage_=Stage::MaskChannel;break;
  case Stage::MaskChannel:channel_=uint8_t(read(Read::Input))&15;stage_=Stage::PublishChannel;break;case Stage::PublishChannel:write(Write::Channel,channel_);stage_=Stage::SetDataCursor;break;
  case Stage::SetDataCursor:cursor_=0x15c1f;stage_=Stage::PublishStatusCursor;break;case Stage::PublishStatusCursor:write(Write::Cursor,cursor_);stage_=Stage::FinishStatus;break;
  case Stage::FinishStatus:stage_=Stage::Complete;break;case Stage::ReadImmediateHandler:read(Read::Handler);stage_=Stage::TailImmediate;break;case Stage::TailImmediate:stage_=Stage::HandoffImmediate;break;
  case Stage::ReadRemaining:remaining_=uint8_t(read(Read::Remaining));stage_=Stage::AdmitRemaining;break;case Stage::AdmitRemaining:stage_=remaining_?Stage::ReadCursor:Stage::FinishData;break;
  case Stage::ReadCursor:cursor_=uint32_t(read(Read::Cursor));stage_=Stage::PublishData;break;case Stage::PublishData:data_=uint8_t(read(Read::Input));write(Write::Data,data_);stage_=Stage::DecrementRemaining;break;
  case Stage::DecrementRemaining:--remaining_;stage_=Stage::PublishDataRemaining;break;case Stage::PublishDataRemaining:write(Write::Remaining,remaining_);stage_=Stage::AdmitMessage;break;
  case Stage::AdmitMessage:stage_=remaining_?Stage::AdvanceCursor:Stage::ReadRunningStatus;break;case Stage::AdvanceCursor:++cursor_;stage_=Stage::PublishDataCursor;break;
  case Stage::PublishDataCursor:write(Write::Cursor,cursor_);stage_=Stage::FinishData;break;case Stage::FinishData:stage_=Stage::Complete;break;
  case Stage::ReadRunningStatus:status_=uint8_t(read(Read::RunningStatus));stage_=Stage::PublishDispatchStatus;break;case Stage::PublishDispatchStatus:write(Write::DispatchStatus,status_);stage_=Stage::DispatchMessageDescriptor;break;
  case Stage::DispatchMessageDescriptor:stage_=Stage::AwaitMessageDescriptor;break;case Stage::ReadRunningFlags:flags_=uint8_t(read(Read::Flags));stage_=Stage::MaskRunning;break;
  case Stage::MaskRunning:stage_=Stage::AdmitRunning;break;case Stage::AdmitRunning:stage_=(flags_&1)?Stage::ReadRunningLength:Stage::ReadMessageHandler;break;
  case Stage::ReadRunningLength:length_=uint8_t(read(Read::Length));stage_=Stage::PublishRunningRemaining;break;case Stage::PublishRunningRemaining:remaining_=length_;write(Write::Remaining,remaining_);stage_=Stage::ResetDataCursor;break;
  case Stage::ResetDataCursor:cursor_=0x15c1f;stage_=Stage::PublishRunningCursor;break;case Stage::PublishRunningCursor:write(Write::Cursor,cursor_);stage_=Stage::ReadMessageHandler;break;
  case Stage::ReadMessageHandler:read(Read::Handler);stage_=Stage::BindPayload;break;case Stage::BindPayload:stage_=Stage::ReadChannel;break;
  case Stage::ReadChannel:channel_=uint8_t(read(Read::Channel));stage_=Stage::PublishDispatchChannel;break;case Stage::PublishDispatchChannel:write(Write::DispatchChannel,channel_);stage_=Stage::DispatchMessage;break;
  case Stage::DispatchMessage:stage_=Stage::AwaitMessage;break;case Stage::ReturnToLoop:stage_=Stage::HandoffLoop;break;default:break;
  }used+=cost;
 }return used;}
private:Stage stage_=Stage::ClearPreview;uint8_t input_=0,received_=0,status_=0,channel_=0,length_=0,flags_=0,remaining_=0,data_=0;uint32_t cursor_=0;
};
}
