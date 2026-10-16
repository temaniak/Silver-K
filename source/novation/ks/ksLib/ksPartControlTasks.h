#pragma once
#include <cstdint>
namespace ks {
class PartLookupTask final {
public:
 enum class Layout {Parameters,Controls,Voice};
 enum class Stage {ReadIndex,NormalizeIndex,Stride,Product,NormalizeProduct,Resolve,Complete};
 explicit PartLookupTask(Layout layout):layout_(layout){}
 static PartLookupTask controlsFor(uint8_t part){PartLookupTask task(Layout::Controls);task.index_=part;task.stage_=Stage::Stride;return task;}
 Layout layout()const{return layout_;}Stage stage()const{return stage_;}uint8_t index()const{return index_;}
 unsigned stride()const{return layout_==Layout::Parameters?334u:20u;}
 uint32_t offset()const{return offset_;}
 unsigned nextClocks()const{
  switch(stage_){case Stage::ReadIndex:return 12;case Stage::Stride:return layout_==Layout::Parameters?6:3;
  case Stage::Product:return layout_==Layout::Parameters?23:15;case Stage::Resolve:return 9;case Stage::Complete:return 0;default:return 3;}
 }
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){
  unsigned elapsed=0;while(const auto cost=nextClocks()){
   if(cost>available)break;
   switch(stage_){case Stage::ReadIndex:index_=uint8_t(read(layout_));stage_=layout_==Layout::Parameters?Stage::NormalizeIndex:Stage::Stride;break;
   case Stage::NormalizeIndex:stage_=Stage::Stride;break;case Stage::Stride:stage_=Stage::Product;break;
   case Stage::Product:offset_=index_*stride();stage_=layout_==Layout::Parameters?Stage::Resolve:Stage::NormalizeProduct;break;
   case Stage::NormalizeProduct:stage_=Stage::Resolve;break;case Stage::Resolve:stage_=Stage::Complete;break;case Stage::Complete:break;}
   elapsed+=cost;available-=cost;
  }return elapsed;
 }
private:Layout layout_;Stage stage_=Stage::ReadIndex;uint8_t index_=0;uint32_t offset_=0;
};
// Twenty live bytes are copied from the selected part controls to the current
// voice mirror. A byte is latched only at its scheduled read, never prefetched.
class PartMirrorTask final {
public:
 enum class Stage {Normalize,Count,Offset,Source,Destination,Read,Write,Advance,Tick,Gate,Complete};
 explicit PartMirrorTask(uint8_t part):part_(part){}
 Stage stage()const{return stage_;}unsigned part()const{return part_;}unsigned position()const{return position_;}
 unsigned remaining()const{return remaining_;}uint8_t byte()const{return byte_;}
 unsigned nextClocks(unsigned writeDataClocks=~0u)const{
  switch(stage_){case Stage::Count:return 6;case Stage::Offset:return 23;case Stage::Source:case Stage::Destination:return 9;
  case Stage::Read:return 8;case Stage::Write:return 3+(writeDataClocks==~0u?(position_&1?0u:3u):writeDataClocks);case Stage::Complete:return 0;default:return 3;}
 }
 // Publication timing comes from the native memory bus. Contiguous paired
 // byte stores share a bus transfer; an intervening writer can break the pair.
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& writeTiming){
  unsigned elapsed=0;while(const auto cost=nextClocks(stage_==Stage::Write?writeTiming(position_):3u)){
   if(cost>available)break;
   switch(stage_){case Stage::Normalize:stage_=Stage::Count;break;case Stage::Count:remaining_=20;stage_=Stage::Offset;break;
   case Stage::Offset:stage_=Stage::Source;break;case Stage::Source:stage_=Stage::Destination;break;case Stage::Destination:stage_=Stage::Read;break;
   case Stage::Read:byte_=uint8_t(read(part_,position_));stage_=Stage::Write;break;
   case Stage::Write:write(position_,byte_);stage_=Stage::Advance;break;case Stage::Advance:++position_;stage_=Stage::Tick;break;
   case Stage::Tick:--remaining_;stage_=Stage::Gate;break;case Stage::Gate:stage_=remaining_?Stage::Read:Stage::Complete;break;case Stage::Complete:break;}
   elapsed+=cost;available-=cost;
  }return elapsed;
 }
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){
  return advanceBefore(available,read,write,[](unsigned at){return at&1?0u:3u;});
 }
private:unsigned part_=0,position_=0,remaining_=0;uint8_t byte_=0;Stage stage_=Stage::Normalize;
};
}
