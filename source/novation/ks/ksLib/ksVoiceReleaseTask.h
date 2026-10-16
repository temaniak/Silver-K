#pragma once
#include <cstdint>
namespace ks {
// Logical voice-release walker. Voice count zero means 256 iterations, and
// the next slot is observed from the live selector rather than a private loop
// index. Pedal-held and bypass-policy decisions retain their original order.
class VoiceReleaseTask final {
public:
 enum class Mode {Note,Part,FastPart,Pedal,ProvidedPart};
 enum class Stage {NoteInput,DefaultCut,FastCut,FastSkip,ReadPart,ZeroIndex,PublishInitial,ReadCount,
  RetainRemaining,RetainMatch,RetainPolicy,DispatchVoice,AwaitVoice,ReadAge,AdmitAge,
  ReadOwner,CompareOwner,AdmitOwner,ReadNote,CompareNote,AdmitNote,ReadExtra,AdmitExtra,
  ReadPatchEarly,ReadFlags,TestBypass,AdmitBypass,DispatchParameters,AwaitParameters,
  ReadPedal,AdmitPedal,WritePending,SkipHeld,ReadPending,AdmitPending,ClearPending,WriteClearPending,
  MaskAge,MarkAge,CaptureAge,SetLowAge,WriteAge,ReadPatchLate,DispatchSelect,AwaitSelect,
  InspectPolicy,RetainInspectedPolicy,AdmitPolicy,SetCutWord,CutOffset,DispatchCut,AwaitCut,
  DispatchGate,AwaitGate,ReadIndex,TickIndex,PublishIndex,RestorePolicy,RestoreMatch,RestoreRemaining,
  TickRemaining,AdmitRemaining,DispatchClear,AwaitClear,Complete};
 enum class Read {Part,Count,Age,Owner,Note,Extra,PatchReference,Flags,Pedal,Pending,Index};
 enum class Write {Index,Pending,Age};
 enum class Child {None,Voice,Parameters,Select,Cut,Gate,Clear};
 explicit VoiceReleaseTask(Mode mode,uint8_t note=0,uint16_t cut=0):mode_(mode),stage_(mode==Mode::Note?Stage::NoteInput:mode==Mode::Part?Stage::DefaultCut:mode==Mode::FastPart?Stage::FastCut:Stage::ReadPart),note_(note),cut_(cut){}
 Mode mode()const{return mode_;}bool wholePart()const{return mode_==Mode::Part||mode_==Mode::FastPart||mode_==Mode::ProvidedPart;}
 Stage stage()const{return stage_;}uint8_t part()const{return part_;}uint8_t note()const{return note_;}uint8_t slot()const{return slot_;}
 uint8_t parameterPart()const{return parameterPart_;}uint8_t owner()const{return owner_;}uint8_t slotNote()const{return slotNote_;}
 uint8_t extra()const{return extra_;}uint8_t flags()const{return flags_;}uint8_t pedal()const{return pedal_;}uint8_t pending()const{return pending_;}
 uint8_t index()const{return index_;}uint16_t remaining()const{return remaining_;}uint16_t age()const{return age_;}uint16_t cut()const{return cut_;}
 uint32_t releaseAge()const{return (uint32_t(age_)<<16)|0xffffu;}
 bool complete()const{return stage_==Stage::Complete;}
 Child child()const{switch(stage_){case Stage::AwaitVoice:return Child::Voice;case Stage::AwaitParameters:return Child::Parameters;
 case Stage::AwaitSelect:return Child::Select;case Stage::AwaitCut:return Child::Cut;case Stage::AwaitGate:return Child::Gate;case Stage::AwaitClear:return Child::Clear;default:return Child::None;}}
 bool resumeChild(uint8_t result=0){switch(stage_){case Stage::AwaitVoice:slot_=result;stage_=Stage::ReadAge;return true;
 case Stage::AwaitParameters:parameterPart_=result;stage_=Stage::ReadPedal;return true;
 case Stage::AwaitSelect:stage_=wholePart()?Stage::InspectPolicy:Stage::DispatchGate;return true;
 case Stage::AwaitCut:stage_=Stage::DispatchGate;return true;case Stage::AwaitGate:stage_=Stage::ReadIndex;return true;
 case Stage::AwaitClear:stage_=Stage::Complete;return true;default:return false;}}
 bool publication()const{return stage_==Stage::PublishInitial||stage_==Stage::PublishIndex||stage_==Stage::WritePending||stage_==Stage::WriteClearPending;}
 Write publicationKind()const{return stage_==Stage::PublishInitial||stage_==Stage::PublishIndex?Write::Index:Write::Pending;}
 unsigned nextClocks(unsigned publicationClocks=3)const{switch(stage_){
 case Stage::FastCut:case Stage::SetLowAge:case Stage::SetCutWord:case Stage::CutOffset:return 6;
 case Stage::ReadPart:case Stage::ReadCount:case Stage::ReadIndex:return 12;
 case Stage::PublishInitial:case Stage::PublishIndex:return 9+publicationClocks;
 case Stage::WritePending:case Stage::WriteClearPending:return 12+publicationClocks;
 case Stage::RetainRemaining:case Stage::RetainMatch:case Stage::RetainPolicy:case Stage::InspectPolicy:case Stage::RetainInspectedPolicy:
 case Stage::RestorePolicy:case Stage::RestoreMatch:case Stage::RestoreRemaining:return 11;
 case Stage::DispatchVoice:case Stage::DispatchParameters:case Stage::DispatchSelect:case Stage::DispatchCut:case Stage::DispatchGate:case Stage::DispatchClear:return 20;
 case Stage::ReadAge:case Stage::ReadOwner:case Stage::ReadNote:case Stage::ReadExtra:case Stage::ReadFlags:case Stage::ReadPedal:case Stage::ReadPending:return 15;
 case Stage::ReadPatchEarly:case Stage::ReadPatchLate:case Stage::WriteAge:return 21;
 case Stage::AdmitRemaining:return mode_==Mode::Note?8:3;
 case Stage::AwaitVoice:case Stage::AwaitParameters:case Stage::AwaitSelect:case Stage::AwaitCut:case Stage::AwaitGate:case Stage::AwaitClear:case Stage::Complete:return 0;
 default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){
 unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationKind()):3u)){
  if(cost>available-used)break;
  switch(stage_){case Stage::NoteInput:stage_=Stage::ReadPart;break;case Stage::DefaultCut:cut_=0;stage_=Stage::ReadPart;break;
  case Stage::FastCut:cut_=1;stage_=Stage::FastSkip;break;case Stage::FastSkip:stage_=Stage::ReadPart;break;
  case Stage::ReadPart:part_=uint8_t(read(Read::Part));stage_=Stage::ZeroIndex;break;
  case Stage::ZeroIndex:index_=0;stage_=Stage::PublishInitial;break;case Stage::PublishInitial:write(Write::Index,index_);stage_=Stage::ReadCount;break;
  case Stage::ReadCount:remaining_=uint8_t(read(Read::Count));if(!remaining_)remaining_=256;stage_=Stage::RetainRemaining;break;
  case Stage::RetainRemaining:stage_=Stage::RetainMatch;break;case Stage::RetainMatch:stage_=wholePart()?Stage::RetainPolicy:Stage::DispatchVoice;break;
  case Stage::RetainPolicy:stage_=Stage::DispatchVoice;break;case Stage::DispatchVoice:stage_=Stage::AwaitVoice;break;
  case Stage::ReadAge:age_=uint16_t(read(Read::Age));stage_=Stage::AdmitAge;break;case Stage::AdmitAge:stage_=age_?Stage::ReadOwner:Stage::ReadIndex;break;
  case Stage::ReadOwner:owner_=uint8_t(read(Read::Owner));stage_=Stage::CompareOwner;break;case Stage::CompareOwner:stage_=Stage::AdmitOwner;break;
  case Stage::AdmitOwner:stage_=owner_!=part_?Stage::ReadIndex:wholePart()?Stage::MaskAge:mode_==Mode::Note?Stage::ReadNote:Stage::ReadPending;break;
  case Stage::ReadNote:slotNote_=uint8_t(read(Read::Note));stage_=Stage::CompareNote;break;case Stage::CompareNote:stage_=Stage::AdmitNote;break;
  case Stage::AdmitNote:stage_=slotNote_==note_?Stage::ReadExtra:Stage::ReadIndex;break;
  case Stage::ReadExtra:extra_=uint8_t(read(Read::Extra));stage_=Stage::AdmitExtra;break;case Stage::AdmitExtra:stage_=extra_?Stage::ReadIndex:Stage::ReadPatchEarly;break;
  case Stage::ReadPatchEarly:read(Read::PatchReference);stage_=Stage::ReadFlags;break;
  case Stage::ReadFlags:flags_=uint8_t(read(Read::Flags));stage_=Stage::TestBypass;break;case Stage::TestBypass:stage_=Stage::AdmitBypass;break;
  case Stage::AdmitBypass:stage_=(flags_&4)?Stage::MaskAge:Stage::DispatchParameters;break;
  case Stage::DispatchParameters:stage_=Stage::AwaitParameters;break;case Stage::ReadPedal:pedal_=uint8_t(read(Read::Pedal));stage_=Stage::AdmitPedal;break;
  case Stage::AdmitPedal:stage_=pedal_?Stage::WritePending:Stage::MaskAge;break;
  case Stage::WritePending:write(Write::Pending,pedal_);stage_=Stage::SkipHeld;break;case Stage::SkipHeld:stage_=Stage::ReadIndex;break;
  case Stage::ReadPending:pending_=uint8_t(read(Read::Pending));stage_=Stage::AdmitPending;break;case Stage::AdmitPending:stage_=pending_?Stage::ClearPending:Stage::ReadIndex;break;
  case Stage::ClearPending:stage_=Stage::WriteClearPending;break;case Stage::WriteClearPending:write(Write::Pending,0);stage_=Stage::MaskAge;break;
  case Stage::MaskAge:age_&=0x3fff;stage_=Stage::MarkAge;break;case Stage::MarkAge:age_|=1;stage_=Stage::CaptureAge;break;
  case Stage::CaptureAge:stage_=Stage::SetLowAge;break;case Stage::SetLowAge:stage_=Stage::WriteAge;break;
  case Stage::WriteAge:write(Write::Age,releaseAge());stage_=mode_==Mode::Note?Stage::DispatchSelect:Stage::ReadPatchLate;break;
  case Stage::ReadPatchLate:read(Read::PatchReference);stage_=Stage::DispatchSelect;break;case Stage::DispatchSelect:stage_=Stage::AwaitSelect;break;
  case Stage::InspectPolicy:stage_=Stage::RetainInspectedPolicy;break;case Stage::RetainInspectedPolicy:stage_=Stage::AdmitPolicy;break;
  case Stage::AdmitPolicy:stage_=cut_?Stage::SetCutWord:Stage::DispatchGate;break;case Stage::SetCutWord:stage_=Stage::CutOffset;break;
  case Stage::CutOffset:stage_=Stage::DispatchCut;break;case Stage::DispatchCut:stage_=Stage::AwaitCut;break;case Stage::DispatchGate:stage_=Stage::AwaitGate;break;
  case Stage::ReadIndex:index_=uint8_t(read(Read::Index));stage_=Stage::TickIndex;break;case Stage::TickIndex:++index_;stage_=Stage::PublishIndex;break;
  case Stage::PublishIndex:write(Write::Index,index_);stage_=wholePart()?Stage::RestorePolicy:Stage::RestoreMatch;break;
  case Stage::RestorePolicy:stage_=Stage::RestoreMatch;break;case Stage::RestoreMatch:stage_=Stage::RestoreRemaining;break;
  case Stage::RestoreRemaining:stage_=Stage::TickRemaining;break;case Stage::TickRemaining:--remaining_;stage_=Stage::AdmitRemaining;break;
  case Stage::AdmitRemaining:stage_=remaining_?Stage::RetainRemaining:wholePart()?Stage::DispatchClear:Stage::Complete;break;
  case Stage::DispatchClear:stage_=Stage::AwaitClear;break;default:break;}
  used+=cost;
 }return used;}
private:Mode mode_;Stage stage_;uint8_t note_=0,part_=0,slot_=0,parameterPart_=0,owner_=0,slotNote_=0,extra_=0,flags_=0,pedal_=0,pending_=0,index_=0;
 uint16_t cut_=0,remaining_=0,age_=0;
};
}
