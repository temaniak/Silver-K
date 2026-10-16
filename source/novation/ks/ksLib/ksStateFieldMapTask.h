#pragma once
#include <array>
#include <cstdint>
namespace ks {
// Four descriptor-based packed controls. Outcomes distinguish exact-limit
// passthrough from above-limit suppression, preserving original edit semantics.
class StateFieldMapTask final {
public:
 enum class Stage {CompareFirst,AdmitFirst,CompareLast,AdmitLast,MarkUnmapped,ReturnUnmapped,ClearDescriptorIndex,CaptureParameter,BiasDescriptorIndex,ScaleDescriptorIndex,ReadDescriptor,ClearInput,CaptureInput,ReadRightCount,AdmitRightCount,ShiftInput,DecrementRight,AdmitMoreRight,ReadLimit,CompareLimit,AdmitLimit,MarkLimit,ReturnLimit,ReadMask,MaskValue,InvertMask,ReadSpare,MakePairIndex,AddPairIndex,ReadLeftCount,AdmitLeftCount,ShiftValue,RotateMask,DecrementLeft,AdmitMoreLeft,ClearField,ReadField,ReadBase,AddFieldBase,ReadCurrent,MaskCurrent,MergeCurrent,MarkMapped,SetMapped,ReturnMapped,Complete};enum class Read {Descriptor,RightCount,Limit,Mask,Spare,LeftCount,Field,Base,Current};
 enum class Outcome {Unmapped,EqualLimit,AboveLimit,Packed};
 StateFieldMapTask(uint8_t parameter,uint8_t value,uint32_t fieldContext):parameter_(parameter),value_(value),field_(fieldContext){}
 Stage stage()const{return phase_;}bool complete()const{return phase_==Stage::Complete;}Outcome outcome()const{return outcome_;}
 uint8_t parameter()const{return parameter_;}uint8_t value()const{return value_;}uint32_t field()const{return field_;}uint8_t preserveMask()const{return mask_;}
 unsigned descriptorNumber()const{return unsigned(parameter_-104);}unsigned descriptorOffset()const{return offset_;}
 unsigned nextClocks()const{constexpr std::array<unsigned,45> c{{3,3,3,3,3,17,3,3,3,3,21,3,3,8,3,3,3,3,8,3,3,3,17,8,3,3,8,3,3,8,3,3,3,3,3,3,6,18,3,6,3,3,3,3,17}};return unsigned(phase_)<c.size()?c[unsigned(phase_)]:0;}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){case Stage::AdmitFirst:phase_=parameter_<104?Stage::MarkUnmapped:Stage::CompareLast;break;case Stage::AdmitLast:phase_=parameter_<=107?Stage::ClearDescriptorIndex:Stage::MarkUnmapped;break;
 case Stage::MarkUnmapped:outcome_=Outcome::Unmapped;next();break;case Stage::ReadDescriptor:(void)read(Read::Descriptor);offset_=0;next();break;case Stage::CaptureInput:shifted_=value_;next();break;
 case Stage::ReadRightCount:right_=uint8_t(read(Read::RightCount));++offset_;next();break;case Stage::AdmitRightCount:phase_=right_?Stage::ShiftInput:Stage::ReadLimit;break;case Stage::ShiftInput:shifted_>>=1;next();break;case Stage::DecrementRight:--right_;next();break;case Stage::AdmitMoreRight:phase_=right_?Stage::ShiftInput:Stage::ReadLimit;break;
 case Stage::ReadLimit:limit_=uint8_t(read(Read::Limit));++offset_;field_=(field_&0xffffff00)|limit_;next();break;
 case Stage::AdmitLimit:if(shifted_<limit_)phase_=Stage::ReadMask;else{outcome_=shifted_==limit_?Outcome::EqualLimit:Outcome::AboveLimit;phase_=Stage::MarkLimit;}break;
 case Stage::ReadMask:mask_=uint8_t(read(Read::Mask));++offset_;next();break;case Stage::MaskValue:value_&=mask_;next();break;case Stage::InvertMask:mask_=uint8_t(~mask_);next();break;
 case Stage::ReadSpare:(void)read(Read::Spare);++offset_;next();break;case Stage::MakePairIndex:shifted_&=254;next();break;case Stage::AddPairIndex:offset_+=shifted_;next();break;
 case Stage::ReadLeftCount:left_=uint8_t(read(Read::LeftCount));++offset_;next();break;case Stage::AdmitLeftCount:phase_=left_?Stage::ShiftValue:Stage::ClearField;break;case Stage::ShiftValue:value_=uint8_t(value_*2);next();break;case Stage::RotateMask:mask_=uint8_t((mask_<<1)|(mask_>>7));next();break;case Stage::DecrementLeft:--left_;next();break;case Stage::AdmitMoreLeft:phase_=left_?Stage::ShiftValue:Stage::ClearField;break;
 case Stage::ClearField:field_=0;next();break;case Stage::ReadField:field_=uint8_t(read(Read::Field));next();break;case Stage::ReadBase:(void)read(Read::Base);next();break;
 case Stage::ReadCurrent:current_=uint8_t(read(Read::Current));next();break;case Stage::MaskCurrent:current_&=mask_;next();break;case Stage::MergeCurrent:value_|=current_;next();break;case Stage::SetMapped:outcome_=Outcome::Packed;next();break;
 case Stage::ReturnUnmapped:case Stage::ReturnLimit:case Stage::ReturnMapped:phase_=Stage::Complete;break;default:next();break;
 }used+=cost;}return used;}
private:
 void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_=Stage::CompareFirst;Outcome outcome_=Outcome::Unmapped;uint8_t parameter_,value_,shifted_=0,right_=0,left_=0,limit_=0,mask_=0,current_=0;unsigned offset_=0;uint32_t field_;
};
}
