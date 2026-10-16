#pragma once
#include <cstdint>
namespace ks {
class MidiControlTask final {
public:
 enum class Kind {Pitch,Pressure,Control};
 enum class Stage {ReadPitchLow,ReadPitchHigh,DoublePitchLow,DoublePitchWord,CopyPitchHigh,CapturePitch,ClearPitchHigh,DispatchPitch,AwaitPitch,FinishPitch,
 ReadPressure,CapturePressure,DispatchPressure,AwaitPressure,FinishPressure,
 ReadController,ReadValue,CompareExpression,AdmitExpression,RemapExpression,CompareWheel,AdmitWheel,CaptureWheel,TailWheel,
 CompareBreath,AdmitBreath,CaptureBreath,TailBreath,CompareBank,AdmitBank,CaptureBank,DispatchPart,AwaitPart,PublishBank,FinishBank,
 CompareSound,AdmitSound,ObserveSoundValue,AdmitSoundValue,DispatchSound,AwaitSound,FinishSound,
 CompareReset,AdmitReset,FinishReset,CompareNotes,AdmitNotes,CompareOmniOff,AdmitOmniOff,CompareOmniOn,AdmitOmniOn,CompareMono,AdmitMono,ComparePoly,AdmitPoly,
 ObserveNotesValue,AdmitNotesValue,DispatchRelease,AwaitRelease,DispatchHeldReset,AwaitHeldReset,FinishNotes,HandoffPedal,HandoffWheel,HandoffBreath,Complete};
 enum class Read {EventFirst,EventSecond,ExpressionController,Value};
 explicit MidiControlTask(Kind k):kind_(k),stage_(k==Kind::Pitch?Stage::ReadPitchLow:k==Kind::Pressure?Stage::ReadPressure:Stage::ReadController){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}uint8_t first()const{return first_;}uint8_t second()const{return second_;}
 uint8_t controller()const{return controller_;}uint8_t value()const{return value_;}uint8_t argument()const{return argument_;}
 uint8_t pitchLow()const{return pitchLow_;}uint16_t pitchWord()const{return pitchWord_;}uint8_t part()const{return part_;}
 bool awaitingPart()const{return stage_==Stage::AwaitPart;}bool resumePart(uint8_t p){if(!awaitingPart())return false;part_=p;stage_=Stage::PublishBank;return true;}
 bool awaitingChild()const{return stage_==Stage::AwaitPitch||stage_==Stage::AwaitPressure||stage_==Stage::AwaitSound||stage_==Stage::AwaitRelease||stage_==Stage::AwaitHeldReset;}
 bool resumeChild(){switch(stage_){case Stage::AwaitPitch:stage_=Stage::FinishPitch;break;case Stage::AwaitPressure:stage_=Stage::FinishPressure;break;case Stage::AwaitSound:stage_=Stage::FinishSound;break;case Stage::AwaitRelease:stage_=Stage::DispatchHeldReset;break;case Stage::AwaitHeldReset:stage_=Stage::FinishNotes;break;default:return false;}return true;}
 bool publication()const{return stage_==Stage::PublishBank;}bool handoff()const{return stage_==Stage::HandoffPedal||stage_==Stage::HandoffWheel||stage_==Stage::HandoffBreath;}bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){
 case Stage::ReadPitchLow:case Stage::ReadPressure:case Stage::ReadController:return 6;case Stage::ReadPitchHigh:case Stage::ReadValue:return 9;case Stage::RemapExpression:return 15;
 case Stage::DispatchPitch:case Stage::DispatchPressure:case Stage::DispatchPart:case Stage::DispatchSound:case Stage::DispatchRelease:case Stage::DispatchHeldReset:return 20;
 case Stage::TailWheel:case Stage::TailBreath:return 8;case Stage::PublishBank:return 12+store;
 case Stage::FinishPitch:case Stage::FinishPressure:case Stage::FinishBank:case Stage::FinishSound:case Stage::FinishReset:case Stage::FinishNotes:return 17;
 case Stage::AwaitPitch:case Stage::AwaitPressure:case Stage::AwaitPart:case Stage::AwaitSound:case Stage::AwaitRelease:case Stage::AwaitHeldReset:case Stage::HandoffPedal:case Stage::HandoffWheel:case Stage::HandoffBreath:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;while(const auto cost=nextClocks(publication()?timing(331):3)){
  if(cost>available-used)break;
  switch(stage_){
  case Stage::ReadPitchLow:first_=pitchLow_=uint8_t(read(Read::EventFirst));stage_=Stage::ReadPitchHigh;break;case Stage::ReadPitchHigh:second_=uint8_t(read(Read::EventSecond));stage_=Stage::DoublePitchLow;break;
  case Stage::DoublePitchLow:pitchLow_=uint8_t(pitchLow_*2);stage_=Stage::DoublePitchWord;break;case Stage::DoublePitchWord:pitchWord_=uint16_t(((unsigned(second_)<<8)|pitchLow_)*2);stage_=Stage::CopyPitchHigh;break;
  case Stage::CopyPitchHigh:value_=uint8_t(pitchWord_>>8);stage_=Stage::CapturePitch;break;case Stage::CapturePitch:argument_=value_;stage_=Stage::ClearPitchHigh;break;case Stage::ClearPitchHigh:stage_=Stage::DispatchPitch;break;case Stage::DispatchPitch:stage_=Stage::AwaitPitch;break;
  case Stage::ReadPressure:first_=value_=uint8_t(read(Read::EventFirst));stage_=Stage::CapturePressure;break;case Stage::CapturePressure:argument_=value_;stage_=Stage::DispatchPressure;break;case Stage::DispatchPressure:stage_=Stage::AwaitPressure;break;
  case Stage::ReadController:first_=controller_=uint8_t(read(Read::EventFirst));stage_=Stage::ReadValue;break;case Stage::ReadValue:second_=value_=uint8_t(read(Read::EventSecond));stage_=Stage::CompareExpression;break;
  case Stage::CompareExpression:stage_=Stage::AdmitExpression;break;case Stage::AdmitExpression:stage_=controller_==11?Stage::RemapExpression:Stage::CompareWheel;break;
  case Stage::RemapExpression:controller_=uint8_t(read(Read::ExpressionController));stage_=Stage::CompareWheel;break;case Stage::CompareWheel:stage_=Stage::AdmitWheel;break;
  case Stage::AdmitWheel:stage_=controller_==1?Stage::CaptureWheel:Stage::CompareBreath;break;case Stage::CaptureWheel:argument_=uint8_t(read(Read::Value));stage_=Stage::TailWheel;break;case Stage::TailWheel:stage_=Stage::HandoffWheel;break;
  case Stage::CompareBreath:stage_=Stage::AdmitBreath;break;case Stage::AdmitBreath:stage_=controller_==2?Stage::CaptureBreath:Stage::CompareBank;break;case Stage::CaptureBreath:argument_=uint8_t(read(Read::Value));stage_=Stage::TailBreath;break;case Stage::TailBreath:stage_=Stage::HandoffBreath;break;
  case Stage::CompareBank:stage_=Stage::AdmitBank;break;case Stage::AdmitBank:stage_=controller_==32?Stage::CaptureBank:Stage::CompareSound;break;case Stage::CaptureBank:argument_=uint8_t(read(Read::Value));stage_=Stage::DispatchPart;break;case Stage::DispatchPart:stage_=Stage::AwaitPart;break;
  case Stage::PublishBank:write(331,argument_);stage_=Stage::FinishBank;break;case Stage::CompareSound:stage_=Stage::AdmitSound;break;case Stage::AdmitSound:stage_=controller_==120?Stage::ObserveSoundValue:Stage::CompareReset;break;
  case Stage::ObserveSoundValue:value_=uint8_t(read(Read::Value));stage_=Stage::AdmitSoundValue;break;case Stage::AdmitSoundValue:stage_=value_?Stage::FinishSound:Stage::DispatchSound;break;case Stage::DispatchSound:stage_=Stage::AwaitSound;break;
  case Stage::CompareReset:stage_=Stage::AdmitReset;break;case Stage::AdmitReset:stage_=controller_==121?Stage::FinishReset:Stage::CompareNotes;break;
  case Stage::CompareNotes:stage_=Stage::AdmitNotes;break;case Stage::AdmitNotes:stage_=controller_==123?Stage::ObserveNotesValue:Stage::CompareOmniOff;break;
  case Stage::CompareOmniOff:stage_=Stage::AdmitOmniOff;break;case Stage::AdmitOmniOff:stage_=controller_==124?Stage::ObserveNotesValue:Stage::CompareOmniOn;break;
  case Stage::CompareOmniOn:stage_=Stage::AdmitOmniOn;break;case Stage::AdmitOmniOn:stage_=controller_==125?Stage::ObserveNotesValue:Stage::CompareMono;break;
  case Stage::CompareMono:stage_=Stage::AdmitMono;break;case Stage::AdmitMono:stage_=controller_==126?Stage::DispatchRelease:Stage::ComparePoly;break;
  case Stage::ComparePoly:stage_=Stage::AdmitPoly;break;case Stage::AdmitPoly:stage_=controller_==127?Stage::ObserveNotesValue:Stage::HandoffPedal;break;
  case Stage::ObserveNotesValue:value_=uint8_t(read(Read::Value));stage_=Stage::AdmitNotesValue;break;case Stage::AdmitNotesValue:stage_=value_?Stage::FinishNotes:Stage::DispatchRelease;break;
  case Stage::DispatchRelease:stage_=Stage::AwaitRelease;break;case Stage::DispatchHeldReset:stage_=Stage::AwaitHeldReset;break;
  case Stage::FinishPitch:case Stage::FinishPressure:case Stage::FinishBank:case Stage::FinishSound:case Stage::FinishReset:case Stage::FinishNotes:stage_=Stage::Complete;break;default:break;
  }used+=cost;
 }return used;}
private:Kind kind_;Stage stage_;uint8_t first_=0,second_=0,controller_=0,value_=0,argument_=0,pitchLow_=0,part_=0;uint16_t pitchWord_=0;
};
}
