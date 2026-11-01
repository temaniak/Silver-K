#pragma once
#include <cstdint>
namespace ks {
// Native slot-selection policy. Original live observations and time intervals
// remain explicit; CPU register/condition/stack state does not drive decisions.
class VoiceSelectionTask final {
public:
 enum class Search {Owner,NewestOwner,Free,SameNote};
 enum class Stage {DispatchPart,AwaitPart,ReadMode,ShiftModeTwo,ShiftModeOne,MaskMode,
  ReadHeld,ReadUnison,ReadPart,CaptureOwner,ResetCandidate,PublishInitialIndex,ReadCount,InitializeAge,CompareMode,AdmitMode,
  TestHeld,AdmitHeld,SetHeldAllocation,JumpHeldAllocation,SetReuseAllocation,PublishReuseAllocation,SetOwnerAllocation,
  CompareOtherMode,AdmitOtherMode,SetPolyAllocation,PublishFreeAllocation,ReadIncoming,SetNoteAllocation,PublishNoteAllocation,
  DispatchVoice,AwaitVoice,ReadAge,ReadOwner,ReadSlotUnison,CompareOwner,AdmitOwner,ResetNewestAge,
  TestAgeHigh,AdmitAgeHigh,CaptureHighAge,MaskActiveAge,AdmitActiveAge,ReadNote,CompareNote,AdmitNote,SetRetrigger,PublishRetrigger,JumpRetrigger,
  CompareAge,AdmitAge,CopyAge,ReadCandidate,ReadIndex,IndexTick,PublishIndex,RemainingTick,RemainingGate,PublishCandidate,
  PublishOwnerAllocation,TestBestHigh,AdmitBestHigh,JumpPopulation,SetNewestThree,CompareHighAge,AdmitHighAge,SetNewestTwo,PublishNewestAllocation,JumpRelease,
  DispatchSelect,AwaitSelect,ReleaseWord,ReleaseAddressOne,DispatchWriteOne,AwaitWriteOne,ReleaseAddressTwo,DispatchWriteTwo,AwaitWriteTwo,DispatchGate,AwaitGate,Complete};
 enum class Read {PatchMode,Held,Unison,Part,Count,Age,OwnerPart,OwnerUnison,SlotNote,VoiceIndex,IncomingNote};
 enum class Write {VoiceIndex,Allocation};
 enum class Child {None,Part,Voice,SelectVoice,WriteOne,WriteTwo,Gate};
 struct Advance {unsigned clocks=0,boundaries=0;};
 Stage stage()const{return stage_;}Search search()const{return search_;}
 uint8_t controlPart()const{return controlPart_;}uint8_t slot()const{return slot_;}
 uint8_t modeBits()const{return mode_;}uint8_t held()const{return held_;}uint8_t unison()const{return unison_;}uint8_t part()const{return part_;}
 uint16_t ownerTag()const{return uint16_t((unsigned(unison_)<<8)|part_);}
 uint16_t slotOwnerTag()const{return uint16_t((unsigned(slotUnison_)<<8)|slotPart_);}
 uint8_t candidate()const{return candidate_;}uint8_t index()const{return index_;}uint8_t remaining()const{return remaining_;}
 uint8_t allocation()const{return allocation_;}uint8_t incoming()const{return incoming_;}uint8_t note()const{return note_;}
 uint8_t slotPart()const{return slotPart_;}uint8_t slotUnison()const{return slotUnison_;}
 uint32_t age()const{return age_;}uint32_t bestAge()const{return bestAge_;}
 bool betterAge()const{return search_==Search::NewestOwner?age_>bestAge_:age_<bestAge_;}
 bool complete()const{return stage_==Stage::Complete;}
 static constexpr uint32_t releaseCommand(){return 0x200000;}
 static constexpr uint16_t firstReleaseOffset(){return 0x72;}static constexpr uint16_t secondReleaseOffset(){return 0x7a;}
 Child child()const{switch(stage_){case Stage::AwaitPart:return Child::Part;case Stage::AwaitVoice:return Child::Voice;
  case Stage::AwaitSelect:return Child::SelectVoice;case Stage::AwaitWriteOne:return Child::WriteOne;
  case Stage::AwaitWriteTwo:return Child::WriteTwo;case Stage::AwaitGate:return Child::Gate;default:return Child::None;}}
 bool resumeChild(uint8_t index=0){switch(stage_){
  case Stage::AwaitPart:controlPart_=index;stage_=Stage::ReadMode;return true;
  case Stage::AwaitVoice:slot_=index;stage_=Stage::ReadAge;return true;
  case Stage::AwaitSelect:stage_=Stage::ReleaseWord;return true;
  case Stage::AwaitWriteOne:stage_=Stage::ReleaseAddressTwo;return true;
  case Stage::AwaitWriteTwo:stage_=Stage::DispatchGate;return true;
  case Stage::AwaitGate:stage_=Stage::Complete;return true;default:return false;}}
 unsigned nextClocks(unsigned indexPublicationClocks=3)const{
  switch(stage_){
  case Stage::DispatchPart:case Stage::DispatchVoice:case Stage::DispatchSelect:case Stage::DispatchWriteOne:case Stage::DispatchWriteTwo:case Stage::DispatchGate:return 20;
  case Stage::ReadMode:case Stage::ReadHeld:case Stage::ReadUnison:case Stage::ReadOwner:case Stage::ReadSlotUnison:case Stage::ReadNote:return 15;
  case Stage::ReadPart:case Stage::ReadCount:case Stage::ReadIncoming:case Stage::ReadCandidate:case Stage::ReadIndex:
  case Stage::PublishReuseAllocation:case Stage::PublishFreeAllocation:case Stage::PublishNoteAllocation:case Stage::PublishRetrigger:
  case Stage::PublishOwnerAllocation:case Stage::PublishNewestAllocation:return 12;
  case Stage::PublishInitialIndex:case Stage::PublishIndex:case Stage::PublishCandidate:return 9+indexPublicationClocks;
  case Stage::InitializeAge:return 9;case Stage::ReadAge:return 21;
  case Stage::AdmitMode:case Stage::JumpHeldAllocation:case Stage::AdmitActiveAge:case Stage::JumpPopulation:case Stage::JumpRelease:return 8;
  case Stage::AdmitAgeHigh:return search_==Search::Free?8:3;
  case Stage::AdmitBestHigh:return search_==Search::Owner?8:3;
  case Stage::CompareHighAge:case Stage::ReleaseWord:case Stage::ReleaseAddressOne:case Stage::ReleaseAddressTwo:return 6;
  case Stage::AwaitPart:case Stage::AwaitVoice:case Stage::AwaitSelect:case Stage::AwaitWriteOne:case Stage::AwaitWriteTwo:case Stage::AwaitGate:case Stage::Complete:return 0;
  default:return 3;
  }
 }
 template<class Reader,class Writer,class Timing>Advance advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){
  Advance p;while(const auto cost=nextClocks(indexPublication()?timing(Write::VoiceIndex):3u)){
   if(cost>available)break;
   switch(stage_){
   case Stage::DispatchPart:stage_=Stage::AwaitPart;break;
   case Stage::ReadMode:mode_=uint8_t(read(Read::PatchMode));stage_=Stage::ShiftModeTwo;break;
   case Stage::ShiftModeTwo:mode_>>=2;stage_=Stage::ShiftModeOne;break;
   case Stage::ShiftModeOne:mode_>>=1;stage_=Stage::MaskMode;break;
   case Stage::MaskMode:mode_&=3;stage_=Stage::ReadHeld;break;
   case Stage::ReadHeld:held_=uint8_t(read(Read::Held));stage_=Stage::ReadUnison;break;
   case Stage::ReadUnison:unison_=uint8_t(read(Read::Unison));stage_=Stage::ReadPart;break;
   case Stage::ReadPart:part_=uint8_t(read(Read::Part));stage_=Stage::CaptureOwner;break;
   case Stage::CaptureOwner:stage_=Stage::ResetCandidate;break;
   case Stage::ResetCandidate:candidate_=0;stage_=Stage::PublishInitialIndex;break;
   case Stage::PublishInitialIndex:write(Write::VoiceIndex,0);stage_=Stage::ReadCount;break;
   case Stage::ReadCount:remaining_=uint8_t(read(Read::Count));stage_=Stage::InitializeAge;break;
   case Stage::InitializeAge:bestAge_=0xffffffff;stage_=Stage::CompareMode;break;
   case Stage::CompareMode:stage_=Stage::AdmitMode;break;
   case Stage::AdmitMode:stage_=mode_>1?Stage::CompareOtherMode:Stage::TestHeld;break;
   case Stage::TestHeld:stage_=Stage::AdmitHeld;break;
   case Stage::AdmitHeld:stage_=held_?Stage::SetHeldAllocation:Stage::SetReuseAllocation;break;
   case Stage::SetHeldAllocation:allocation_=1;stage_=Stage::JumpHeldAllocation;break;
   case Stage::JumpHeldAllocation:stage_=Stage::PublishFreeAllocation;break;
   case Stage::SetReuseAllocation:allocation_=4;stage_=Stage::PublishReuseAllocation;break;
   case Stage::PublishReuseAllocation:write(Write::Allocation,allocation_);stage_=Stage::SetOwnerAllocation;break;
   case Stage::SetOwnerAllocation:allocation_=1;search_=Search::Owner;stage_=Stage::DispatchVoice;break;
   case Stage::CompareOtherMode:stage_=Stage::AdmitOtherMode;break;
   case Stage::AdmitOtherMode:stage_=mode_==2?Stage::SetPolyAllocation:Stage::ReadIncoming;break;
   case Stage::SetPolyAllocation:allocation_=0;stage_=Stage::PublishFreeAllocation;break;
   case Stage::PublishFreeAllocation:write(Write::Allocation,allocation_);search_=Search::Free;stage_=Stage::DispatchVoice;break;
   case Stage::ReadIncoming:incoming_=uint8_t(read(Read::IncomingNote));stage_=Stage::SetNoteAllocation;break;
   case Stage::SetNoteAllocation:allocation_=0;stage_=Stage::PublishNoteAllocation;break;
   case Stage::PublishNoteAllocation:write(Write::Allocation,allocation_);search_=Search::SameNote;stage_=Stage::DispatchVoice;break;
   case Stage::DispatchVoice:stage_=Stage::AwaitVoice;break;
   case Stage::ReadAge:age_=read(Read::Age);stage_=(search_==Search::Free||search_==Search::SameNote)?Stage::TestAgeHigh:Stage::ReadOwner;break;
   case Stage::ReadOwner:slotPart_=uint8_t(read(Read::OwnerPart));stage_=Stage::ReadSlotUnison;break;
   case Stage::ReadSlotUnison:slotUnison_=uint8_t(read(Read::OwnerUnison));stage_=Stage::CompareOwner;break;
   case Stage::CompareOwner:stage_=Stage::AdmitOwner;break;
   case Stage::AdmitOwner:
    if(search_==Search::Owner)stage_=slotOwnerTag()==ownerTag()?Stage::ResetNewestAge:Stage::CompareAge;
    else if(search_==Search::NewestOwner)stage_=slotOwnerTag()==ownerTag()?Stage::CaptureHighAge:Stage::ReadIndex;
    else stage_=slotOwnerTag()==ownerTag()?Stage::ReadNote:Stage::CompareAge;break;
   case Stage::ResetNewestAge:bestAge_=0;search_=Search::NewestOwner;stage_=Stage::DispatchVoice;break;
   case Stage::TestAgeHigh:stage_=Stage::AdmitAgeHigh;break;
   case Stage::AdmitAgeHigh:
    if(search_==Search::Free)stage_=(age_>>16)?Stage::CompareAge:Stage::Complete;
    else stage_=(age_>>16)?Stage::ReadOwner:Stage::CompareAge;break;
   case Stage::CaptureHighAge:stage_=Stage::MaskActiveAge;break;
   case Stage::MaskActiveAge:stage_=Stage::AdmitActiveAge;break;
   case Stage::AdmitActiveAge:stage_=age_&0xc0000000?Stage::Complete:Stage::CompareAge;break;
   case Stage::ReadNote:note_=uint8_t(read(Read::SlotNote));stage_=Stage::CompareNote;break;
   case Stage::CompareNote:stage_=Stage::AdmitNote;break;
   case Stage::AdmitNote:stage_=incoming_==note_?Stage::SetRetrigger:Stage::CompareAge;break;
   case Stage::SetRetrigger:allocation_=4;stage_=Stage::PublishRetrigger;break;
   case Stage::PublishRetrigger:write(Write::Allocation,allocation_);stage_=Stage::JumpRetrigger;break;
   case Stage::JumpRetrigger:stage_=Stage::Complete;break;
   case Stage::CompareAge:stage_=Stage::AdmitAge;break;
   case Stage::AdmitAge:stage_=betterAge()?Stage::CopyAge:Stage::ReadIndex;break;
   case Stage::CopyAge:bestAge_=age_;stage_=Stage::ReadCandidate;break;
   case Stage::ReadCandidate:candidate_=uint8_t(read(Read::VoiceIndex));stage_=Stage::ReadIndex;break;
   case Stage::ReadIndex:index_=uint8_t(read(Read::VoiceIndex));stage_=Stage::IndexTick;break;
   case Stage::IndexTick:++index_;stage_=Stage::PublishIndex;break;
   case Stage::PublishIndex:write(Write::VoiceIndex,index_);stage_=Stage::RemainingTick;break;
   case Stage::RemainingTick:--remaining_;stage_=Stage::RemainingGate;break;
   case Stage::RemainingGate:stage_=remaining_?Stage::DispatchVoice:Stage::PublishCandidate;break;
   case Stage::PublishCandidate:write(Write::VoiceIndex,candidate_);stage_=search_==Search::Owner?Stage::PublishOwnerAllocation:search_==Search::Free?Stage::JumpRelease:Stage::TestBestHigh;break;
   case Stage::PublishOwnerAllocation:write(Write::Allocation,allocation_);stage_=Stage::TestBestHigh;break;
   case Stage::TestBestHigh:stage_=Stage::AdmitBestHigh;break;
   case Stage::AdmitBestHigh:
    if(search_==Search::Owner)stage_=(bestAge_>>16)?Stage::DispatchSelect:Stage::JumpPopulation;
    else if(search_==Search::NewestOwner)stage_=(bestAge_>>16)?Stage::SetNewestThree:Stage::PublishNewestAllocation;
    else stage_=(bestAge_>>16)?Stage::DispatchSelect:Stage::Complete;break;
   case Stage::JumpPopulation:stage_=Stage::Complete;break;
   case Stage::SetNewestThree:allocation_=3;stage_=Stage::CompareHighAge;break;
   case Stage::CompareHighAge:stage_=Stage::AdmitHighAge;break;
   case Stage::AdmitHighAge:stage_=(bestAge_>>16)>=256?Stage::PublishNewestAllocation:Stage::SetNewestTwo;break;
   case Stage::SetNewestTwo:allocation_=2;stage_=Stage::PublishNewestAllocation;break;
   case Stage::PublishNewestAllocation:write(Write::Allocation,allocation_);stage_=Stage::JumpPopulation;break;
   case Stage::JumpRelease:stage_=Stage::DispatchSelect;break;
   case Stage::DispatchSelect:stage_=Stage::AwaitSelect;break;
   case Stage::ReleaseWord:stage_=Stage::ReleaseAddressOne;break;
   case Stage::ReleaseAddressOne:stage_=Stage::DispatchWriteOne;break;
   case Stage::DispatchWriteOne:stage_=Stage::AwaitWriteOne;break;
   case Stage::ReleaseAddressTwo:stage_=Stage::DispatchWriteTwo;break;
   case Stage::DispatchWriteTwo:stage_=Stage::AwaitWriteTwo;break;
   case Stage::DispatchGate:stage_=Stage::AwaitGate;break;default:break;
   }
   available-=cost;p.clocks+=cost;++p.boundaries;
  }return p;
 }
private:
 bool indexPublication()const{return stage_==Stage::PublishInitialIndex||stage_==Stage::PublishIndex||stage_==Stage::PublishCandidate;}
 Stage stage_=Stage::DispatchPart;Search search_=Search::Owner;
 uint8_t controlPart_=0,slot_=0,mode_=0,held_=0,unison_=0,part_=0,candidate_=0,index_=0,remaining_=0,allocation_=0,incoming_=0,note_=0,slotPart_=0,slotUnison_=0;
 uint32_t age_=0,bestAge_=0;
};
class VoiceGateOffTask final {
public:
 enum class Stage {Value,Offset,Dispatch,Await,Complete};
 Stage stage()const{return stage_;}static constexpr uint32_t word(){return 0;}static constexpr uint16_t offset(){return 0x8085;}
 unsigned nextClocks()const{return stage_==Stage::Value?3:stage_==Stage::Offset?6:stage_==Stage::Dispatch?20:0;}
 bool advanceBefore(unsigned available){if(!nextClocks()||available<nextClocks())return false;
  stage_=stage_==Stage::Value?Stage::Offset:stage_==Stage::Offset?Stage::Dispatch:Stage::Await;return true;}
 bool resumeChild(){if(stage_!=Stage::Await)return false;stage_=Stage::Complete;return true;}
private:Stage stage_=Stage::Value;
};
}
