#pragma once
#include <cstdint>
namespace ks {
// Native panel-voice tracking and the two associated effect-envelope updates.
// Patch domain and table/parameter observations are bindings, not CPU pointers.
class VoiceTrackingTask final {
public:
 enum class Kind {Observe,Auxiliary,Refresh};
 enum class Stage {CheckPatch,AdmitPatch,ReadPart,ReadActive,ComparePart,AdmitPart,
  ReadVoice,ReadTracked,CompareVoice,AdmitVoice,Invalidate,PublishInvalid,
  SetAbsoluteOffset,ReadSendVoice,PublishVoice,ReadAuxiliary,ClearAbsoluteHigh,DispatchAbsolute,AwaitAbsolute,
  ReadRefreshTracked,AdmitTracked,PublishIndex,DispatchSelect,AwaitSelect,ReadPatch,
  ReadSample,AdmitSample,ZeroFirstWord,SetFirstOffset,DispatchFirst,AwaitFirst,
  DecrementSample,ShiftSampleOne,ShiftSampleTwo,NarrowSampleWord,NarrowSampleAddress,LocateTable,ReadTable,JumpSecond,
  ReadTime,SetTimeLow,RetainTime,HalfTime,RateMultiplier,MultiplyRate,CaptureRate,BiasRate,ReadBase,BiasBase,AddBase,
  DispatchCoefficient,AwaitCoefficient,RestoreTime,CompareScale,AdmitScale,CapScale,ShiftScaleOne,ShiftScaleTwo,
  CaptureCoefficientHigh,MultiplyHigh,MultiplyLow,CaptureLowHigh,ClearLowHigh,AddProducts,DoublePacked,
  ZeroSecondWord,SetSecondOffset,DispatchSecond,AwaitSecond,Complete};
 enum class Read {Part,ActivePart,Voice,Tracked,Auxiliary,PatchReference,Sample,Time,Base,Table};
 enum class Write {Tracked,Index};
 enum class Child {None,Absolute,Select,Coefficient,First,Second};
 explicit VoiceTrackingTask(Kind kind,bool editable=true):kind_(kind),stage_(kind==Kind::Observe?Stage::CheckPatch:kind==Kind::Auxiliary?Stage::SetAbsoluteOffset:Stage::ReadPart),editable_(editable){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}unsigned effect()const{return effect_;}
 bool sent()const{return sent_;}bool editable()const{return editable_;}uint8_t part()const{return part_;}uint8_t activePart()const{return activePart_;}
 uint8_t voice()const{return voice_;}uint8_t tracked()const{return tracked_;}uint8_t auxiliary()const{return auxiliary_;}
 uint8_t sample()const{return sample_;}uint8_t sampleIndex()const{return sampleIndex_;}uint8_t time()const{return time_;}uint8_t scale()const{return scale_;}
 uint16_t rate()const{return rate_;}uint16_t base()const{return base_;}uint32_t coefficient()const{return coefficient_;}
 uint32_t highProduct()const{return highProduct_;}uint32_t lowProduct()const{return lowProduct_;}uint32_t packed()const{return packed_;}
 uint16_t firstOffset()const{return uint16_t(0x7e + effect_*8);}uint16_t secondOffset()const{return uint16_t(firstOffset()+1);}
 uint16_t absoluteOffset()const{return kind_==Kind::Auxiliary?0x8045:0x8046;}
 uint8_t absoluteValue()const{return kind_==Kind::Auxiliary?auxiliary_:voice_;}
 bool complete()const{return stage_==Stage::Complete;}
 Child child()const{switch(stage_){case Stage::AwaitAbsolute:return Child::Absolute;case Stage::AwaitSelect:return Child::Select;
 case Stage::AwaitCoefficient:return Child::Coefficient;case Stage::AwaitFirst:return Child::First;case Stage::AwaitSecond:return Child::Second;default:return Child::None;}}
 bool resumeChild(uint32_t result=0){switch(stage_){case Stage::AwaitAbsolute:sent_=true;stage_=Stage::Complete;return true;
 case Stage::AwaitSelect:stage_=Stage::ReadPatch;return true;case Stage::AwaitCoefficient:coefficient_=result;stage_=Stage::RestoreTime;return true;
 case Stage::AwaitFirst:stage_=sample_?Stage::DecrementSample:Stage::ZeroSecondWord;return true;
 case Stage::AwaitSecond:if(++effect_==2)stage_=Stage::Complete;else stage_=Stage::ReadSample;return true;default:return false;}}
 bool publication()const{return stage_==Stage::PublishInvalid||stage_==Stage::PublishVoice||stage_==Stage::PublishIndex;}
 Write publicationKind()const{return stage_==Stage::PublishIndex?Write::Index:Write::Tracked;}
 unsigned nextClocks(unsigned storeClocks=3)const{switch(stage_){case Stage::CheckPatch:case Stage::LocateTable:return 9;
 case Stage::ReadPart:case Stage::ReadActive:case Stage::ReadVoice:case Stage::ReadTracked:case Stage::ReadSendVoice:case Stage::ReadRefreshTracked:case Stage::ReadBase:case Stage::ReadTable:return 12;
 case Stage::PublishInvalid:case Stage::PublishVoice:case Stage::PublishIndex:return 9+storeClocks;
 case Stage::SetAbsoluteOffset:case Stage::SetFirstOffset:case Stage::SetSecondOffset:case Stage::RateMultiplier:case Stage::BiasRate:case Stage::BiasBase:return 6;
 case Stage::DispatchAbsolute:case Stage::DispatchSelect:case Stage::DispatchFirst:case Stage::DispatchSecond:case Stage::DispatchCoefficient:return 20;
 case Stage::AdmitPart:return kind_==Kind::Refresh?8:3;case Stage::AdmitTracked:return 8;
 case Stage::ReadPatch: return 18;case Stage::ReadSample:case Stage::ReadTime:return 15;
 case Stage::RetainTime:case Stage::RestoreTime:return 11;
 case Stage::MultiplyRate:case Stage::MultiplyHigh:case Stage::MultiplyLow:return 23;
 case Stage::AwaitAbsolute:case Stage::AwaitSelect:case Stage::AwaitFirst:case Stage::AwaitSecond:case Stage::AwaitCoefficient:case Stage::Complete:return 0;
 default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){
 unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationKind()):3u)){
  if(cost>available-used)break;
  switch(stage_){case Stage::CheckPatch:stage_=Stage::AdmitPatch;break;case Stage::AdmitPatch:stage_=editable_?Stage::ReadPart:Stage::Invalidate;break;
  case Stage::ReadPart:part_=uint8_t(read(Read::Part));stage_=Stage::ReadActive;break;case Stage::ReadActive:activePart_=uint8_t(read(Read::ActivePart));stage_=Stage::ComparePart;break;
  case Stage::ComparePart:stage_=Stage::AdmitPart;break;case Stage::AdmitPart:stage_=part_==activePart_?(kind_==Kind::Refresh?Stage::ReadRefreshTracked:Stage::SetAbsoluteOffset):(kind_==Kind::Refresh?Stage::Complete:Stage::ReadVoice);break;
  case Stage::ReadVoice:voice_=uint8_t(read(Read::Voice));stage_=Stage::ReadTracked;break;case Stage::ReadTracked:tracked_=uint8_t(read(Read::Tracked));stage_=Stage::CompareVoice;break;
  case Stage::CompareVoice:stage_=Stage::AdmitVoice;break;case Stage::AdmitVoice:stage_=voice_==tracked_?Stage::Invalidate:Stage::Complete;break;
  case Stage::Invalidate:voice_=0xff;stage_=Stage::PublishInvalid;break;case Stage::PublishInvalid:write(Write::Tracked,voice_);stage_=Stage::Complete;break;
  case Stage::SetAbsoluteOffset:stage_=kind_==Kind::Auxiliary?Stage::ReadAuxiliary:Stage::ReadSendVoice;break;
  case Stage::ReadSendVoice:voice_=uint8_t(read(Read::Voice));stage_=Stage::PublishVoice;break;case Stage::PublishVoice:write(Write::Tracked,voice_);stage_=Stage::ClearAbsoluteHigh;break;
  case Stage::ReadAuxiliary:auxiliary_=uint8_t(read(Read::Auxiliary));stage_=Stage::ClearAbsoluteHigh;break;case Stage::ClearAbsoluteHigh:stage_=Stage::DispatchAbsolute;break;
  case Stage::DispatchAbsolute:stage_=Stage::AwaitAbsolute;break;
  case Stage::ReadRefreshTracked:tracked_=uint8_t(read(Read::Tracked));stage_=Stage::AdmitTracked;break;case Stage::AdmitTracked:stage_=tracked_&0x80?Stage::Complete:Stage::PublishIndex;break;
  case Stage::PublishIndex:write(Write::Index,tracked_);stage_=Stage::DispatchSelect;break;case Stage::DispatchSelect:stage_=Stage::AwaitSelect;break;
  case Stage::ReadPatch:read(Read::PatchReference);stage_=Stage::ReadSample;break;
  case Stage::ReadSample:sample_=uint8_t(read(Read::Sample));stage_=Stage::AdmitSample;break;case Stage::AdmitSample:stage_=sample_?Stage::ZeroFirstWord:Stage::ReadTime;break;
  case Stage::ZeroFirstWord:packed_=0;stage_=Stage::SetFirstOffset;break;case Stage::SetFirstOffset:stage_=Stage::DispatchFirst;break;case Stage::DispatchFirst:stage_=Stage::AwaitFirst;break;
  case Stage::DecrementSample:sampleIndex_=uint8_t(sample_-1);stage_=Stage::ShiftSampleOne;break;case Stage::ShiftSampleOne:sampleIndex_=uint8_t(sampleIndex_<<1);stage_=Stage::ShiftSampleTwo;break;
  case Stage::ShiftSampleTwo:sampleIndex_=uint8_t(sampleIndex_<<1);stage_=Stage::NarrowSampleWord;break;case Stage::NarrowSampleWord:stage_=Stage::NarrowSampleAddress;break;
  case Stage::NarrowSampleAddress:stage_=Stage::LocateTable;break;case Stage::LocateTable:stage_=Stage::ReadTable;break;case Stage::ReadTable:packed_=uint32_t(read(Read::Table));stage_=Stage::JumpSecond;break;
  case Stage::JumpSecond:stage_=Stage::SetSecondOffset;break;
  case Stage::ReadTime:time_=uint8_t(read(Read::Time));scale_=time_;stage_=Stage::SetTimeLow;break;case Stage::SetTimeLow:stage_=Stage::RetainTime;break;
  case Stage::RetainTime:stage_=Stage::HalfTime;break;case Stage::HalfTime:rate_=uint16_t(time_)<<7;stage_=Stage::RateMultiplier;break;
  case Stage::RateMultiplier:stage_=Stage::MultiplyRate;break;case Stage::MultiplyRate:rate_=uint16_t((uint32_t(rate_)*0xaaab)>>16);stage_=Stage::CaptureRate;break;
  case Stage::CaptureRate:stage_=Stage::BiasRate;break;case Stage::BiasRate:rate_=uint16_t(rate_+0x4700);stage_=Stage::ReadBase;break;
  case Stage::ReadBase:base_=uint16_t(read(Read::Base));stage_=Stage::BiasBase;break;case Stage::BiasBase:base_=uint16_t(base_-0x4470);stage_=Stage::AddBase;break;
  case Stage::AddBase:rate_=uint16_t(rate_+base_);stage_=Stage::DispatchCoefficient;break;case Stage::DispatchCoefficient:stage_=Stage::AwaitCoefficient;break;
  case Stage::RestoreTime:stage_=Stage::CompareScale;break;case Stage::CompareScale:stage_=Stage::AdmitScale;break;
  case Stage::AdmitScale:stage_=scale_>32?Stage::CapScale:Stage::ShiftScaleOne;break;case Stage::CapScale:scale_=32;stage_=Stage::ShiftScaleOne;break;
  case Stage::ShiftScaleOne:scale_=uint8_t(scale_<<1);stage_=Stage::ShiftScaleTwo;break;case Stage::ShiftScaleTwo:scale_=uint8_t(scale_<<1);stage_=Stage::CaptureCoefficientHigh;break;
  case Stage::CaptureCoefficientHigh:stage_=Stage::MultiplyHigh;break;case Stage::MultiplyHigh:highProduct_=uint16_t(coefficient_>>16)*uint32_t(uint16_t(scale_)<<8);stage_=Stage::MultiplyLow;break;
  case Stage::MultiplyLow:lowProduct_=uint16_t(coefficient_)*uint32_t(uint16_t(scale_)<<8);stage_=Stage::CaptureLowHigh;break;
  case Stage::CaptureLowHigh:stage_=Stage::ClearLowHigh;break;case Stage::ClearLowHigh:stage_=Stage::AddProducts;break;
  case Stage::AddProducts:packed_=highProduct_+(lowProduct_>>16);stage_=Stage::DoublePacked;break;case Stage::DoublePacked:packed_<<=1;stage_=Stage::SetFirstOffset;break;
  case Stage::ZeroSecondWord:packed_=0;stage_=Stage::SetSecondOffset;break;case Stage::SetSecondOffset:stage_=Stage::DispatchSecond;break;
  case Stage::DispatchSecond:stage_=Stage::AwaitSecond;break;default:break;}
  used+=cost;
 }return used;}
private:
 Kind kind_;Stage stage_;bool editable_,sent_=false;unsigned effect_=0;
 uint8_t part_=0,activePart_=0,voice_=0,tracked_=0,auxiliary_=0,sample_=0,sampleIndex_=0,time_=0,scale_=0;
 uint16_t rate_=0,base_=0;uint32_t coefficient_=0,highProduct_=0,lowProduct_=0,packed_=0;
};
}
