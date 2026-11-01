#pragma once
#include <cstdint>
namespace ks {
class MidiPartInitTask final {
public:
 enum class Stage {ClearPart,SetParts,PublishPart,DispatchPart,AwaitPart,BindData,SetSentinel,PublishSentinel,ReadReference,PublishReference,LocateData,SetDataCount,ClearDataValue,PublishData,AdvanceData,TickData,AdmitData,TickPart,TickParts,AdmitParts,Finish,Complete};
 enum class Write {SelectedPart,Sentinel,Reference,Data};
 Stage stage()const{return stage_;}uint8_t index()const{return index_;}uint8_t part()const{return part_;}unsigned remainingParts()const{return remainingParts_;}unsigned field()const{return field_;}
 unsigned dataOffset()const{return dataOffset_;}unsigned remainingData()const{return remainingData_;}uint32_t reference()const{return reference_;}
 unsigned sentinelOffset()const{constexpr unsigned offsets[]{314,331,332,318,328,327,324};return offsets[field_];}
 bool awaitingPart()const{return stage_==Stage::AwaitPart;}bool resumePart(uint8_t p){if(!awaitingPart())return false;part_=p;stage_=Stage::BindData;return true;}
 bool publication()const{return stage_==Stage::PublishPart||stage_==Stage::PublishSentinel||stage_==Stage::PublishData;}
 Write publicationKind()const{return stage_==Stage::PublishPart?Write::SelectedPart:stage_==Stage::PublishSentinel?Write::Sentinel:Write::Data;}
 unsigned publicationOffset()const{return stage_==Stage::PublishPart?0:stage_==Stage::PublishSentinel?sentinelOffset():dataOffset_;}
 bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::PublishPart:return 9+store;case Stage::DispatchPart:return 20;case Stage::PublishSentinel:return 12+store;case Stage::ReadReference:return 24;case Stage::PublishReference:return 21;case Stage::LocateData:return 9;case Stage::SetDataCount:return 6;case Stage::PublishData:return 3+store;case Stage::Finish:return 17;case Stage::AwaitPart:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationKind(),publicationOffset()):3)){
  if(cost>available-used)break;switch(stage_){
  case Stage::ClearPart:index_=0;stage_=Stage::SetParts;break;case Stage::SetParts:remainingParts_=5;stage_=Stage::PublishPart;break;case Stage::PublishPart:write(Write::SelectedPart,0,index_);stage_=Stage::DispatchPart;break;case Stage::DispatchPart:stage_=Stage::AwaitPart;break;
  case Stage::BindData:stage_=Stage::SetSentinel;break;case Stage::SetSentinel:field_=0;stage_=Stage::PublishSentinel;break;
  case Stage::PublishSentinel:write(Write::Sentinel,sentinelOffset(),255);if(++field_==7)stage_=Stage::ReadReference;break;
  case Stage::ReadReference:reference_=uint32_t(read());stage_=Stage::PublishReference;break;case Stage::PublishReference:write(Write::Reference,0,reference_);stage_=Stage::LocateData;break;
  case Stage::LocateData:dataOffset_=60;stage_=Stage::SetDataCount;break;case Stage::SetDataCount:remainingData_=253;stage_=Stage::ClearDataValue;break;case Stage::ClearDataValue:stage_=Stage::PublishData;break;
  case Stage::PublishData:write(Write::Data,dataOffset_,0);stage_=Stage::AdvanceData;break;case Stage::AdvanceData:++dataOffset_;stage_=Stage::TickData;break;case Stage::TickData:--remainingData_;stage_=Stage::AdmitData;break;
  case Stage::AdmitData:stage_=remainingData_?Stage::PublishData:Stage::TickPart;break;case Stage::TickPart:++index_;stage_=Stage::TickParts;break;case Stage::TickParts:--remainingParts_;stage_=Stage::AdmitParts;break;
  case Stage::AdmitParts:stage_=remainingParts_?Stage::PublishPart:Stage::Finish;break;case Stage::Finish:stage_=Stage::Complete;break;default:break;}used+=cost;
 }return used;}
private:Stage stage_=Stage::ClearPart;uint8_t index_=0,part_=0;unsigned remainingParts_=0,field_=0,dataOffset_=0,remainingData_=0;uint32_t reference_=0;
};
class MidiControlInitTask final {
public:
 enum class Stage {BindControls,SetParts,BindPatch,BindTemplate,SetBytes,CaptureDestination,ReadByte,PublishByte,AdvanceByte,TickByte,AdmitByte,PublishPatch,AdvancePatch,TickPart,AdmitPart,Finish,Complete};
 enum class Write {Byte,Patch};
 Stage stage()const{return stage_;}uint8_t part()const{return part_;}unsigned offset()const{return offset_;}unsigned remainingBytes()const{return remainingBytes_;}unsigned remainingParts()const{return remainingParts_;}uint8_t byte()const{return byte_;}uint32_t patch()const{return patch_;}
 bool publication()const{return stage_==Stage::PublishByte;}bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::BindControls:case Stage::BindPatch:case Stage::BindTemplate:return 9;case Stage::SetBytes:return 6;case Stage::ReadByte:return 8;case Stage::PublishByte:return 3+store;case Stage::PublishPatch:return 21;case Stage::AdvancePatch:return 9;case Stage::Finish:return 17;case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;while(const auto cost=nextClocks(publication()?timing(part_,offset_):3)){
  if(cost>available-used)break;switch(stage_){
  case Stage::BindControls:part_=0;stage_=Stage::SetParts;break;case Stage::SetParts:remainingParts_=5;stage_=Stage::BindPatch;break;case Stage::BindPatch:patch_=0x168fc;stage_=Stage::BindTemplate;break;
  case Stage::BindTemplate:offset_=0;stage_=Stage::SetBytes;break;case Stage::SetBytes:remainingBytes_=20;stage_=Stage::CaptureDestination;break;case Stage::CaptureDestination:stage_=Stage::ReadByte;break;
  case Stage::ReadByte:byte_=uint8_t(read(offset_));stage_=Stage::PublishByte;break;case Stage::PublishByte:write(Write::Byte,part_,offset_,byte_);stage_=Stage::AdvanceByte;break;
  case Stage::AdvanceByte:++offset_;stage_=Stage::TickByte;break;case Stage::TickByte:--remainingBytes_;stage_=Stage::AdmitByte;break;case Stage::AdmitByte:stage_=remainingBytes_?Stage::ReadByte:Stage::PublishPatch;break;
  case Stage::PublishPatch:write(Write::Patch,part_,0,patch_);stage_=Stage::AdvancePatch;break;case Stage::AdvancePatch:patch_+=256;stage_=Stage::TickPart;break;
  case Stage::TickPart:++part_;--remainingParts_;stage_=Stage::AdmitPart;break;case Stage::AdmitPart:stage_=remainingParts_?Stage::BindTemplate:Stage::Finish;break;case Stage::Finish:stage_=Stage::Complete;break;default:break;}used+=cost;
 }return used;}
private:Stage stage_=Stage::BindControls;uint8_t part_=0,byte_=0;unsigned offset_=0,remainingParts_=0,remainingBytes_=0;uint32_t patch_=0;
};
}
