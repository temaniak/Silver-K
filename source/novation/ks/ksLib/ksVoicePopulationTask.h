#pragma once
#include <cstdint>
namespace ks {
// Native initialization of one selected slot. PatchReference is a binding
// operation: its writer links the current patch, without a processor pointer
// in the task. Mutable fields are individually observed at admitted times.
class VoicePopulationTask final {
public:
 enum class Stage {DispatchVoice,AwaitVoice,DispatchPart,AwaitPart,WritePatch,
  ReadNote,WriteNote,ReadTranspose,ShiftTranspose,TransposeNote,WriteTransposed,
  ClearHeld,WriteHeld,ReadUnison,WriteUnison,ReadVelocity,ReadCurve,VelocityDefaults,
  ReadPower,PowerBias,DispatchVelocity,AwaitVelocity,WriteVelocity,InitialAge,WriteAge,
  ReadPart,WritePart,DispatchMirror,AwaitMirror,ReadExtra,WriteExtra,DispatchSelect,AwaitSelect,
  PitchOffset,DispatchPitch,AwaitPitch,PitchBias,ReadGlideNote,Difference,AdmitDifference,
  Absolute,WriteDistance,ClearPending,WritePending,Complete};
 enum class Read {Note,Transpose,Unison,Velocity,Curve,Power,Part,Extra,GlideNote};
 enum class Write {PatchReference,Note,Transposed,Held,Unison,Velocity,Age,Part,Extra,Distance,Pending};
 enum class Child {None,Voice,Part,Velocity,Mirror,Select,Pitch};
 struct Advance {unsigned clocks=0,boundaries=0;};
 Stage stage()const{return stage_;}uint8_t slot()const{return slot_;}uint8_t controlPart()const{return controlPart_;}
 uint8_t note()const{return note_;}uint8_t transpose()const{return transpose_;}uint8_t transposed()const{return transposed_;}
 uint8_t unison()const{return unison_;}uint8_t velocity()const{return velocity_;}uint8_t curve()const{return curve_;}uint8_t power()const{return power_;}
 uint8_t part()const{return part_;}uint8_t extra()const{return extra_;}uint8_t pitch()const{return pitch_;}
 uint8_t glideNote()const{return glideNote_;}uint8_t distance()const{return distance_;}
 static constexpr uint32_t initialAge(){return 0x8000ffff;}static constexpr uint16_t pitchOffset(){return 0x808c;}
 bool complete()const{return stage_==Stage::Complete;}
 Child child()const{switch(stage_){case Stage::AwaitVoice:return Child::Voice;case Stage::AwaitPart:return Child::Part;
  case Stage::AwaitVelocity:return Child::Velocity;case Stage::AwaitMirror:return Child::Mirror;
  case Stage::AwaitSelect:return Child::Select;case Stage::AwaitPitch:return Child::Pitch;default:return Child::None;}}
 bool resumeChild(uint8_t result=0){switch(stage_){case Stage::AwaitVoice:slot_=result;stage_=Stage::DispatchPart;return true;
  case Stage::AwaitPart:controlPart_=result;stage_=Stage::WritePatch;return true;
  case Stage::AwaitVelocity:velocity_=result;stage_=Stage::WriteVelocity;return true;
  case Stage::AwaitMirror:stage_=Stage::ReadExtra;return true;case Stage::AwaitSelect:stage_=Stage::PitchOffset;return true;
  case Stage::AwaitPitch:pitch_=result;stage_=Stage::PitchBias;return true;default:return false;}}
 unsigned nextClocks(unsigned publicationClocks=3)const{
  switch(stage_){case Stage::DispatchVoice:case Stage::DispatchPart:case Stage::DispatchMirror:case Stage::DispatchSelect:case Stage::DispatchPitch:return 20;
  case Stage::DispatchVelocity:return 15;case Stage::WritePatch:case Stage::WriteAge:return 21;
  case Stage::ReadNote:case Stage::ReadVelocity:case Stage::ReadPart:return 12;
  case Stage::ReadTranspose:case Stage::ReadUnison:case Stage::ReadCurve:case Stage::ReadPower:case Stage::ReadExtra:case Stage::ReadGlideNote:return 15;
  case Stage::WriteNote:case Stage::WriteTransposed:case Stage::WriteHeld:case Stage::WriteUnison:case Stage::WriteVelocity:
  case Stage::WritePart:case Stage::WriteExtra:case Stage::WriteDistance:case Stage::WritePending:return 12+publicationClocks;
  case Stage::VelocityDefaults:case Stage::InitialAge:return 9;case Stage::PitchOffset:return 6;
  case Stage::AwaitVoice:case Stage::AwaitPart:case Stage::AwaitVelocity:case Stage::AwaitMirror:case Stage::AwaitSelect:case Stage::AwaitPitch:case Stage::Complete:return 0;
  default:return 3;}
 }
 template<class Reader,class Writer,class Timing>Advance advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){
  Advance p;while(const auto cost=nextClocks(bytePublication()?timing(publication()):3u)){
   if(cost>available)break;
   switch(stage_){case Stage::DispatchVoice:stage_=Stage::AwaitVoice;break;case Stage::DispatchPart:stage_=Stage::AwaitPart;break;
   case Stage::WritePatch:write(Write::PatchReference,0);stage_=Stage::ReadNote;break;
   case Stage::ReadNote:note_=uint8_t(read(Read::Note));stage_=Stage::WriteNote;break;
   case Stage::WriteNote:write(Write::Note,note_);stage_=Stage::ReadTranspose;break;
   case Stage::ReadTranspose:transpose_=uint8_t(read(Read::Transpose));stage_=Stage::ShiftTranspose;break;
   case Stage::ShiftTranspose:transpose_=uint8_t(transpose_-64);stage_=Stage::TransposeNote;break;
   case Stage::TransposeNote:transposed_=uint8_t(note_+transpose_);stage_=Stage::WriteTransposed;break;
   case Stage::WriteTransposed:write(Write::Transposed,transposed_);stage_=Stage::ClearHeld;break;
   case Stage::ClearHeld:stage_=Stage::WriteHeld;break;case Stage::WriteHeld:write(Write::Held,0);stage_=Stage::ReadUnison;break;
   case Stage::ReadUnison:unison_=uint8_t(read(Read::Unison));stage_=Stage::WriteUnison;break;
   case Stage::WriteUnison:write(Write::Unison,unison_);stage_=Stage::ReadVelocity;break;
   case Stage::ReadVelocity:velocity_=uint8_t(read(Read::Velocity));stage_=Stage::ReadCurve;break;
   case Stage::ReadCurve:curve_=uint8_t(read(Read::Curve));stage_=Stage::VelocityDefaults;break;
   case Stage::VelocityDefaults:stage_=Stage::ReadPower;break;case Stage::ReadPower:power_=uint8_t(read(Read::Power));stage_=Stage::PowerBias;break;
   case Stage::PowerBias:power_=uint8_t(power_+2);stage_=Stage::DispatchVelocity;break;case Stage::DispatchVelocity:stage_=Stage::AwaitVelocity;break;
   case Stage::WriteVelocity:write(Write::Velocity,velocity_);stage_=Stage::InitialAge;break;
   case Stage::InitialAge:stage_=Stage::WriteAge;break;case Stage::WriteAge:write(Write::Age,initialAge());stage_=Stage::ReadPart;break;
   case Stage::ReadPart:part_=uint8_t(read(Read::Part));stage_=Stage::WritePart;break;case Stage::WritePart:write(Write::Part,part_);stage_=Stage::DispatchMirror;break;
   case Stage::DispatchMirror:stage_=Stage::AwaitMirror;break;case Stage::ReadExtra:extra_=uint8_t(read(Read::Extra));stage_=Stage::WriteExtra;break;
   case Stage::WriteExtra:write(Write::Extra,extra_);stage_=Stage::DispatchSelect;break;case Stage::DispatchSelect:stage_=Stage::AwaitSelect;break;
   case Stage::PitchOffset:stage_=Stage::DispatchPitch;break;case Stage::DispatchPitch:stage_=Stage::AwaitPitch;break;
   case Stage::PitchBias:pitch_=uint8_t(pitch_+60);stage_=Stage::ReadGlideNote;break;
   case Stage::ReadGlideNote:glideNote_=uint8_t(read(Read::GlideNote));stage_=Stage::Difference;break;
   case Stage::Difference:distance_=uint8_t(glideNote_-pitch_);stage_=Stage::AdmitDifference;break;
   case Stage::AdmitDifference:stage_=glideNote_<pitch_?Stage::Absolute:Stage::WriteDistance;break;
   case Stage::Absolute:distance_=uint8_t(0-distance_);stage_=Stage::WriteDistance;break;
   case Stage::WriteDistance:write(Write::Distance,distance_);stage_=Stage::ClearPending;break;
   case Stage::ClearPending:stage_=Stage::WritePending;break;case Stage::WritePending:write(Write::Pending,0);stage_=Stage::Complete;break;default:break;}
   available-=cost;p.clocks+=cost;++p.boundaries;
  }return p;
 }
 bool bytePublication()const{switch(stage_){case Stage::WriteNote:case Stage::WriteTransposed:case Stage::WriteHeld:case Stage::WriteUnison:case Stage::WriteVelocity:
  case Stage::WritePart:case Stage::WriteExtra:case Stage::WriteDistance:case Stage::WritePending:return true;default:return false;}}
 Write publication()const{switch(stage_){case Stage::WriteNote:return Write::Note;case Stage::WriteTransposed:return Write::Transposed;case Stage::WriteHeld:return Write::Held;
  case Stage::WriteUnison:return Write::Unison;case Stage::WriteVelocity:return Write::Velocity;case Stage::WritePart:return Write::Part;case Stage::WriteExtra:return Write::Extra;
  case Stage::WriteDistance:return Write::Distance;default:return Write::Pending;}}
private:
 Stage stage_=Stage::DispatchVoice;
 uint8_t slot_=0,controlPart_=0,note_=0,transpose_=0,transposed_=0,unison_=0,velocity_=0,curve_=0,power_=0,part_=0,extra_=0,pitch_=0,glideNote_=0,distance_=0;
};
}
