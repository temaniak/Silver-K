#pragma once
#include <cstdint>
namespace ks {
// Retained ADC reference initialization arms each threshold. The indexed read
// uses the original byte-wrapped offset and subtracts one high-byte count.
class PanelAdcCalibrationTask final {
public:
 enum class Kind { Snapshot, Read };
 enum class Stage { SaveHistoryAddress, SaveSample, SelectHistory, SetCount, ArmThreshold, ReadSample, WriteHistory, WriteArmed, AdvanceHistory, DecrementCount, MoreSamples, RestoreSample, RestoreHistoryAddress, ReturnSnapshot, SaveAddress, ClearIndex, CaptureIndex, DoubleIndex, LocateHistory, ReadHistory, SubtractFloor, SelectFloor, ClearFloor, BindValue, RestoreAddress, ReturnValue, Complete };
 enum class Read { Sample, History };enum class Write { History, Armed };
 PanelAdcCalibrationTask(Kind kind,uint8_t index):stage_(kind==Kind::Snapshot?Stage::SaveHistoryAddress:Stage::SaveAddress),index_(index){}
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
 unsigned offset()const{return stage_<=Stage::ReturnSnapshot?position_*2:unsigned(uint8_t(index_<<1));}
 unsigned position()const{return position_;}uint8_t index()const{return index_;}uint16_t sample()const{return sample_;}
 uint8_t result()const{return uint8_t(adjusted_>>8);}
 unsigned nextClocks()const{switch(stage_){case Stage::WriteHistory:case Stage::ReadHistory:case Stage::SubtractFloor:return 6; case Stage::SelectHistory:case Stage::ReadSample:case Stage::WriteArmed:case Stage::LocateHistory:return 9; case Stage::ReturnSnapshot:case Stage::ReturnValue:return 17; case Stage::SaveHistoryAddress:case Stage::SaveSample:case Stage::RestoreSample:case Stage::RestoreHistoryAddress:case Stage::SaveAddress:case Stage::RestoreAddress:return 20; case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){
  const auto cost=nextClocks();if(!cost||cost>available)return 0;
  switch(stage_){
  case Stage::ReadSample:sample_=uint16_t(read(Read::Sample));next();break;
  case Stage::WriteHistory:write(Write::History,sample_);next();break;
  case Stage::WriteArmed:write(Write::Armed,1);next();break;
  case Stage::AdvanceHistory:++position_;next();break;
  case Stage::MoreSamples:stage_=position_<36?Stage::ReadSample:Stage::RestoreSample;break;
  case Stage::ReadHistory:sample_=uint16_t(read(Read::History));next();break;
  case Stage::SubtractFloor:underflow_=sample_<256;adjusted_=uint16_t(sample_-256);next();break;
  case Stage::SelectFloor:stage_=underflow_?Stage::ClearFloor:Stage::BindValue;break;
  case Stage::ClearFloor:adjusted_=0;next();break;
  case Stage::ReturnSnapshot:case Stage::ReturnValue:stage_=Stage::Complete;break;
  default:next();break;
  }return cost;
 }
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_;uint8_t index_;unsigned position_=0;uint16_t sample_=0,adjusted_=0;bool underflow_=false;
};
}
