#pragma once
#include <cstdint>
namespace ks {
// Ordered held-key removal. The first matching duplicate is removed and the
// remaining suffix is moved forward; physical/ordered counters are republished
// from the live ordered count, including byte wraparound.
class KeyCompactTask final {
public:
 enum class Stage {RetainScratch,DispatchPart,AwaitPart,ReadCount,AdmitCount,BindList,LocateList,ReadKey,CompareKey,AdmitMatch,
  SearchDecrement,AdmitSearchMore,NoMatch,MatchDecrement,AdmitShift,ReadShift,WriteShift,ShiftDecrement,AdmitShiftMore,
  ReloadCount,DecrementCount,PublishOrdered,PublishPhysical,ReleaseScratch,Finish,Complete};
 enum class Read {Count,Key,Note};
 enum class Write {Shift,Ordered,Physical};
 Stage stage()const{return stage_;}uint8_t part()const{return part_;}unsigned position()const{return position_;}
 uint8_t remaining()const{return remaining_;}uint16_t key()const{return key_;}bool matches()const{return matches_;}
 bool awaitingPart()const{return stage_==Stage::AwaitPart;}
 bool resumePart(uint8_t part){if(!awaitingPart())return false;part_=part;stage_=Stage::ReadCount;return true;}
 bool complete()const{return stage_==Stage::Complete;}
 bool publication()const{return stage_==Stage::PublishOrdered||stage_==Stage::PublishPhysical;}
 unsigned publicationOffset()const{return stage_==Stage::PublishOrdered?307:306;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){
 case Stage::RetainScratch:case Stage::ReleaseScratch:return 11;case Stage::DispatchPart:return 20;
 case Stage::ReadCount:case Stage::ReloadCount:return 15;case Stage::LocateList:case Stage::WriteShift:return 9;
 case Stage::ReadKey:case Stage::ReadShift:return 8;case Stage::PublishOrdered:case Stage::PublishPhysical:return 12+store;
 case Stage::Finish:return 17;case Stage::AwaitPart:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){
  unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationOffset()):3)){
   if(cost>available-used)break;
   switch(stage_){
   case Stage::RetainScratch:stage_=Stage::DispatchPart;break;case Stage::DispatchPart:stage_=Stage::AwaitPart;break;
   case Stage::ReadCount:remaining_=uint8_t(read(Read::Count,0));stage_=Stage::AdmitCount;break;
   case Stage::AdmitCount:stage_=remaining_?Stage::BindList:Stage::ReleaseScratch;break;
   case Stage::BindList:stage_=Stage::LocateList;break;case Stage::LocateList:position_=0;stage_=Stage::ReadKey;break;
   case Stage::ReadKey:key_=uint16_t(read(Read::Key,position_++));stage_=Stage::CompareKey;break;
   case Stage::CompareKey:matches_=uint8_t(key_)==uint8_t(read(Read::Note,0));stage_=Stage::AdmitMatch;break;
   case Stage::AdmitMatch:stage_=matches_?Stage::MatchDecrement:Stage::SearchDecrement;break;
   case Stage::SearchDecrement:--remaining_;stage_=Stage::AdmitSearchMore;break;
   case Stage::AdmitSearchMore:stage_=remaining_?Stage::ReadKey:Stage::NoMatch;break;case Stage::NoMatch:stage_=Stage::ReleaseScratch;break;
   case Stage::MatchDecrement:--remaining_;stage_=Stage::AdmitShift;break;
   case Stage::AdmitShift:stage_=remaining_?Stage::ReadShift:Stage::ReloadCount;break;
   case Stage::ReadShift:key_=uint16_t(read(Read::Key,position_++));stage_=Stage::WriteShift;break;
   case Stage::WriteShift:write(Write::Shift,position_-2,key_);stage_=Stage::ShiftDecrement;break;
   case Stage::ShiftDecrement:--remaining_;stage_=Stage::AdmitShiftMore;break;
   case Stage::AdmitShiftMore:stage_=remaining_?Stage::ReadShift:Stage::ReloadCount;break;
   case Stage::ReloadCount:remaining_=uint8_t(read(Read::Count,0));stage_=Stage::DecrementCount;break;
   case Stage::DecrementCount:--remaining_;stage_=Stage::PublishOrdered;break;
   case Stage::PublishOrdered:write(Write::Ordered,307,remaining_);stage_=Stage::PublishPhysical;break;
   case Stage::PublishPhysical:write(Write::Physical,306,remaining_);stage_=Stage::ReleaseScratch;break;
   case Stage::ReleaseScratch:stage_=Stage::Finish;break;case Stage::Finish:stage_=Stage::Complete;break;default:break;
   }used+=cost;
  }return used;
 }
private:Stage stage_=Stage::RetainScratch;uint8_t part_=0,remaining_=0;unsigned position_=0;uint16_t key_=0;bool matches_=false;
};
// Adjacent held-key priority: keep exact unsigned byte distance comparison and
// the original velocity-memory index (the input note, not the replacement).
class MonoPriorityTask final {
public:
 enum class Stage {RetainPart,RetainIndex,RetainMap,RetainDistance,RetainCandidate,DispatchPart,AwaitPart,BindMap,LocateMap,
 DispatchUpper,AwaitUpper,CaptureUpper,CopyUpperDistance,MeasureUpper,DispatchLower,AwaitLower,CaptureVelocityKey,CopyLowerDistance,CaptureLower,MeasureLower,
 ObserveUpper,AdmitUpper,ObserveLower,AdmitLower,CompareDistances,AdmitDistance,SelectUpper,ObserveResult,AdmitResult,
 NormalizeKeyWord,NormalizeKey,LocateVelocity,ReadVelocity,ReleaseCandidate,ReleaseDistance,ReleaseMap,ReleaseIndex,ReleasePart,Finish,Complete};
 enum class Read {Candidate,Note,Velocity};
 Stage stage()const{return stage_;}uint8_t part()const{return part_;}uint8_t upper()const{return upper_;}uint8_t lower()const{return lower_;}
 uint8_t upperDistance()const{return upperDistance_;}uint8_t lowerDistance()const{return lowerDistance_;}
 uint8_t replacement()const{return replacement_;}uint8_t velocityKey()const{return velocityKey_;}uint8_t velocity()const{return velocity_;}
 bool awaitingPart()const{return stage_==Stage::AwaitPart;}bool awaitingUpper()const{return stage_==Stage::AwaitUpper;}bool awaitingLower()const{return stage_==Stage::AwaitLower;}
 bool resumePart(uint8_t part){if(!awaitingPart())return false;part_=part;stage_=Stage::BindMap;return true;}
 bool resumeUpper(){if(!awaitingUpper())return false;stage_=Stage::CaptureUpper;return true;}
 bool resumeLower(){if(!awaitingLower())return false;stage_=Stage::CaptureVelocityKey;return true;}
 bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks()const{switch(stage_){
 case Stage::RetainPart:case Stage::RetainIndex:case Stage::RetainMap:case Stage::ReleaseMap:case Stage::ReleaseIndex:case Stage::ReleasePart:case Stage::DispatchPart:case Stage::DispatchLower:return 20;
 case Stage::RetainDistance:case Stage::RetainCandidate:case Stage::ReleaseCandidate:case Stage::ReleaseDistance:return 11;
 case Stage::DispatchUpper:return 15;case Stage::LocateMap:return 9;case Stage::ReadVelocity:return 15;case Stage::Finish:return 17;
 case Stage::AwaitPart:case Stage::AwaitUpper:case Stage::AwaitLower:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){
  if(cost>available-used)break;
  switch(stage_){
  case Stage::RetainPart:stage_=Stage::RetainIndex;break;case Stage::RetainIndex:stage_=Stage::RetainMap;break;case Stage::RetainMap:stage_=Stage::RetainDistance;break;
  case Stage::RetainDistance:stage_=Stage::RetainCandidate;break;case Stage::RetainCandidate:stage_=Stage::DispatchPart;break;case Stage::DispatchPart:stage_=Stage::AwaitPart;break;
  case Stage::BindMap:stage_=Stage::LocateMap;break;case Stage::LocateMap:stage_=Stage::DispatchUpper;break;case Stage::DispatchUpper:stage_=Stage::AwaitUpper;break;
  case Stage::CaptureUpper:upper_=uint8_t(read(Read::Candidate,0));stage_=Stage::CopyUpperDistance;break;
  case Stage::CopyUpperDistance:upperDistance_=upper_;stage_=Stage::MeasureUpper;break;
  case Stage::MeasureUpper:upperDistance_=uint8_t(upperDistance_-uint8_t(read(Read::Note,0)));stage_=Stage::DispatchLower;break;
  case Stage::DispatchLower:stage_=Stage::AwaitLower;break;case Stage::CaptureVelocityKey:velocityKey_=uint8_t(read(Read::Note,0));stage_=Stage::CopyLowerDistance;break;
  case Stage::CopyLowerDistance:lowerDistance_=uint8_t(read(Read::Note,0));stage_=Stage::CaptureLower;break;
  case Stage::CaptureLower:lower_=replacement_=uint8_t(read(Read::Candidate,0));stage_=Stage::MeasureLower;break;
  case Stage::MeasureLower:lowerDistance_=uint8_t(lowerDistance_-lower_);stage_=Stage::ObserveUpper;break;
  case Stage::ObserveUpper:stage_=Stage::AdmitUpper;break;
  case Stage::AdmitUpper:stage_=upper_>=128?Stage::ObserveResult:Stage::ObserveLower;break;
  case Stage::ObserveLower:stage_=Stage::AdmitLower;break;
  case Stage::AdmitLower:stage_=lower_>=128?Stage::SelectUpper:Stage::CompareDistances;break;
  case Stage::CompareDistances:stage_=Stage::AdmitDistance;break;
  case Stage::AdmitDistance:stage_=upperDistance_>lowerDistance_?Stage::ObserveResult:Stage::SelectUpper;break;
  case Stage::SelectUpper:replacement_=upper_;stage_=Stage::ObserveResult;break;
  case Stage::ObserveResult:stage_=Stage::AdmitResult;break;
  case Stage::AdmitResult:stage_=replacement_>=128?Stage::ReleaseCandidate:Stage::NormalizeKeyWord;break;
  case Stage::NormalizeKeyWord:stage_=Stage::NormalizeKey;break;case Stage::NormalizeKey:stage_=Stage::LocateVelocity;break;
  case Stage::LocateVelocity:stage_=Stage::ReadVelocity;break;
  case Stage::ReadVelocity:velocity_=uint8_t(read(Read::Velocity,velocityKey_));stage_=Stage::ReleaseCandidate;break;
  case Stage::ReleaseCandidate:stage_=Stage::ReleaseDistance;break;case Stage::ReleaseDistance:stage_=Stage::ReleaseMap;break;
  case Stage::ReleaseMap:stage_=Stage::ReleaseIndex;break;case Stage::ReleaseIndex:stage_=Stage::ReleasePart;break;case Stage::ReleasePart:stage_=Stage::Finish;break;
  case Stage::Finish:stage_=Stage::Complete;break;default:break;
  }used+=cost;
 }return used;}
private:Stage stage_=Stage::RetainPart;uint8_t part_=0,upper_=0xff,lower_=0xff,upperDistance_=0,lowerDistance_=0,replacement_=0xff,velocityKey_=0,velocity_=0;
};
}
