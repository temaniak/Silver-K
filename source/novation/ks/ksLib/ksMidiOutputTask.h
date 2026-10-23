#pragma once
#include <cstdint>
namespace ks {
// MIDI packet construction and input/local/output routing. Child services are
// explicit suspension points; channel/global observations remain live.
class MidiOutputTask final {
public:
 enum class Kind { NoteOff, NoteOn, Pedal, Expression, PitchBend, Modulation, Pressure, Byte, Empty, RouteOff, RouteOn, QueueOff, QueueOn, TransmitOff, TransmitOn, Local, Panic, Control, RawControl, Program };
 enum class Stage { SelectOffChannel, OffStatus, BindOffPacket, SelectOnChannel, OnStatus, SendNoteStatus, BindNote, SendNote, BindVelocity, FinishNote, SavePedal, SelectPedalChannel, PedalStatus, SendPedalStatus, BindPedalController, SendPedalController, RestorePedal, BindPedalValue, FinishPedal, SaveExpression, SelectExpressionChannel, ExpressionStatus, SendExpressionStatus, BindExpressionController, SendExpressionController, RestoreExpression, BindExpressionValue, FinishExpression, SaveBend, SelectBendChannel, BendStatus, SendBendStatus, RestoreBend, ClearBendLow, HalfBend, SelectBendLow, SetBendLow, CompareBendEnd, SelectBendEnd, SetBendEnd, SaveBendHigh, SendBendLow, RestoreBendHigh, BindBendHigh, FinishBend, SaveModulation, SelectModulationChannel, ModulationStatus, SendModulationStatus, BindModulationController, SendModulationController, RestoreModulation, BindModulationValue, FinishModulation, SavePressure, SelectPressureChannel, PressureStatus, SendPressureStatus, RestorePressure, BindPressureValue, FinishPressure, SaveByteContext, SelectByteGlobals, ReadByteMode, CompareByteMode, SelectByteQueue, QueueByte, RestoreByteContext, FinishByte, ReturnEmpty, SaveOffContext, SelectOffGlobals, ReadOffMode, CompareOffMode, SelectOffQueue, SaveOffEvent, QueueOffEvent, RestoreOffEvent, CheckOffLocal, SelectOffLocal, SendOffEvent, RestoreOffContext, ReturnOff, SaveOnContext, SelectOnGlobals, ReadOnMode, CompareOnMode, SelectOnQueue, SaveOnEvent, QueueOnEvent, RestoreOnEvent, CheckOnLocal, SelectOnLocal, SendOnEvent, RestoreOnContext, ReturnOn, SaveQueueOff, SelectQueueOffChannel, QueueOffStatus, BindQueueOff, SaveQueueOn, SelectQueueOnChannel, QueueOnStatus, QueueNoteStatus, RestoreQueueNote, QueueNote, BindQueueVelocity, FinishQueueNote, SaveTransmitOff, SelectTransmitOffChannel, TransmitOffStatus, BindTransmitOff, SaveTransmitOn, SelectTransmitOnChannel, TransmitOnStatus, TransmitNoteStatus, RestoreTransmitNote, TransmitNote, BindTransmitVelocity, FinishTransmitNote, SaveLocalEvent, SelectLocalPart, WriteLocalPart, RestoreLocalEvent, LocateLocalControls, ReadLocalFlags, TestLocalEnable, SelectLocalEnable, MaskLocalFlags, FinishLocalFlags, ReturnLocal, LocatePanicControls, ReadPanicChannel, PanicStatus, SendPanicStatus, BindPanicController, SendPanicController, BindPanicValue, SendPanicValue, ReturnPanic, SaveControlEvent, CompareSpecialControl, SelectControlChannel, SaveControlContext, SelectControlGlobals, ReadControlChannel, RestoreControlContext, BindControlChannel, SaveRawControlEvent, SelectRawControlChannel, ControlStatus, SendControlStatus, RestoreControlEvent, SendControlIndex, BindControlValue, FinishControl, SaveProgramEvent, SelectProgramChannel, ProgramStatus, SendProgramStatus, RestoreProgramEvent, FinishProgram, AwaitChild, Complete };
 enum class Read { SendMode, LocalFlags, PanicChannel, ControlChannel };enum class Write { SelectedPart };
 MidiOutputTask(Kind kind,uint16_t event,uint16_t note,uint8_t control):event_(event),note_(uint8_t(note)),velocity_(uint8_t(note>>8)),control_(control){switch(kind){case Kind::NoteOff:stage_=Stage::SelectOffChannel;break;case Kind::NoteOn:stage_=Stage::SelectOnChannel;break;case Kind::Pedal:stage_=Stage::SavePedal;break;case Kind::Expression:stage_=Stage::SaveExpression;break;case Kind::PitchBend:stage_=Stage::SaveBend;break;case Kind::Modulation:stage_=Stage::SaveModulation;break;case Kind::Pressure:stage_=Stage::SavePressure;break;case Kind::Byte:stage_=Stage::SaveByteContext;break;case Kind::Empty:stage_=Stage::ReturnEmpty;break;case Kind::RouteOff:stage_=Stage::SaveOffContext;break;case Kind::RouteOn:stage_=Stage::SaveOnContext;break;case Kind::QueueOff:stage_=Stage::SaveQueueOff;break;case Kind::QueueOn:stage_=Stage::SaveQueueOn;break;case Kind::TransmitOff:stage_=Stage::SaveTransmitOff;break;case Kind::TransmitOn:stage_=Stage::SaveTransmitOn;break;case Kind::Local:stage_=Stage::SaveLocalEvent;break;case Kind::Panic:stage_=Stage::LocatePanicControls;break;case Kind::Control:stage_=Stage::SaveControlEvent;break;case Kind::RawControl:stage_=Stage::SaveRawControlEvent;break;case Kind::Program:stage_=Stage::SaveProgramEvent;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint16_t event,uint32_t address,bool local){if(!awaitingChild())return false;event_=event;address_=address;local_=local;stage_=resume_;return true;}
 uint8_t low()const{return uint8_t(event_);}uint8_t high()const{return uint8_t(event_>>8);}uint16_t event()const{return event_;}uint8_t mode()const{return mode_;}uint8_t flags()const{return flags_;}uint32_t address()const{return address_;}
 unsigned nextClocks(unsigned dataRead=3,unsigned partWrite=3)const{switch(stage_){case Stage::FinishNote:case Stage::FinishByte:case Stage::FinishQueueNote:case Stage::FinishTransmitNote:case Stage::FinishControl:case Stage::FinishProgram:return 8; case Stage::SelectByteGlobals:case Stage::SelectOffGlobals:case Stage::SelectOnGlobals:case Stage::SelectControlGlobals:return 9; case Stage::SavePedal:case Stage::RestorePedal:case Stage::SaveExpression:case Stage::RestoreExpression:case Stage::SaveBend:case Stage::RestoreBend:case Stage::SaveBendHigh:case Stage::RestoreBendHigh:case Stage::SaveModulation:case Stage::RestoreModulation:case Stage::SavePressure:case Stage::RestorePressure:case Stage::SaveOffEvent:case Stage::RestoreOffEvent:case Stage::SaveOnEvent:case Stage::RestoreOnEvent:case Stage::SaveQueueOff:case Stage::SaveQueueOn:case Stage::RestoreQueueNote:case Stage::SaveTransmitOff:case Stage::SaveTransmitOn:case Stage::RestoreTransmitNote:case Stage::SaveLocalEvent:case Stage::RestoreLocalEvent:case Stage::SaveControlEvent:case Stage::SaveRawControlEvent:case Stage::RestoreControlEvent:case Stage::SaveProgramEvent:case Stage::RestoreProgramEvent:return 11; case Stage::SendExpressionStatus:case Stage::SendExpressionController:case Stage::SendBendStatus:case Stage::SendBendLow:case Stage::SendModulationStatus:case Stage::SendModulationController:case Stage::SendPressureStatus:case Stage::ReadByteMode:case Stage::ReadOffMode:case Stage::QueueOffEvent:case Stage::SendOffEvent:case Stage::ReadOnMode:case Stage::QueueOnEvent:case Stage::CheckOnLocal:case Stage::SendOnEvent:case Stage::ReadControlChannel:return 15; case Stage::ReturnEmpty:case Stage::ReturnOff:case Stage::ReturnOn:case Stage::ReturnLocal:case Stage::ReturnPanic:return 17; case Stage::SelectOffChannel:case Stage::SelectOnChannel:case Stage::SendNoteStatus:case Stage::SendNote:case Stage::SelectPedalChannel:case Stage::SendPedalStatus:case Stage::SendPedalController:case Stage::SelectExpressionChannel:case Stage::SelectBendChannel:case Stage::SelectModulationChannel:case Stage::SelectPressureChannel:case Stage::SaveByteContext:case Stage::QueueByte:case Stage::RestoreByteContext:case Stage::SaveOffContext:case Stage::CheckOffLocal:case Stage::RestoreOffContext:case Stage::SaveOnContext:case Stage::RestoreOnContext:case Stage::SelectQueueOffChannel:case Stage::SelectQueueOnChannel:case Stage::QueueNoteStatus:case Stage::QueueNote:case Stage::SelectTransmitOffChannel:case Stage::SelectTransmitOnChannel:case Stage::TransmitNoteStatus:case Stage::TransmitNote:case Stage::SelectLocalPart:case Stage::LocateLocalControls:case Stage::LocatePanicControls:case Stage::SendPanicStatus:case Stage::SendPanicController:case Stage::SendPanicValue:case Stage::SaveControlContext:case Stage::RestoreControlContext:case Stage::SelectRawControlChannel:case Stage::SendControlStatus:case Stage::SendControlIndex:case Stage::SelectProgramChannel:case Stage::SendProgramStatus:return 20; case Stage::ReadLocalFlags:return 12+dataRead;case Stage::ReadPanicChannel:return 12+dataRead;case Stage::WriteLocalPart:return 9+partWrite;case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned dataRead=3,unsigned partWrite=3){const auto cost=nextClocks(dataRead,partWrite);if(!cost||cost>available)return 0;switch(stage_){case Stage::SelectOffChannel:await(Stage::OffStatus);break;
case Stage::BindOffPacket:stage_=Stage::SendNoteStatus;break;
case Stage::SelectOnChannel:await(Stage::OnStatus);break;
case Stage::SendNoteStatus:await(Stage::BindNote);break;
case Stage::SendNote:await(Stage::BindVelocity);break;
case Stage::FinishNote:stage_=Stage::Complete;break;
case Stage::SelectPedalChannel:await(Stage::PedalStatus);break;
case Stage::SendPedalStatus:await(Stage::BindPedalController);break;
case Stage::SendPedalController:await(Stage::RestorePedal);break;
case Stage::FinishPedal:stage_=Stage::Complete;break;
case Stage::SelectExpressionChannel:await(Stage::ExpressionStatus);break;
case Stage::SendExpressionStatus:await(Stage::BindExpressionController);break;
case Stage::SendExpressionController:await(Stage::RestoreExpression);break;
case Stage::FinishExpression:stage_=Stage::Complete;break;
case Stage::SelectBendChannel:await(Stage::BendStatus);break;
case Stage::SendBendStatus:await(Stage::RestoreBend);break;
case Stage::SendBendLow:await(Stage::RestoreBendHigh);break;
case Stage::FinishBend:stage_=Stage::Complete;break;
case Stage::SelectModulationChannel:await(Stage::ModulationStatus);break;
case Stage::SendModulationStatus:await(Stage::BindModulationController);break;
case Stage::SendModulationController:await(Stage::RestoreModulation);break;
case Stage::FinishModulation:stage_=Stage::Complete;break;
case Stage::SelectPressureChannel:await(Stage::PressureStatus);break;
case Stage::SendPressureStatus:await(Stage::RestorePressure);break;
case Stage::FinishPressure:stage_=Stage::Complete;break;
case Stage::QueueByte:await(Stage::RestoreByteContext);break;
case Stage::FinishByte:stage_=Stage::Complete;break;
case Stage::ReturnEmpty:stage_=Stage::Complete;break;
case Stage::QueueOffEvent:await(Stage::RestoreOffEvent);break;
case Stage::CheckOffLocal:await(Stage::SelectOffLocal);break;
case Stage::SendOffEvent:await(Stage::RestoreOffContext);break;
case Stage::ReturnOff:stage_=Stage::Complete;break;
case Stage::QueueOnEvent:await(Stage::RestoreOnEvent);break;
case Stage::CheckOnLocal:await(Stage::SelectOnLocal);break;
case Stage::SendOnEvent:await(Stage::RestoreOnContext);break;
case Stage::ReturnOn:stage_=Stage::Complete;break;
case Stage::SelectQueueOffChannel:await(Stage::QueueOffStatus);break;
case Stage::BindQueueOff:stage_=Stage::QueueNoteStatus;break;
case Stage::SelectQueueOnChannel:await(Stage::QueueOnStatus);break;
case Stage::QueueNoteStatus:await(Stage::RestoreQueueNote);break;
case Stage::QueueNote:await(Stage::BindQueueVelocity);break;
case Stage::FinishQueueNote:stage_=Stage::Complete;break;
case Stage::SelectTransmitOffChannel:await(Stage::TransmitOffStatus);break;
case Stage::BindTransmitOff:stage_=Stage::TransmitNoteStatus;break;
case Stage::SelectTransmitOnChannel:await(Stage::TransmitOnStatus);break;
case Stage::TransmitNoteStatus:await(Stage::RestoreTransmitNote);break;
case Stage::TransmitNote:await(Stage::BindTransmitVelocity);break;
case Stage::FinishTransmitNote:stage_=Stage::Complete;break;
case Stage::SelectLocalPart:await(Stage::WriteLocalPart);break;
case Stage::LocateLocalControls:await(Stage::ReadLocalFlags);break;
case Stage::ReturnLocal:stage_=Stage::Complete;break;
case Stage::LocatePanicControls:await(Stage::ReadPanicChannel);break;
case Stage::SendPanicStatus:await(Stage::BindPanicController);break;
case Stage::SendPanicController:await(Stage::BindPanicValue);break;
case Stage::SendPanicValue:await(Stage::ReturnPanic);break;
case Stage::ReturnPanic:stage_=Stage::Complete;break;
case Stage::BindControlChannel:stage_=Stage::ControlStatus;break;
case Stage::SelectRawControlChannel:await(Stage::ControlStatus);break;
case Stage::SendControlStatus:await(Stage::RestoreControlEvent);break;
case Stage::SendControlIndex:await(Stage::BindControlValue);break;
case Stage::FinishControl:stage_=Stage::Complete;break;
case Stage::SelectProgramChannel:await(Stage::ProgramStatus);break;
case Stage::SendProgramStatus:await(Stage::RestoreProgramEvent);break;
case Stage::FinishProgram:stage_=Stage::Complete;break;
case Stage::SavePedal:saved_=event_;next();break;
case Stage::SaveExpression:saved_=event_;next();break;
case Stage::SaveBend:saved_=event_;next();break;
case Stage::SaveModulation:saved_=event_;next();break;
case Stage::SavePressure:saved_=event_;next();break;
case Stage::SaveOffEvent:saved_=event_;next();break;
case Stage::SaveOnEvent:saved_=event_;next();break;
case Stage::SaveQueueOff:saved_=event_;next();break;
case Stage::SaveQueueOn:saved_=event_;next();break;
case Stage::SaveTransmitOff:saved_=event_;next();break;
case Stage::SaveTransmitOn:saved_=event_;next();break;
case Stage::SaveLocalEvent:saved_=event_;next();break;
case Stage::SaveControlEvent:saved_=event_;next();break;
case Stage::SaveRawControlEvent:saved_=event_;next();break;
case Stage::SaveProgramEvent:saved_=event_;next();break;
case Stage::RestorePedal:event_=saved_;next();break;
case Stage::RestoreExpression:event_=saved_;next();break;
case Stage::RestoreBend:event_=saved_;next();break;
case Stage::RestoreModulation:event_=saved_;next();break;
case Stage::RestorePressure:event_=saved_;next();break;
case Stage::RestoreOffEvent:event_=saved_;next();break;
case Stage::RestoreOnEvent:event_=saved_;next();break;
case Stage::RestoreQueueNote:event_=saved_;next();break;
case Stage::RestoreTransmitNote:event_=saved_;next();break;
case Stage::RestoreLocalEvent:event_=saved_;next();break;
case Stage::RestoreControlEvent:event_=saved_;next();break;
case Stage::RestoreProgramEvent:event_=saved_;next();break;
case Stage::OffStatus:setLow(low()|0x80);next();break;
case Stage::QueueOffStatus:setLow(low()|0x80);next();break;
case Stage::TransmitOffStatus:setLow(low()|0x80);next();break;
case Stage::OnStatus:setLow(low()|0x90);next();break;
case Stage::QueueOnStatus:setLow(low()|0x90);next();break;
case Stage::TransmitOnStatus:setLow(low()|0x90);next();break;
case Stage::PedalStatus:setLow(low()|0xb0);next();break;
case Stage::ExpressionStatus:setLow(low()|0xb0);next();break;
case Stage::ModulationStatus:setLow(low()|0xb0);next();break;
case Stage::ControlStatus:setLow(low()|0xb0);next();break;
case Stage::PanicStatus:setLow(low()|0xb0);next();break;
case Stage::BendStatus:setLow(low()|0xe0);next();break;
case Stage::PressureStatus:setLow(low()|0xd0);next();break;
case Stage::ProgramStatus:setLow(low()|0xc0);next();break;
case Stage::BindNote:setLow(note_);next();break;
case Stage::BindVelocity:setLow(velocity_);next();break;
case Stage::BindPedalValue:setLow(high());next();break;
case Stage::BindExpressionValue:setLow(high());next();break;
case Stage::BindModulationValue:setLow(high());next();break;
case Stage::BindPressureValue:setLow(high());next();break;
case Stage::BindQueueVelocity:setLow(high());next();break;
case Stage::BindTransmitVelocity:setLow(high());next();break;
case Stage::BindControlValue:setLow(high());next();break;
case Stage::BindBendHigh:setLow(high());next();break;
case Stage::BindPedalController:setLow(64);next();break;
case Stage::BindExpressionController:setLow(11);next();break;
case Stage::BindModulationController:setLow(1);next();break;
case Stage::BindPanicController:setLow(123);next();break;
case Stage::BindPanicValue:setLow(0);next();break;
case Stage::ClearBendLow:setLow(0);next();break;
case Stage::SetBendLow:setLow(64);next();break;
case Stage::SetBendEnd:setLow(127);next();break;
case Stage::HalfBend:bendOdd_=bool(high()&1);event_=uint16_t((event_&255)|(unsigned(high()>>1)<<8));next();break;
case Stage::SelectBendLow:stage_=bendOdd_?Stage::SetBendLow:Stage::SaveBendHigh;break;
case Stage::SelectBendEnd:stage_=high()==127?Stage::SetBendEnd:Stage::SaveBendHigh;break;
case Stage::SaveBendHigh:bendHigh_=event_;next();break;
case Stage::RestoreBendHigh:event_=bendHigh_;next();break;
case Stage::ReadByteMode:mode_=uint8_t(read(Read::SendMode));next();break;
case Stage::ReadOffMode:mode_=uint8_t(read(Read::SendMode));next();break;
case Stage::ReadOnMode:mode_=uint8_t(read(Read::SendMode));next();break;
case Stage::SelectByteQueue:stage_=mode_<=63?Stage::RestoreByteContext:Stage::QueueByte;break;
case Stage::SelectOffQueue:stage_=mode_<=63?Stage::CheckOffLocal:Stage::SaveOffEvent;break;
case Stage::SelectOnQueue:stage_=mode_<=63?Stage::CheckOnLocal:Stage::SaveOnEvent;break;
case Stage::SelectOffLocal:stage_=local_?Stage::SendOffEvent:Stage::RestoreOffContext;break;
case Stage::SelectOnLocal:stage_=local_?Stage::SendOnEvent:Stage::RestoreOnContext;break;
case Stage::WriteLocalPart:write(Write::SelectedPart,high());next();break;
case Stage::ReadLocalFlags:flags_=uint8_t(read(Read::LocalFlags));next();break;
case Stage::SelectLocalEnable:stage_=(flags_&4)?Stage::MaskLocalFlags:Stage::ReturnLocal;break;
case Stage::MaskLocalFlags:flags_&=0x60;next();break;
case Stage::FinishLocalFlags:stage_=Stage::ReturnLocal;break;
case Stage::ReadPanicChannel:setLow(uint8_t(read(Read::PanicChannel)));next();break;
case Stage::SelectControlChannel:stage_=control_>=126?Stage::SaveControlContext:Stage::SelectRawControlChannel;break;
case Stage::ReadControlChannel:setLow(uint8_t(read(Read::ControlChannel)));next();break; default:next();break;}return cost;}
private:
 void setLow(uint8_t value){event_=uint16_t((event_&0xff00)|value);}void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint16_t event_,saved_=0,bendHigh_=0;uint8_t note_,velocity_,control_,mode_=0,flags_=0;uint32_t address_=0;bool local_=false,bendOdd_=false;
};
}
