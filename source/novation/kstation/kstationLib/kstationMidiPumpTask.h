#pragma once
#include <cstdint>
namespace kstation {
// Foreground policy owns no parser or queue implementation. It requests each
// child explicitly and remains suspended until its owner reports completion.
class MidiPumpTask final {
public:
 enum class Stage:uint8_t { PopExternal, CompareExternalData, SelectExternalData, ParseExternal, RepeatExternal, CompareExternalError, SelectExternalError, ResetExternal, PopInternal, CompareInternalData, SelectInternalData, ParseInternal, RepeatInternal, CompareInternalError, SelectInternalError, ResetInternal, Return, AwaitChild, Complete };
 enum class Child:uint8_t { PopExternal, ParseExternal, ResetExternal, PopInternal, ParseInternal, ResetInternal };
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}Child child()const{return child_;}uint16_t result()const{return result_;}uint8_t byte()const{return uint8_t(result_);}
 bool resume(Child child,uint16_t result=0){if(!awaitingChild()||child!=child_)return false;if(child==Child::PopExternal||child==Child::PopInternal)result_=result;stage_=resume_;return true;}
 bool advance(){switch(stage_){
  case Stage::PopExternal: request(Child::PopExternal,Stage::CompareExternalData);break;
  case Stage::CompareExternalData:stage_=Stage::SelectExternalData;break;
  case Stage::SelectExternalData:stage_=(result_>>8)==1?Stage::ParseExternal:Stage::CompareExternalError;break;
  case Stage::ParseExternal:request(Child::ParseExternal,Stage::RepeatExternal);break;
  case Stage::RepeatExternal:stage_=Stage::PopExternal;break;
  case Stage::CompareExternalError:stage_=Stage::SelectExternalError;break;
  case Stage::SelectExternalError:stage_=(result_>>8)==0?Stage::PopInternal:Stage::ResetExternal;break;
  case Stage::ResetExternal:request(Child::ResetExternal,Stage::PopInternal);break;
  case Stage::PopInternal:request(Child::PopInternal,Stage::CompareInternalData);break;
  case Stage::CompareInternalData:stage_=Stage::SelectInternalData;break;
  case Stage::SelectInternalData:stage_=(result_>>8)==1?Stage::ParseInternal:Stage::CompareInternalError;break;
  case Stage::ParseInternal:request(Child::ParseInternal,Stage::RepeatInternal);break;
  case Stage::RepeatInternal:stage_=Stage::PopInternal;break;
  case Stage::CompareInternalError:stage_=Stage::SelectInternalError;break;
  case Stage::SelectInternalError:stage_=(result_>>8)==0?Stage::Return:Stage::ResetInternal;break;
  case Stage::ResetInternal:request(Child::ResetInternal,Stage::Return);break;
  case Stage::Return:stage_=Stage::Complete;break;
  case Stage::AwaitChild:case Stage::Complete:return false;
 }return true;}
private:void request(Child c,Stage s){child_=c;resume_=s;stage_=Stage::AwaitChild;}Stage stage_=Stage::PopExternal,resume_=Stage::Complete;Child child_=Child::PopExternal;uint16_t result_=0;
};

// Consumer-side queue task. Reads are deliberately phase-local, so a producer
// may update state between phases. Out-of-range 16-bit indices are preserved.
class MidiDequeueTask final {
public:
 enum class Kind:uint8_t { External, Internal };
 enum class Stage:uint8_t { ClearResult, SaveFirst, SaveSecond, LocateStatus, SaveMask, Mask, ReadStatus, ClearByte, ClearStatus, RestoreMask, ReadConsumer, ReadProducer, Compare, SelectData, LocateData, Normalize, ResolveData, Increment, MaskIndex, WriteConsumer, ReadData, MarkData, RestoreSecond, RestoreFirst, Return, Complete };
 enum class Read:uint8_t { Status, Consumer, Producer, Data };enum class Write:uint8_t { Status, Consumer };
 explicit MidiDequeueTask(Kind k):kind_(k),stage_(k==Kind::External?Stage::SaveFirst:Stage::ClearResult){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}uint16_t result()const{return uint16_t(uint16_t(status_)<<8|data_);}uint16_t consumer()const{return consumer_;}uint16_t producer()const{return producer_;}uint16_t nextConsumer()const{return next_;}uint32_t dataAddress()const{return (kind_==Kind::External?0xfffd30u:0xc9bau)+consumer_;}
 template<class R,class W> bool advance(R&&read,W&&write){switch(stage_){
  case Stage::ClearResult:status_=data_=0;stage_=Stage::SaveFirst;break;
  case Stage::SaveFirst:stage_=Stage::SaveSecond;break;
  case Stage::SaveSecond:stage_=kind_==Kind::External?Stage::LocateStatus:Stage::ReadConsumer;break;
  case Stage::LocateStatus:stage_=Stage::SaveMask;break;case Stage::SaveMask:stage_=Stage::Mask;break;case Stage::Mask:stage_=Stage::ReadStatus;break;
  case Stage::ReadStatus:status_=uint8_t(read(Read::Status));stage_=Stage::ClearByte;break;case Stage::ClearByte:data_=0;stage_=Stage::ClearStatus;break;
  case Stage::ClearStatus:write(Write::Status,0);stage_=Stage::RestoreMask;break;case Stage::RestoreMask:stage_=Stage::ReadConsumer;break;
  case Stage::ReadConsumer:consumer_=uint16_t(read(Read::Consumer));stage_=Stage::ReadProducer;break;
  case Stage::ReadProducer:producer_=uint16_t(read(Read::Producer));stage_=Stage::Compare;break;case Stage::Compare:stage_=Stage::SelectData;break;
  case Stage::SelectData:stage_=consumer_==producer_?Stage::RestoreSecond:Stage::LocateData;break;
  case Stage::LocateData:stage_=Stage::Normalize;break;case Stage::Normalize:stage_=Stage::ResolveData;break;case Stage::ResolveData:stage_=Stage::Increment;break;
  case Stage::Increment:next_=uint16_t(consumer_+1);stage_=Stage::MaskIndex;break;case Stage::MaskIndex:next_&=255;stage_=Stage::WriteConsumer;break;
  case Stage::WriteConsumer:write(Write::Consumer,next_);stage_=Stage::ReadData;break;case Stage::ReadData:data_=uint8_t(read(Read::Data));stage_=Stage::MarkData;break;
  case Stage::MarkData:status_|=1;stage_=Stage::RestoreSecond;break;case Stage::RestoreSecond:stage_=Stage::RestoreFirst;break;case Stage::RestoreFirst:stage_=Stage::Return;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::Complete:return false;
 }return true;}
private:Kind kind_;Stage stage_;uint8_t status_=0,data_=0;uint16_t consumer_=0,producer_=0,next_=0;
};
// The ROM's descriptor selector is an arithmetic service, not a dispatcher.
// The returned address is read by a parser owner at later phases.
class MidiDescriptorTask final {
public:
 enum class Stage:uint8_t { Compare, Select, MaskChannel, Shift1, Shift2, Shift3, Shift4, WidthChannel, MultiplyChannel, ExtendChannel, AddChannel, ReturnChannel, MaskSystem, WidthSystem, MultiplySystem, ExtendSystem, AddSystem, ReturnSystem, Complete };
 explicit MidiDescriptorTask(uint8_t input):input_(input),value_(input){}
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}uint32_t result()const{return result_;}
 bool advance(){switch(stage_){
  case Stage::Compare:stage_=Stage::Select;break;case Stage::Select:stage_=input_>=0xf0?Stage::MaskSystem:Stage::MaskChannel;break;
  case Stage::MaskChannel:value_&=0x70;stage_=Stage::Shift1;break;case Stage::Shift1:case Stage::Shift2:case Stage::Shift3:case Stage::Shift4:value_>>=1;stage_=Stage(unsigned(stage_)+1);break;
  case Stage::WidthChannel:stage_=Stage::MultiplyChannel;break;case Stage::MultiplyChannel:value_=uint8_t(value_*6);stage_=Stage::ExtendChannel;break;case Stage::ExtendChannel:stage_=Stage::AddChannel;break;case Stage::AddChannel:result_=0xab36+value_;stage_=Stage::ReturnChannel;break;case Stage::ReturnChannel:stage_=Stage::Complete;break;
  case Stage::MaskSystem:value_&=15;stage_=Stage::WidthSystem;break;case Stage::WidthSystem:stage_=Stage::MultiplySystem;break;case Stage::MultiplySystem:value_=uint8_t(value_*6);stage_=Stage::ExtendSystem;break;case Stage::ExtendSystem:stage_=Stage::AddSystem;break;case Stage::AddSystem:result_=0xab60+value_;stage_=Stage::ReturnSystem;break;case Stage::ReturnSystem:stage_=Stage::Complete;break;case Stage::Complete:return false;
 }return true;}
private:Stage stage_=Stage::Compare;uint8_t input_,value_;uint32_t result_=0;
};

// Both firmware parsers collect independent streams. Descriptor and event
// handlers are explicit children; no code address is executed by this task.
class MidiParserTask final {
public:
 enum class Kind:uint8_t { External, Internal };
 enum class Stage:uint8_t { CopyInput,SelectStatus,WriteReceived,ClearActivity,WriteActivity,RequestDescriptor,ReadLength,WriteExpected,ReadFlags,MaskImmediate,SelectImmediate,WriteRemaining,SelectLength,WriteRunning,MaskChannel,WriteChannel,BindCursor,WriteCursor,ReturnStatus,ReadRemaining,SelectRemaining,ReadCursor,WriteData,DecrementRemaining,WriteDataRemaining,SelectComplete,IncrementCursor,WriteDataCursor,ReturnData,ReadRunning,RequestRunningDescriptor,ReadRunningFlags,MaskRunning,SelectRunning,ReadRunningLength,WriteRunningRemaining,BindRunningCursor,WriteRunningCursor,ReadHandler,BindPayload,ReadChannel,WriteDispatchChannel,RequestHandler,PostReadStatus,PostCompareStatus,PostSelectStatus,PostBindProgram,PostReadProgramChannel,PostReadChannel,PostCompareChannel,PostSelectChannel,PostActivity,Return,AwaitChild,Complete };
 enum class Child:uint8_t { Descriptor, Handler, Activity };
 enum class Read:uint8_t { Length, Flags, Remaining, Cursor, Running, Handler, Channel, ProgramChannel };
 enum class Write:uint8_t { Received, Activity, Expected, Remaining, Running, Channel, Cursor, Data, DispatchChannel };
 MidiParserTask(Kind kind,uint8_t input):kind_(kind),input_(input){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}Child child()const{return child_;}bool descriptorForRunning()const{return awaitingChild()&&child_==Child::Descriptor&&resume_==Stage::ReadRunningFlags;}uint8_t descriptorStatus()const{return descriptorStatus_;}uint32_t descriptor()const{return descriptor_;}uint32_t handler()const{return handler_;}uint32_t cursor()const{return cursor_;}uint8_t input()const{return input_;}uint8_t remaining()const{return remaining_;}uint8_t channel()const{return channel_;}bool immediate()const{return immediate_;}
 bool resumeDescriptor(uint32_t address){if(!awaitingChild()||child_!=Child::Descriptor)return false;descriptor_=address;stage_=resume_;return true;}
 bool resumeChild(Child c){if(!awaitingChild()||child_!=c||c==Child::Descriptor)return false;stage_=resume_;return true;}
 template<class R,class W>bool advance(R&&read,W&&write){switch(stage_){
  case Stage::CopyInput:stage_=Stage::SelectStatus;break;case Stage::SelectStatus:stage_=(input_&0x80)?Stage::WriteReceived:Stage::ReadRemaining;break;
  case Stage::WriteReceived:write(Write::Received,input_);stage_=kind_==Kind::External?Stage::ClearActivity:Stage::RequestDescriptor;break;
  case Stage::ClearActivity:stage_=Stage::WriteActivity;break;case Stage::WriteActivity:write(Write::Activity,0);stage_=Stage::RequestDescriptor;break;
  case Stage::RequestDescriptor:descriptorStatus_=input_;request(Child::Descriptor,Stage::ReadLength);break;
  case Stage::ReadLength:length_=uint8_t(read(Read::Length));stage_=Stage::WriteExpected;break;case Stage::WriteExpected:write(Write::Expected,length_);stage_=Stage::ReadFlags;break;
  case Stage::ReadFlags:flags_=uint8_t(read(Read::Flags));stage_=Stage::MaskImmediate;break;case Stage::MaskImmediate:stage_=Stage::SelectImmediate;break;
  case Stage::SelectImmediate:immediate_=bool(flags_&2);stage_=immediate_?Stage::ReadHandler:Stage::WriteRemaining;break;
  case Stage::WriteRemaining:remaining_=length_;write(Write::Remaining,remaining_);stage_=Stage::SelectLength;break;
  case Stage::SelectLength:immediate_=remaining_==0;stage_=immediate_?Stage::ReadHandler:Stage::WriteRunning;break;
  case Stage::WriteRunning:write(Write::Running,input_);stage_=Stage::MaskChannel;break;case Stage::MaskChannel:channel_=input_&15;stage_=Stage::WriteChannel;break;
  case Stage::WriteChannel:write(Write::Channel,channel_);stage_=Stage::BindCursor;break;case Stage::BindCursor:cursor_=payload();stage_=Stage::WriteCursor;break;
  case Stage::WriteCursor:write(Write::Cursor,cursor_);stage_=Stage::ReturnStatus;break;case Stage::ReturnStatus:stage_=Stage::Complete;break;
  case Stage::ReadRemaining:remaining_=uint8_t(read(Read::Remaining));stage_=Stage::SelectRemaining;break;case Stage::SelectRemaining:stage_=remaining_?Stage::ReadCursor:Stage::ReturnData;break;
  case Stage::ReadCursor:cursor_=read(Read::Cursor);stage_=Stage::WriteData;break;case Stage::WriteData:write(Write::Data,input_);stage_=Stage::DecrementRemaining;break;
  case Stage::DecrementRemaining:--remaining_;stage_=Stage::WriteDataRemaining;break;case Stage::WriteDataRemaining:write(Write::Remaining,remaining_);stage_=Stage::SelectComplete;break;
  case Stage::SelectComplete:stage_=remaining_?Stage::IncrementCursor:Stage::ReadRunning;break;case Stage::IncrementCursor:++cursor_;stage_=Stage::WriteDataCursor;break;
  case Stage::WriteDataCursor:write(Write::Cursor,cursor_);stage_=Stage::ReturnData;break;case Stage::ReturnData:stage_=Stage::Complete;break;
  case Stage::ReadRunning:descriptorStatus_=uint8_t(read(Read::Running));stage_=Stage::RequestRunningDescriptor;break;case Stage::RequestRunningDescriptor:request(Child::Descriptor,Stage::ReadRunningFlags);break;
  case Stage::ReadRunningFlags:flags_=uint8_t(read(Read::Flags));stage_=Stage::MaskRunning;break;case Stage::MaskRunning:stage_=Stage::SelectRunning;break;
  case Stage::SelectRunning:stage_=(flags_&1)?Stage::ReadRunningLength:Stage::ReadHandler;break;
  case Stage::ReadRunningLength:length_=uint8_t(read(Read::Length));stage_=Stage::WriteRunningRemaining;break;case Stage::WriteRunningRemaining:remaining_=length_;write(Write::Remaining,remaining_);stage_=Stage::BindRunningCursor;break;
  case Stage::BindRunningCursor:cursor_=payload();stage_=Stage::WriteRunningCursor;break;case Stage::WriteRunningCursor:write(Write::Cursor,cursor_);stage_=Stage::ReadHandler;break;
  case Stage::ReadHandler:handler_=read(Read::Handler);stage_=immediate_?Stage::RequestHandler:Stage::BindPayload;break;
  case Stage::BindPayload:stage_=Stage::ReadChannel;break;case Stage::ReadChannel:channel_=uint8_t(read(Read::Channel));stage_=Stage::WriteDispatchChannel;break;
  case Stage::WriteDispatchChannel:write(Write::DispatchChannel,channel_);stage_=Stage::RequestHandler;break;
  case Stage::RequestHandler:request(Child::Handler,immediate_||kind_==Kind::Internal?Stage::Return:Stage::PostReadStatus);break;
  case Stage::PostReadStatus:descriptorStatus_=uint8_t(read(Read::Running));stage_=Stage::PostCompareStatus;break;case Stage::PostCompareStatus:stage_=Stage::PostSelectStatus;break;
  case Stage::PostSelectStatus:stage_=descriptorStatus_>=0xf0?Stage::Return:Stage::PostBindProgram;break;case Stage::PostBindProgram:stage_=Stage::PostReadProgramChannel;break;
  case Stage::PostReadProgramChannel:programChannel_=uint8_t(read(Read::ProgramChannel));stage_=Stage::PostReadChannel;break;case Stage::PostReadChannel:channel_=uint8_t(read(Read::Channel));stage_=Stage::PostCompareChannel;break;
  case Stage::PostCompareChannel:stage_=Stage::PostSelectChannel;break;case Stage::PostSelectChannel:stage_=programChannel_==channel_?Stage::PostActivity:Stage::Return;break;
  case Stage::PostActivity:request(Child::Activity,Stage::Return);break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitChild:case Stage::Complete:return false;
 }return true;}
private:
 uint32_t payload()const{return kind_==Kind::External?0xcad3:0xcac5;}void request(Child c,Stage next){child_=c;resume_=next;stage_=Stage::AwaitChild;}
 Kind kind_;Stage stage_=Stage::CopyInput,resume_=Stage::Complete;Child child_=Child::Descriptor;uint8_t input_,descriptorStatus_=0,length_=0,flags_=0,remaining_=0,channel_=0,programChannel_=0;uint32_t cursor_=0,descriptor_=0,handler_=0;bool immediate_=false;
};

class MidiUtilityTask final {
public:
 enum class Kind:uint8_t { ResetExternal,ResetInternal,Activity };
 enum class Stage:uint8_t { SaveMask,Mask,SaveWord,Clear,WriteProducer,WriteConsumer,WriteStatus,RestoreWord,RestoreMask,ReadPort,TogglePort,WritePort,PrepareHold,WriteHold,Return,Complete };
 enum class Read:uint8_t { Port };enum class Write:uint8_t { Producer,Consumer,Status,Port,Hold };
 explicit MidiUtilityTask(Kind k):kind_(k),stage_(k==Kind::ResetExternal?Stage::SaveMask:Stage::SaveWord){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}uint8_t port()const{return port_;}
 template<class R,class W>bool advance(R&&read,W&&write){switch(stage_){
 case Stage::SaveMask:stage_=Stage::Mask;break;case Stage::Mask:stage_=Stage::SaveWord;break;case Stage::SaveWord:stage_=kind_==Kind::Activity?Stage::ReadPort:Stage::Clear;break;
 case Stage::Clear:stage_=Stage::WriteProducer;break;case Stage::WriteProducer:write(Write::Producer,0);stage_=Stage::WriteConsumer;break;case Stage::WriteConsumer:write(Write::Consumer,0);stage_=Stage::WriteStatus;break;case Stage::WriteStatus:write(Write::Status,0);stage_=Stage::RestoreWord;break;
 case Stage::RestoreWord:stage_=kind_==Kind::ResetExternal?Stage::RestoreMask:Stage::Return;break;case Stage::RestoreMask:stage_=Stage::Return;break;
 case Stage::ReadPort:port_=uint8_t(read(Read::Port));stage_=Stage::TogglePort;break;case Stage::TogglePort:port_^=0x10;stage_=Stage::WritePort;break;case Stage::WritePort:write(Write::Port,port_);stage_=Stage::PrepareHold;break;case Stage::PrepareHold:stage_=Stage::WriteHold;break;case Stage::WriteHold:write(Write::Hold,5);stage_=Stage::RestoreWord;break;
 case Stage::Return:stage_=Stage::Complete;break;case Stage::Complete:return false;}return true;}
private:Kind kind_;Stage stage_;uint8_t port_=0;
};

}
