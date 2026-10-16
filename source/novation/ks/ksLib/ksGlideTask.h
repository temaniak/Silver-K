#pragma once
#include <cstdint>
namespace ks {
// Shared native pitch/glide preparation for full parameter upload and note
// startup. Logical values survive child calls; mutable patch/slot fields are
// observed only at the corresponding admitted boundary.
class GlideTask final {
public:
 enum class Stage {ReadPatchPitch,AdmitPatchPitch,DispatchPitchVoice,AwaitPitchVoice,ReadSlotPitch,BiasPitch,ClearPitchLow,CapturePitch,ClearPitchMiddle,OffsetPitch,DispatchPitch,AwaitPitch,
  ReadDetune,BiasDetune,AdmitDetune,CopyMagnitude,AdmitMagnitudeSign,NegateMagnitude,DispatchDetuneVoice,AwaitDetuneVoice,WriteDistance,ReadTransposed,AddDetune,WritePrevious,
  ReadTime,ReadMode,ShiftModeTwo,ShiftModeOne,MaskMode,CompareMode,AdmitMode,ReadAllocation,CompareOne,AdmitOne,CompareTwo,AdmitTwo,ClearTime,TestTime,AdmitTime,
  ZeroRate,OffsetZeroRate,DispatchZeroRate,AwaitZeroRate,UnitySecond,OffsetUnitySecond,DispatchUnitySecond,AwaitUnitySecond,JumpPreviousLong,
  InvertTime,CopyInverted,SetMultiplier,MultiplyTime,SetThreshold,SubtractThreshold,AdmitThreshold,ClearAdjustment,AddAdjustment,ReadBase,AddBase,DispatchCoefficient,AwaitCoefficient,
  ReadDistanceMode,AdmitDistanceMode,DispatchDistanceVoice,AwaitDistanceVoice,ReadDistance,AdmitDistance,PackDistance,CopyDistance,MultiplyHigh,MultiplyLow,CaptureLowHigh,ClearLowHigh,AddProducts,CompareMinimum,AdmitMinimum,ClampMinimum,
  OffsetScaledRate,DispatchScaledRate,AwaitScaledRate,ZeroSecondRate,OffsetScaledSecond,DispatchScaledSecond,AwaitScaledSecond,JumpPreviousShort,
  CaptureCoefficient,RetainCoefficient,SetShifts,ShiftRate,DecrementShifts,AdmitShifts,OffsetBalancedRate,DispatchBalancedRate,AwaitBalancedRate,ReleaseCoefficient,OffsetBalancedSecond,DispatchBalancedSecond,AwaitBalancedSecond,
  ReadPrevious,BiasPrevious,ClearPreviousLow,CapturePrevious,ClearPreviousMiddle,OffsetPrevious,DispatchPrevious,AwaitPrevious,ReadGateAllocation,CompareGate,AdmitGate,Complete};
 enum class Read {PatchPitch,SlotPitch,Detune,Transposed,Time,Mode,Allocation,Base,DistanceMode,Distance,Previous};
 enum class Write {Distance,Previous};
 enum class Child {None,PitchVoice,Pitch,DetuneVoice,ZeroRate,UnitySecond,Coefficient,DistanceVoice,ScaledRate,ScaledSecond,BalancedRate,BalancedSecond,Previous};
 explicit GlideTask(bool relative):relative_(relative){}
 Stage stage()const{return stage_;}bool relative()const{return relative_;}bool resumeGate()const{return gateAllocation_<=2;}
 uint8_t slot()const{return slot_;}uint8_t pitch()const{return pitch_;}uint8_t detune()const{return detune_;}uint8_t magnitude()const{return magnitude_;}
 uint8_t transposed()const{return transposed_;}uint8_t previous()const{return previous_;}uint8_t time()const{return time_;}uint8_t mode()const{return mode_;}
 uint8_t allocation()const{return allocation_;}uint8_t inverted()const{return inverted_;}uint8_t adjustment()const{return adjustment_;}
 uint16_t rate()const{return rate_;}uint16_t base()const{return base_;}uint32_t coefficient()const{return coefficient_;}
 uint8_t distanceMode()const{return distanceMode_;}uint8_t distance()const{return distance_;}uint32_t highProduct()const{return highProduct_;}
 uint32_t lowProduct()const{return lowProduct_;}uint32_t packed()const{return packed_;}uint32_t balanced()const{return balanced_;}uint8_t shifts()const{return shifts_;}
 uint8_t gateAllocation()const{return gateAllocation_;}uint32_t dspWord()const{return dspWord_;}uint16_t offset()const{return offset_;}
 bool complete()const{return stage_==Stage::Complete;}
 bool publication()const{return stage_==Stage::WriteDistance||stage_==Stage::WritePrevious;}
 Write publicationKind()const{return stage_==Stage::WriteDistance?Write::Distance:Write::Previous;}
 Child child()const{switch(stage_){case Stage::AwaitPitchVoice:return Child::PitchVoice;case Stage::AwaitPitch:return Child::Pitch;
 case Stage::AwaitDetuneVoice:return Child::DetuneVoice;case Stage::AwaitZeroRate:return Child::ZeroRate;case Stage::AwaitUnitySecond:return Child::UnitySecond;
 case Stage::AwaitCoefficient:return Child::Coefficient;case Stage::AwaitDistanceVoice:return Child::DistanceVoice;case Stage::AwaitScaledRate:return Child::ScaledRate;
 case Stage::AwaitScaledSecond:return Child::ScaledSecond;case Stage::AwaitBalancedRate:return Child::BalancedRate;case Stage::AwaitBalancedSecond:return Child::BalancedSecond;
 case Stage::AwaitPrevious:return Child::Previous;default:return Child::None;}}
 bool resumeChild(uint32_t result=0){switch(stage_){case Stage::AwaitPitchVoice:slot_=uint8_t(result);stage_=Stage::ReadSlotPitch;return true;
 case Stage::AwaitPitch:stage_=Stage::ReadDetune;return true;case Stage::AwaitDetuneVoice:slot_=uint8_t(result);stage_=Stage::WriteDistance;return true;
 case Stage::AwaitZeroRate:stage_=Stage::UnitySecond;return true;case Stage::AwaitUnitySecond:stage_=Stage::JumpPreviousLong;return true;
 case Stage::AwaitCoefficient:coefficient_=result;stage_=Stage::ReadDistanceMode;return true;case Stage::AwaitDistanceVoice:slot_=uint8_t(result);stage_=Stage::ReadDistance;return true;
 case Stage::AwaitScaledRate:stage_=Stage::ZeroSecondRate;return true;case Stage::AwaitScaledSecond:stage_=Stage::JumpPreviousShort;return true;
 case Stage::AwaitBalancedRate:stage_=Stage::ReleaseCoefficient;return true;case Stage::AwaitBalancedSecond:stage_=Stage::ReadPrevious;return true;
 case Stage::AwaitPrevious:stage_=relative_?Stage::ReadGateAllocation:Stage::Complete;return true;default:return false;}}
 unsigned nextClocks(unsigned storeClocks=3)const{switch(stage_){
 case Stage::ReadPatchPitch:case Stage::ReadSlotPitch:case Stage::ReadDetune:case Stage::ReadTransposed:case Stage::ReadTime:case Stage::ReadMode:case Stage::ReadDistanceMode:case Stage::ReadDistance:return 15;
 case Stage::ReadAllocation:case Stage::ReadBase:case Stage::ReadPrevious:case Stage::ReadGateAllocation:return 12;
 case Stage::WriteDistance:return 12+storeClocks;case Stage::WritePrevious:return 9+storeClocks;
 case Stage::DispatchPitchVoice:case Stage::DispatchPitch:case Stage::DispatchDetuneVoice:case Stage::DispatchZeroRate:case Stage::DispatchUnitySecond:case Stage::DispatchCoefficient:
 case Stage::DispatchDistanceVoice:case Stage::DispatchScaledRate:case Stage::DispatchScaledSecond:case Stage::DispatchBalancedRate:case Stage::DispatchBalancedSecond:case Stage::DispatchPrevious:
 case Stage::RetainCoefficient:case Stage::ReleaseCoefficient:return 20;
 case Stage::OffsetPitch:case Stage::OffsetZeroRate:case Stage::OffsetUnitySecond:case Stage::OffsetScaledRate:case Stage::OffsetScaledSecond:case Stage::OffsetBalancedRate:case Stage::OffsetBalancedSecond:case Stage::OffsetPrevious:case Stage::ClampMinimum:return 6;
 case Stage::UnitySecond:case Stage::CompareMinimum:return 9;case Stage::JumpPreviousLong:case Stage::AdmitGate:return 8;
 case Stage::MultiplyTime:return 15;case Stage::MultiplyHigh:case Stage::MultiplyLow:return 23;
 case Stage::AwaitPitchVoice:case Stage::AwaitPitch:case Stage::AwaitDetuneVoice:case Stage::AwaitZeroRate:case Stage::AwaitUnitySecond:case Stage::AwaitCoefficient:
 case Stage::AwaitDistanceVoice:case Stage::AwaitScaledRate:case Stage::AwaitScaledSecond:case Stage::AwaitBalancedRate:case Stage::AwaitBalancedSecond:case Stage::AwaitPrevious:case Stage::Complete:return 0;
 default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;
 while(const auto cost=nextClocks(publication()?timing(publicationKind()):3u)){if(cost>available-used)break;
  switch(stage_){case Stage::ReadPatchPitch:pitch_=uint8_t(read(Read::PatchPitch));stage_=Stage::AdmitPatchPitch;break;
  case Stage::AdmitPatchPitch:stage_=pitch_?Stage::BiasPitch:Stage::DispatchPitchVoice;break;case Stage::DispatchPitchVoice:stage_=Stage::AwaitPitchVoice;break;
  case Stage::ReadSlotPitch:pitch_=uint8_t(read(Read::SlotPitch));stage_=Stage::BiasPitch;break;case Stage::BiasPitch:pitch_=uint8_t(pitch_-60);stage_=Stage::ClearPitchLow;break;
  case Stage::ClearPitchLow:stage_=Stage::CapturePitch;break;case Stage::CapturePitch:stage_=Stage::ClearPitchMiddle;break;
  case Stage::ClearPitchMiddle:packet(uint32_t(pitch_)<<16,0x8c);stage_=relative_?Stage::OffsetPitch:Stage::DispatchPitch;break;
  case Stage::OffsetPitch:stage_=Stage::DispatchPitch;break;case Stage::DispatchPitch:stage_=Stage::AwaitPitch;break;
  case Stage::ReadDetune:detune_=uint8_t(read(Read::Detune));stage_=Stage::BiasDetune;break;case Stage::BiasDetune:detune_=uint8_t(detune_-64);stage_=Stage::AdmitDetune;break;
  case Stage::AdmitDetune:stage_=detune_?Stage::CopyMagnitude:Stage::ReadTime;break;case Stage::CopyMagnitude:magnitude_=detune_;stage_=Stage::AdmitMagnitudeSign;break;
  case Stage::AdmitMagnitudeSign:stage_=detune_&0x80?Stage::NegateMagnitude:Stage::DispatchDetuneVoice;break;
  case Stage::NegateMagnitude:magnitude_=uint8_t(0-magnitude_);stage_=Stage::DispatchDetuneVoice;break;case Stage::DispatchDetuneVoice:stage_=Stage::AwaitDetuneVoice;break;
  case Stage::WriteDistance:write(Write::Distance,magnitude_);stage_=Stage::ReadTransposed;break;case Stage::ReadTransposed:transposed_=uint8_t(read(Read::Transposed));stage_=Stage::AddDetune;break;
  case Stage::AddDetune:previous_=uint8_t(transposed_+detune_);stage_=Stage::WritePrevious;break;case Stage::WritePrevious:write(Write::Previous,previous_);stage_=Stage::ReadTime;break;
  case Stage::ReadTime:time_=uint8_t(read(Read::Time));stage_=Stage::ReadMode;break;case Stage::ReadMode:mode_=uint8_t(read(Read::Mode));stage_=Stage::ShiftModeTwo;break;
  case Stage::ShiftModeTwo:mode_>>=2;stage_=Stage::ShiftModeOne;break;case Stage::ShiftModeOne:mode_>>=1;stage_=Stage::MaskMode;break;case Stage::MaskMode:mode_&=3;stage_=Stage::CompareMode;break;
  case Stage::CompareMode:stage_=Stage::AdmitMode;break;case Stage::AdmitMode:stage_=mode_==1?Stage::ReadAllocation:Stage::TestTime;break;
  case Stage::ReadAllocation:allocation_=uint8_t(read(Read::Allocation));stage_=Stage::CompareOne;break;case Stage::CompareOne:stage_=Stage::AdmitOne;break;
  case Stage::AdmitOne:stage_=allocation_==1?Stage::ClearTime:Stage::CompareTwo;break;case Stage::CompareTwo:stage_=Stage::AdmitTwo;break;
  case Stage::AdmitTwo:stage_=allocation_==2?Stage::ClearTime:Stage::TestTime;break;case Stage::ClearTime:time_=0;stage_=Stage::TestTime;break;
  case Stage::TestTime:stage_=Stage::AdmitTime;break;case Stage::AdmitTime:stage_=time_?Stage::InvertTime:Stage::ZeroRate;break;
  case Stage::ZeroRate:packet(0,0x8d);stage_=relative_?Stage::OffsetZeroRate:Stage::DispatchZeroRate;break;case Stage::OffsetZeroRate:stage_=Stage::DispatchZeroRate;break;
  case Stage::DispatchZeroRate:stage_=Stage::AwaitZeroRate;break;case Stage::UnitySecond:packet(0x7fffff,0x8e);stage_=relative_?Stage::OffsetUnitySecond:Stage::DispatchUnitySecond;break;
  case Stage::OffsetUnitySecond:stage_=Stage::DispatchUnitySecond;break;case Stage::DispatchUnitySecond:stage_=Stage::AwaitUnitySecond;break;case Stage::JumpPreviousLong:stage_=Stage::ReadPrevious;break;
  case Stage::InvertTime:inverted_=time_^0x7f;stage_=Stage::CopyInverted;break;case Stage::CopyInverted:stage_=Stage::SetMultiplier;break;case Stage::SetMultiplier:stage_=Stage::MultiplyTime;break;
  case Stage::MultiplyTime:rate_=uint16_t(inverted_)*93;stage_=Stage::SetThreshold;break;case Stage::SetThreshold:stage_=Stage::SubtractThreshold;break;
  case Stage::SubtractThreshold:adjustment_=uint8_t(inverted_-118);stage_=Stage::AdmitThreshold;break;
  case Stage::AdmitThreshold:stage_=inverted_>118?Stage::ClearAdjustment:Stage::ReadBase;break;case Stage::ClearAdjustment:stage_=Stage::AddAdjustment;break;
  case Stage::AddAdjustment:rate_=uint16_t(rate_+(uint16_t(adjustment_)<<8));stage_=Stage::ReadBase;break;case Stage::ReadBase:base_=uint16_t(read(Read::Base));stage_=Stage::AddBase;break;
  case Stage::AddBase:rate_=uint16_t(rate_+base_);stage_=Stage::DispatchCoefficient;break;case Stage::DispatchCoefficient:stage_=Stage::AwaitCoefficient;break;
  case Stage::ReadDistanceMode:distanceMode_=uint8_t(read(Read::DistanceMode));stage_=Stage::AdmitDistanceMode;break;
  case Stage::AdmitDistanceMode:stage_=distanceMode_?Stage::DispatchDistanceVoice:Stage::CaptureCoefficient;break;case Stage::DispatchDistanceVoice:stage_=Stage::AwaitDistanceVoice;break;
  case Stage::ReadDistance:distance_=uint8_t(read(Read::Distance));stage_=Stage::AdmitDistance;break;case Stage::AdmitDistance:stage_=distance_?Stage::PackDistance:Stage::CaptureCoefficient;break;
  case Stage::PackDistance:stage_=Stage::CopyDistance;break;case Stage::CopyDistance:stage_=Stage::MultiplyHigh;break;
  case Stage::MultiplyHigh:highProduct_=uint16_t(coefficient_>>16)*uint32_t(uint16_t(distance_)<<8);stage_=Stage::MultiplyLow;break;
  case Stage::MultiplyLow:lowProduct_=uint16_t(coefficient_)*uint32_t(uint16_t(distance_)<<8);stage_=Stage::CaptureLowHigh;break;
  case Stage::CaptureLowHigh:stage_=Stage::ClearLowHigh;break;case Stage::ClearLowHigh:stage_=Stage::AddProducts;break;
  case Stage::AddProducts:packed_=highProduct_+(lowProduct_>>16);stage_=Stage::CompareMinimum;break;case Stage::CompareMinimum:stage_=Stage::AdmitMinimum;break;
  case Stage::AdmitMinimum:if(packed_<0x200)stage_=Stage::ClampMinimum;else scaledRate();break;
  case Stage::ClampMinimum:packed_=0x200;scaledRate();break;case Stage::OffsetScaledRate:stage_=Stage::DispatchScaledRate;break;case Stage::DispatchScaledRate:stage_=Stage::AwaitScaledRate;break;
  case Stage::ZeroSecondRate:packet(0,0x8e);stage_=relative_?Stage::OffsetScaledSecond:Stage::DispatchScaledSecond;break;
  case Stage::OffsetScaledSecond:stage_=Stage::DispatchScaledSecond;break;case Stage::DispatchScaledSecond:stage_=Stage::AwaitScaledSecond;break;case Stage::JumpPreviousShort:stage_=Stage::ReadPrevious;break;
  case Stage::CaptureCoefficient:balanced_=coefficient_;stage_=Stage::RetainCoefficient;break;case Stage::RetainCoefficient:stage_=Stage::SetShifts;break;case Stage::SetShifts:shifts_=5;stage_=Stage::ShiftRate;break;
  case Stage::ShiftRate:balanced_>>=1;stage_=Stage::DecrementShifts;break;case Stage::DecrementShifts:--shifts_;stage_=Stage::AdmitShifts;break;
  case Stage::AdmitShifts:if(shifts_)stage_=Stage::ShiftRate;else{packet(balanced_>>8,0x8d);stage_=relative_?Stage::OffsetBalancedRate:Stage::DispatchBalancedRate;}break;
  case Stage::OffsetBalancedRate:stage_=Stage::DispatchBalancedRate;break;case Stage::DispatchBalancedRate:stage_=Stage::AwaitBalancedRate;break;
  case Stage::ReleaseCoefficient:packet(coefficient_>>8,0x8e);stage_=relative_?Stage::OffsetBalancedSecond:Stage::DispatchBalancedSecond;break;
  case Stage::OffsetBalancedSecond:stage_=Stage::DispatchBalancedSecond;break;case Stage::DispatchBalancedSecond:stage_=Stage::AwaitBalancedSecond;break;
  case Stage::ReadPrevious:previous_=uint8_t(read(Read::Previous));stage_=Stage::BiasPrevious;break;case Stage::BiasPrevious:previous_=uint8_t(previous_-60);stage_=Stage::ClearPreviousLow;break;
  case Stage::ClearPreviousLow:stage_=Stage::CapturePrevious;break;case Stage::CapturePrevious:stage_=Stage::ClearPreviousMiddle;break;
  case Stage::ClearPreviousMiddle:packet(uint32_t(previous_)<<16,0x8f);stage_=relative_?Stage::OffsetPrevious:Stage::DispatchPrevious;break;
  case Stage::OffsetPrevious:stage_=Stage::DispatchPrevious;break;case Stage::DispatchPrevious:stage_=Stage::AwaitPrevious;break;
  case Stage::ReadGateAllocation:gateAllocation_=uint8_t(read(Read::Allocation));stage_=Stage::CompareGate;break;case Stage::CompareGate:stage_=Stage::AdmitGate;break;case Stage::AdmitGate:stage_=Stage::Complete;break;
  default:break;}
  used+=cost;
 }return used;}
private:
 void packet(uint32_t word,uint16_t offset){dspWord_=word;offset_=offset;}
 void scaledRate(){packet(packed_>>8,0x8d);stage_=relative_?Stage::OffsetScaledRate:Stage::DispatchScaledRate;}
 bool relative_;Stage stage_=Stage::ReadPatchPitch;uint8_t slot_=0,pitch_=0,detune_=0,magnitude_=0,transposed_=0,previous_=0,time_=0,mode_=0,allocation_=0,inverted_=0,adjustment_=0,distanceMode_=0,distance_=0,shifts_=0,gateAllocation_=0;
 uint16_t rate_=0,base_=0,offset_=0;uint32_t coefficient_=0,highProduct_=0,lowProduct_=0,packed_=0,balanced_=0,dspWord_=0;
};
}
