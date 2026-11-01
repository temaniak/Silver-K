#pragma once
#include <cstdint>
namespace ks {
// Live time-to-coefficient upload. Coefficient conversion is an explicit native
// child request; optional zero/half mode retains its own observation path.
class ParameterTimeTask final {
public:
 enum class Stage {ClearOutput,ReadValue,AdmitZero,InvertValue,CaptureValue,SetMultiplier,Multiply,SetThreshold,SubtractThreshold,AdmitCorrection,ClearCorrection,AddCorrection,ReadBase,AddBase,DispatchCoefficient,ShiftCoefficient,CaptureCoefficient,DispatchWord,AwaitCoefficient,AwaitWord,Complete};
 ParameterTimeTask(uint8_t multiplier,uint8_t threshold,bool optional):multiplier_(multiplier),threshold_(threshold),optional_(optional),phase_(optional?Stage::ClearOutput:Stage::ReadValue){}
 Stage stage()const{return phase_;}bool awaitingCoefficient()const{return phase_==Stage::AwaitCoefficient;}bool awaitingWord()const{return phase_==Stage::AwaitWord;}bool complete()const{return phase_==Stage::Complete;}
 uint16_t scaleInput()const{return input_;}uint32_t dspWord()const{return coefficient_>>8;}
 bool resumeCoefficient(uint32_t value){if(!awaitingCoefficient())return false;coefficient_=value;phase_=optional_?Stage::ShiftCoefficient:Stage::CaptureCoefficient;return true;}
 bool resumeWord(){if(!awaitingWord())return false;phase_=Stage::Complete;return true;}
 unsigned nextClocks()const{switch(phase_){case Stage::ReadValue:return 15;case Stage::Multiply:return 15;case Stage::ReadBase:return 12;case Stage::DispatchCoefficient:case Stage::DispatchWord:return 20;
 case Stage::AwaitCoefficient:case Stage::AwaitWord:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){case Stage::ClearOutput:coefficient_=0;phase_=Stage::ReadValue;break;
 case Stage::ReadValue:value_=uint8_t(read(false));phase_=optional_?Stage::AdmitZero:Stage::InvertValue;break;
 case Stage::AdmitZero:phase_=value_?Stage::InvertValue:Stage::DispatchWord;break;
 case Stage::InvertValue:value_^=127;next();break;
 case Stage::Multiply:input_=uint16_t(unsigned(value_)*multiplier_);next();break;
 case Stage::SubtractThreshold:correction_=uint8_t(value_-threshold_);next();break;
 case Stage::AdmitCorrection:phase_=value_<=threshold_?Stage::ReadBase:Stage::ClearCorrection;break;
 case Stage::AddCorrection:input_=uint16_t(input_+(unsigned(correction_)<<8));next();break;
 case Stage::ReadBase:base_=uint16_t(read(true));next();break;
 case Stage::AddBase:input_=uint16_t(input_+base_);next();break;
 case Stage::DispatchCoefficient:phase_=Stage::AwaitCoefficient;break;
 case Stage::ShiftCoefficient:coefficient_>>=1;next();break;
 case Stage::DispatchWord:phase_=Stage::AwaitWord;break;
 default:next();break;
 }used+=cost;}return used;}
private:
 void next(){phase_=Stage(unsigned(phase_)+1);}uint8_t multiplier_,threshold_;bool optional_;Stage phase_;uint8_t value_=0,correction_=0;uint16_t input_=0,base_=0;uint32_t coefficient_=0;
};
}
