#pragma once
#include <cstdint>
namespace ks {
// Analog panel controls: original min/max history, offset/gain derivation,
// wheel dead-zone/clipping, square curve and output register values.
class PanelAnalogTask final {
public:
 enum class Kind { Initialize, Duty, Level, Range, Coefficients, Convert, Scale, Square };enum class Stage { BindControlMode, WriteControlMode, ClearControlCount, WriteControlCount, BindInitialDuty, WriteInitialDuty, ReturnInitialize, WriteDuty, ReturnDuty, BindLevelCenter, SubtractLevel, DoubleLevel, BiasLevel, WriteLevel, ReturnLevel, ReadMinimum, CompareMinimum, SelectMinimum, WriteMinimum, ReadMaximum, CompareMaximum, SelectMaximum, WriteMaximum, ReturnRange, ReadCoefficientMaximum, ReadCoefficientMinimum, ComputeSpan, RetainSpan, HalfSpan, ComputeCenter, BindCenter, ComputeOffset, WriteOffset, RestoreSpan, ComputeFactor, WriteFactor, ReturnCoefficients, CompareFirstWheel, SelectFirstWheel, CompareSecondWheel, SelectSecondWheel, CompareSquare, SelectSquare, ReturnUnchanged, SaveFirstAddress, SaveFirstBias, SetFirstBias, SelectFirstCoefficients, ScaleFirst, RestoreFirstBias, RestoreFirstAddress, ReturnFirst, SaveSecondAddress, SaveSecondBias, SetSecondBias, SelectSecondCoefficients, ReverseSecond, ScaleSecond, HalfSecond, RestoreSecondBias, RestoreSecondAddress, ReturnSecond, SaveValue, ReadFactor, ClearFactorHigh, RetainFactor, ReadOffset, AddInput, CenterInput, SignInput, MultiplyInput, SelectSign, SubtractBias, SelectPositiveFloor, ClearPositive, BindPositiveLimit, ComparePositiveLimit, SelectPositiveLimit, ClampPositive, FinishPositive, AddBias, SelectNegativeCeiling, ClearNegative, BindNegativeLimit, CompareNegativeLimit, SelectNegativeLimit, ClampNegative, DoubleResult, BiasResult, PublishResult, RestoreValue, ReturnValue, SaveSquare, CopySquare, ComputeSquare, HalfSquare, PublishSquare, RestoreSquare, ReturnSquare, AwaitChild, Complete };
 enum class Read { Minimum, Maximum, Factor, Offset };enum class Write { ControlMode, ControlCount, Duty, Minimum, Maximum, Offset, Factor };
 PanelAnalogTask(Kind kind,uint8_t input,uint8_t selector,uint16_t bias):input_(input),selector_(selector),bias_(bias){switch(kind){case Kind::Initialize:stage_=Stage::BindControlMode;break;case Kind::Duty:stage_=Stage::WriteDuty;break;case Kind::Level:stage_=Stage::BindLevelCenter;break;case Kind::Range:stage_=Stage::ReadMinimum;break;case Kind::Coefficients:stage_=Stage::ReadCoefficientMaximum;break;case Kind::Convert:stage_=Stage::CompareFirstWheel;break;case Kind::Scale:stage_=Stage::SaveValue;break;case Kind::Square:stage_=Stage::SaveSquare;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint8_t input){if(!awaitingChild())return false;input_=input;stage_=resume_;return true;}
 uint8_t input()const{return input_;}uint8_t minimum()const{return minimum_;}uint8_t maximum()const{return maximum_;}uint8_t factor()const{return factor_;}uint8_t offset()const{return offset_;}uint8_t level()const{return level_;}
 unsigned nextClocks(unsigned memory=3)const{switch(stage_){case Stage::WriteControlMode:case Stage::WriteControlCount:case Stage::WriteInitialDuty:case Stage::WriteDuty:case Stage::WriteLevel:return 4; case Stage::SetFirstBias:case Stage::SetSecondBias:case Stage::BindPositiveLimit:case Stage::BindNegativeLimit:return 6; case Stage::SelectSquare:return 8; case Stage::SelectFirstCoefficients:case Stage::SelectSecondCoefficients:return 9; case Stage::RetainSpan:case Stage::RestoreSpan:case Stage::SaveFirstBias:case Stage::RestoreFirstBias:case Stage::SaveSecondBias:case Stage::RestoreSecondBias:case Stage::SaveSquare:case Stage::RestoreSquare:return 11; case Stage::ComputeFactor:case Stage::ScaleFirst:case Stage::ScaleSecond:case Stage::ComputeSquare:return 15; case Stage::ReturnInitialize:case Stage::ReturnDuty:case Stage::ReturnLevel:case Stage::ReturnRange:case Stage::ReturnCoefficients:case Stage::ReturnUnchanged:case Stage::ReturnFirst:case Stage::ReturnSecond:case Stage::ReturnValue:case Stage::ReturnSquare:return 17; case Stage::SaveFirstAddress:case Stage::RestoreFirstAddress:case Stage::SaveSecondAddress:case Stage::RestoreSecondAddress:case Stage::SaveValue:case Stage::RestoreValue:return 20; case Stage::MultiplyInput:return 26; case Stage::ReadMinimum:return 3+memory; case Stage::WriteMinimum:return 3+memory; case Stage::ReadMaximum:return 6+memory; case Stage::WriteMaximum:return 6+memory; case Stage::ReadCoefficientMaximum:return 6+memory; case Stage::ReadCoefficientMinimum:return 3+memory; case Stage::WriteOffset:return 3+memory; case Stage::WriteFactor:return 6+memory; case Stage::ReadFactor:return 6+memory; case Stage::ReadOffset:return 3+memory; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned memory=3){const auto cost=nextClocks(memory);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReturnInitialize:stage_=Stage::Complete;break;
case Stage::ReturnDuty:stage_=Stage::Complete;break;
case Stage::ReturnLevel:stage_=Stage::Complete;break;
case Stage::ReturnRange:stage_=Stage::Complete;break;
case Stage::ReturnCoefficients:stage_=Stage::Complete;break;
case Stage::ReturnUnchanged:stage_=Stage::Complete;break;
case Stage::ScaleFirst:await(Stage::RestoreFirstBias);break;
case Stage::ReturnFirst:stage_=Stage::Complete;break;
case Stage::ScaleSecond:await(Stage::HalfSecond);break;
case Stage::ReturnSecond:stage_=Stage::Complete;break;
case Stage::FinishPositive:stage_=Stage::DoubleResult;break;
case Stage::ReturnValue:stage_=Stage::Complete;break;
case Stage::ReturnSquare:stage_=Stage::Complete;break;
case Stage::WriteControlMode:write(Write::ControlMode,0xc0);next();break;
case Stage::WriteControlCount:write(Write::ControlCount,0);next();break;
case Stage::WriteInitialDuty:write(Write::Duty,0xc0);next();break;
case Stage::WriteDuty:write(Write::Duty,input_);next();break;
case Stage::BindLevelCenter:level_=64;next();break;
case Stage::SubtractLevel:level_=uint8_t(level_-input_);next();break;
case Stage::DoubleLevel:level_=uint8_t(level_<<1);next();break;
case Stage::BiasLevel:level_=uint8_t(level_+64);next();break;
case Stage::WriteLevel:write(Write::ControlCount,level_);next();break;
case Stage::ReadMinimum:minimum_=uint8_t(read(Read::Minimum));next();break;
case Stage::SelectMinimum:stage_=input_<minimum_?Stage::WriteMinimum:Stage::ReadMaximum;break;
case Stage::WriteMinimum:write(Write::Minimum,input_);next();break;
case Stage::ReadMaximum:maximum_=uint8_t(read(Read::Maximum));next();break;
case Stage::SelectMaximum:stage_=input_>maximum_?Stage::WriteMaximum:Stage::ReturnRange;break;
case Stage::WriteMaximum:write(Write::Maximum,input_);next();break;
case Stage::ReadCoefficientMaximum:maximum_=uint8_t(read(Read::Maximum));next();break;
case Stage::ReadCoefficientMinimum:minimum_=uint8_t(read(Read::Minimum));next();break;
case Stage::ComputeSpan:span_=uint8_t(maximum_-minimum_);next();break;
case Stage::HalfSpan:center_=uint8_t(span_>>1);next();break;
case Stage::ComputeCenter:center_=uint8_t(center_+minimum_);next();break;
case Stage::ComputeOffset:offset_=uint8_t(128-center_);next();break;
case Stage::WriteOffset:write(Write::Offset,offset_);next();break;
case Stage::ComputeFactor:factor_=span_?uint8_t(bias_/span_):0;next();break;
case Stage::WriteFactor:write(Write::Factor,factor_);next();break;
case Stage::SelectFirstWheel:stage_=selector_==0x22?Stage::SaveFirstAddress:Stage::CompareSecondWheel;break;
case Stage::SelectSecondWheel:stage_=selector_==0x23?Stage::SaveSecondAddress:Stage::CompareSquare;break;
case Stage::SelectSquare:stage_=selector_==0x20?Stage::Complete:Stage::ReturnUnchanged;break;
case Stage::ReverseSecond:input_=uint8_t(~input_);next();break;
case Stage::HalfSecond:input_>>=1;next();break;
case Stage::ReadFactor:factor_=uint8_t(read(Read::Factor));next();break;
case Stage::ReadOffset:offset_=uint8_t(read(Read::Offset));next();break;
case Stage::AddInput:center_=uint8_t(offset_+input_);next();break;
case Stage::CenterInput:center_=uint8_t(center_+128);next();break;
case Stage::MultiplyInput:product_=int32_t(int8_t(center_))*int32_t(factor_);word_=uint16_t(product_);next();break;
case Stage::SelectSign:stage_=product_<0?Stage::AddBias:Stage::SubtractBias;break;
case Stage::SubtractBias:word_=uint16_t(word_-bias_);next();break;
case Stage::SelectPositiveFloor:stage_=(word_&0x8000)?Stage::ClearPositive:Stage::BindPositiveLimit;break;
case Stage::ClearPositive:word_=0;next();break;
case Stage::SelectPositiveLimit:stage_=int16_t(word_)<=0x3f80?Stage::FinishPositive:Stage::ClampPositive;break;
case Stage::ClampPositive:word_=0x3f80;next();break;
case Stage::AddBias:word_=uint16_t(word_+bias_);next();break;
case Stage::SelectNegativeCeiling:stage_=(word_&0x8000)?Stage::BindNegativeLimit:Stage::ClearNegative;break;
case Stage::ClearNegative:word_=0;next();break;
case Stage::SelectNegativeLimit:stage_=int16_t(word_)>=-16384?Stage::DoubleResult:Stage::ClampNegative;break;
case Stage::ClampNegative:word_=0xc000;next();break;
case Stage::DoubleResult:word_=uint16_t(word_<<1);next();break;
case Stage::BiasResult:input_=uint8_t((word_>>8)+128);next();break;
case Stage::ComputeSquare:word_=uint16_t(input_)*input_;next();break;
case Stage::HalfSquare:input_=uint8_t((word_>>8)>>1);next();break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint8_t input_,selector_,minimum_=0,maximum_=0,span_=0,center_=0,offset_=0,factor_=0,level_=0;uint16_t bias_,word_=0;int32_t product_=0;
};
}
