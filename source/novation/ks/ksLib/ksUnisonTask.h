#pragma once
#include <cstdint>
namespace ks {
// The encoded unison amount is an extra-voice count: zero still starts one
// voice. After each extra voice the live full part byte is reread/decremented;
// a final single-voice tail follows, including when the encoded amount is zero.
class UnisonTask final {
public:
 enum class Stage {PublishNote,PublishVelocity,DispatchPart,AwaitPart,ReadPatch,ReadMode,
  MaskCount,AdmitCount,DispatchPartAgain,AwaitPartAgain,ReadUnison,KeepUpper,MergeCount,
  WriteUnison,RetainPart,DispatchSingle,AwaitSingle,ReleasePart,ReadRemaining,
  Decrement,WriteRemaining,MaskRemaining,AdmitMore,Complete};
 enum class Read {Note,Velocity,PatchReference,Mode,Unison};
 enum class Write {Note,Velocity,Unison};
 enum class Child {None,Part,PartAgain,Single};
 Stage stage()const{return stage_;}uint8_t part()const{return part_;}
 uint8_t note()const{return note_;}uint8_t velocity()const{return velocity_;}
 uint8_t mode()const{return mode_;}uint8_t count()const{return count_;}uint8_t unison()const{return unison_;}
 uint8_t remaining()const{return remaining_;}bool complete()const{return stage_==Stage::Complete;}
 Child child()const{switch(stage_){case Stage::AwaitPart:return Child::Part;case Stage::AwaitPartAgain:return Child::PartAgain;case Stage::AwaitSingle:return Child::Single;default:return Child::None;}}
 bool resumeChild(uint8_t part=0){switch(stage_){case Stage::AwaitPart:part_=part;stage_=Stage::ReadPatch;return true;
 case Stage::AwaitPartAgain:part_=part;stage_=Stage::ReadUnison;return true;case Stage::AwaitSingle:stage_=Stage::ReleasePart;return true;default:return false;}}
 bool publication()const{return stage_==Stage::PublishNote||stage_==Stage::PublishVelocity||stage_==Stage::WriteUnison||stage_==Stage::WriteRemaining;}
 Write publicationKind()const{return stage_==Stage::PublishNote?Write::Note:stage_==Stage::PublishVelocity?Write::Velocity:Write::Unison;}
 unsigned nextClocks(unsigned storeClocks=3)const{switch(stage_){case Stage::PublishNote:case Stage::PublishVelocity:return 9+storeClocks;
 case Stage::DispatchPart:case Stage::DispatchPartAgain:case Stage::RetainPart:case Stage::ReleasePart:return 20;
 case Stage::ReadPatch:return 21;case Stage::ReadMode:case Stage::ReadUnison:case Stage::ReadRemaining:case Stage::DispatchSingle:return 15;
 case Stage::WriteUnison:case Stage::WriteRemaining:return 12+storeClocks;
 case Stage::AwaitPart:case Stage::AwaitPartAgain:case Stage::AwaitSingle:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){
 unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationKind()):3u)){
  if(cost>available-used)break;
  switch(stage_){case Stage::PublishNote:note_=uint8_t(read(Read::Note));write(Write::Note,note_);stage_=Stage::PublishVelocity;break;
  case Stage::PublishVelocity:velocity_=uint8_t(read(Read::Velocity));write(Write::Velocity,velocity_);stage_=Stage::DispatchPart;break;
  case Stage::DispatchPart:stage_=Stage::AwaitPart;break;case Stage::ReadPatch:read(Read::PatchReference);stage_=Stage::ReadMode;break;
  case Stage::ReadMode:mode_=uint8_t(read(Read::Mode));stage_=Stage::MaskCount;break;case Stage::MaskCount:count_=mode_&7;stage_=Stage::AdmitCount;break;
  case Stage::AdmitCount:stage_=count_?Stage::DispatchPartAgain:Stage::Complete;break;
  case Stage::DispatchPartAgain:stage_=Stage::AwaitPartAgain;break;case Stage::ReadUnison:unison_=uint8_t(read(Read::Unison));stage_=Stage::KeepUpper;break;
  case Stage::KeepUpper:unison_&=0xf8;stage_=Stage::MergeCount;break;case Stage::MergeCount:unison_|=count_;stage_=Stage::WriteUnison;break;
  case Stage::WriteUnison:write(Write::Unison,unison_);stage_=Stage::RetainPart;break;case Stage::RetainPart:stage_=Stage::DispatchSingle;break;
  case Stage::DispatchSingle:stage_=Stage::AwaitSingle;break;case Stage::ReleasePart:stage_=Stage::ReadRemaining;break;
  case Stage::ReadRemaining:remaining_=uint8_t(read(Read::Unison));stage_=Stage::Decrement;break;case Stage::Decrement:--remaining_;stage_=Stage::WriteRemaining;break;
  case Stage::WriteRemaining:write(Write::Unison,remaining_);stage_=Stage::MaskRemaining;break;case Stage::MaskRemaining:remaining_&=7;stage_=Stage::AdmitMore;break;
  case Stage::AdmitMore:stage_=remaining_?Stage::RetainPart:Stage::Complete;break;default:break;}
  used+=cost;
 }return used;}
private:Stage stage_=Stage::PublishNote;uint8_t part_=0,note_=0,velocity_=0,mode_=0,count_=0,unison_=0,remaining_=0;
};
}
