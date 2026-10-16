#pragma once
#include <cstdint>
namespace ks {
// Reset one keyboard map. Down clears only the physical-down map. Held also
// clears ordered-key counters, but deliberately retains per-key velocities.
class KeyResetTask final {
public:
 enum class Kind {Held,Down};
 enum class Stage {RetainPart,RetainBitmap,RetainCount,DispatchPart,AwaitPart,
  BindBitmap,LocateEnd,SetCount,ClearByte,Decrement,AdmitMore,ClearOrdered,
  ClearPhysical,ClearCursor,ReleaseCount,ReleaseBitmap,ReleasePart,Complete};
 explicit KeyResetTask(Kind kind):kind_(kind){}
 Stage stage()const{return stage_;}Kind kind()const{return kind_;}
 uint8_t part()const{return part_;}unsigned remaining()const{return remaining_;}
 unsigned byteOffset()const{return (kind_==Kind::Held?0xd0u:0xc0u)+remaining_-1;}
 unsigned publicationOffset()const{switch(stage_){case Stage::ClearByte:return byteOffset();case Stage::ClearOrdered:return 307;case Stage::ClearPhysical:return 306;default:return 308;}}
 bool publication()const{return stage_==Stage::ClearByte||stage_==Stage::ClearOrdered||stage_==Stage::ClearPhysical||stage_==Stage::ClearCursor;}
 bool awaitingPart()const{return stage_==Stage::AwaitPart;}
 bool resumePart(uint8_t part){if(!awaitingPart())return false;part_=part;stage_=Stage::BindBitmap;return true;}
 bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks(unsigned storeClocks=3)const{switch(stage_){
  case Stage::RetainPart:case Stage::RetainBitmap:case Stage::ReleaseBitmap:case Stage::ReleasePart:return 20;
  case Stage::RetainCount:case Stage::ReleaseCount:return 11;case Stage::DispatchPart:return 20;
  case Stage::LocateEnd:return 9;case Stage::SetCount:return 6;case Stage::ClearByte:return 5+storeClocks;
  case Stage::ClearOrdered:case Stage::ClearPhysical:case Stage::ClearCursor:return 12+storeClocks;
  case Stage::AwaitPart:case Stage::Complete:return 0;default:return 3;}}
 template<class Writer,class Timing>unsigned advanceBefore(unsigned available,Writer&& write,Timing&& timing){
  unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationOffset()):3u)){
   if(cost>available-used)break;
   switch(stage_){case Stage::RetainPart:stage_=Stage::RetainBitmap;break;
   case Stage::RetainBitmap:stage_=Stage::RetainCount;break;case Stage::RetainCount:stage_=Stage::DispatchPart;break;
   case Stage::DispatchPart:stage_=Stage::AwaitPart;break;case Stage::BindBitmap:stage_=Stage::LocateEnd;break;
   case Stage::LocateEnd:stage_=Stage::SetCount;break;case Stage::SetCount:remaining_=16;stage_=Stage::ClearByte;break;
   case Stage::ClearByte:write(byteOffset(),0);stage_=Stage::Decrement;break;case Stage::Decrement:--remaining_;stage_=Stage::AdmitMore;break;
   case Stage::AdmitMore:stage_=remaining_?Stage::ClearByte:kind_==Kind::Held?Stage::ClearOrdered:Stage::ReleaseCount;break;
   case Stage::ClearOrdered:write(307,0);stage_=Stage::ClearPhysical;break;case Stage::ClearPhysical:write(306,0);stage_=Stage::ClearCursor;break;
   case Stage::ClearCursor:write(308,0);stage_=Stage::ReleaseCount;break;case Stage::ReleaseCount:stage_=Stage::ReleaseBitmap;break;
   case Stage::ReleaseBitmap:stage_=Stage::ReleasePart;break;case Stage::ReleasePart:stage_=Stage::Complete;break;default:break;}
   used+=cost;
  }return used;
 }
private:Kind kind_;Stage stage_=Stage::RetainPart;uint8_t part_=0;unsigned remaining_=0;
};
}
