#pragma once
#include <cstdint>
namespace ks {
// Read the selected keyboard map in original order, stopping at its first
// nonzero word. The result retains that word, not merely a boolean.
class KeyActivityTask final {
public:
 enum class Kind {Physical,Held};
 enum class Stage {DispatchPart,AwaitPart,BindMap,LocateMap,SetRemaining,ReadWord,AdmitWord,Decrement,AdmitMore,Finish,Complete};
 explicit KeyActivityTask(Kind kind):kind_(kind){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}
 uint8_t part()const{return part_;}unsigned cursor()const{return cursor_;}
 unsigned remaining()const{return remaining_;}uint16_t result()const{return result_;}
 unsigned byteOffset()const{return (kind_==Kind::Held?0xd0u:0xc0u)+2*cursor_;}
 bool awaitingPart()const{return stage_==Stage::AwaitPart;}
 bool complete()const{return stage_==Stage::Complete;}
 bool resumePart(uint8_t part){if(!awaitingPart())return false;part_=part;stage_=Stage::BindMap;return true;}
 unsigned nextClocks()const{switch(stage_){case Stage::DispatchPart:return 20;case Stage::LocateMap:return 9;
 case Stage::SetRemaining:return 6;case Stage::ReadWord:return 8;case Stage::Finish:return 17;
 case Stage::AwaitPart:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;
  while(const auto cost=nextClocks()){if(cost>available-used)break;
   switch(stage_){case Stage::DispatchPart:stage_=Stage::AwaitPart;break;case Stage::BindMap:stage_=Stage::LocateMap;break;
   case Stage::LocateMap:stage_=Stage::SetRemaining;break;case Stage::SetRemaining:remaining_=8;stage_=Stage::ReadWord;break;
   case Stage::ReadWord:result_=read(byteOffset());++cursor_;stage_=Stage::AdmitWord;break;
   case Stage::AdmitWord:stage_=result_?Stage::Finish:Stage::Decrement;break;
   case Stage::Decrement:--remaining_;stage_=Stage::AdmitMore;break;
   case Stage::AdmitMore:stage_=remaining_?Stage::ReadWord:Stage::Finish;break;
   case Stage::Finish:stage_=Stage::Complete;break;default:break;}used+=cost;
  }return used;
 }
private:Kind kind_;Stage stage_=Stage::DispatchPart;uint8_t part_=0;unsigned cursor_=0,remaining_=0;uint16_t result_=0;
};
}
