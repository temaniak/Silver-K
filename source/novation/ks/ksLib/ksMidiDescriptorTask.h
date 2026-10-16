#pragma once
#include <cstdint>
namespace ks {
struct MidiDescriptorKey final {
 enum class Family {Channel,System};Family family=Family::Channel;uint8_t index=0;
 static MidiDescriptorKey forStatus(uint8_t status){return status>=0xf0?MidiDescriptorKey{Family::System,uint8_t(status&15)}:MidiDescriptorKey{Family::Channel,uint8_t((status&0x70)>>4)};}
};
// Select a typed descriptor record; code addresses and CPU state are absent.
class MidiDescriptorTask final {
public:
 enum class Stage {CompareSystem,AdmitSystem,MaskChannel,QuarterChannel,SixteenthChannel,SetChannelStride,ChannelProduct,NormalizeChannel,ResolveChannel,
 MaskSystem,SetSystemStride,SystemProduct,NormalizeSystem,ResolveSystem,FinishChannel,FinishSystem,Complete};
 explicit MidiDescriptorTask(uint8_t status):status_(status),index_(status){}
 Stage stage()const{return stage_;}uint8_t status()const{return status_;}uint8_t index()const{return index_;}unsigned offset()const{return offset_;}
 MidiDescriptorKey key()const{return {status_>=0xf0?MidiDescriptorKey::Family::System:MidiDescriptorKey::Family::Channel,index_};}
 bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks()const{switch(stage_){case Stage::ChannelProduct:case Stage::SystemProduct:return 15;case Stage::ResolveChannel:case Stage::ResolveSystem:return 9;case Stage::FinishChannel:case Stage::FinishSystem:return 17;case Stage::Complete:return 0;default:return 3;}}
 unsigned advanceBefore(unsigned available){unsigned used=0;while(const auto cost=nextClocks()){
  if(cost>available-used)break;
  switch(stage_){case Stage::CompareSystem:stage_=Stage::AdmitSystem;break;case Stage::AdmitSystem:stage_=status_>=0xf0?Stage::MaskSystem:Stage::MaskChannel;break;
  case Stage::MaskChannel:index_&=0x70;stage_=Stage::QuarterChannel;break;case Stage::QuarterChannel:index_>>=2;stage_=Stage::SixteenthChannel;break;
  case Stage::SixteenthChannel:index_>>=2;stage_=Stage::SetChannelStride;break;case Stage::SetChannelStride:stage_=Stage::ChannelProduct;break;
  case Stage::ChannelProduct:offset_=index_*6;stage_=Stage::NormalizeChannel;break;case Stage::NormalizeChannel:stage_=Stage::ResolveChannel;break;case Stage::ResolveChannel:stage_=Stage::FinishChannel;break;
  case Stage::MaskSystem:index_&=15;stage_=Stage::SetSystemStride;break;case Stage::SetSystemStride:stage_=Stage::SystemProduct;break;case Stage::SystemProduct:offset_=index_*6;stage_=Stage::NormalizeSystem;break;case Stage::NormalizeSystem:stage_=Stage::ResolveSystem;break;case Stage::ResolveSystem:stage_=Stage::FinishSystem;break;
  case Stage::FinishChannel:case Stage::FinishSystem:stage_=Stage::Complete;break;default:break;}used+=cost;
 }return used;}
private:Stage stage_=Stage::CompareSystem;uint8_t status_,index_;unsigned offset_=0;
};
}
