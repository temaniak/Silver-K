#pragma once
#include <cstdint>
namespace ks {
class ParameterCurveTask final {
public:
 enum class Stage {SetFactor,ReadValue,Bias,AdmitSign,NegateValue,NegateFactor,DoubleValue,ClearWord,ClearIndex,ReadCurve,Multiply,DoubleOutput,Dispatch,Await,Complete};Stage stage()const{return phase_;}uint8_t index()const{return value_;}uint32_t dspWord()const{return converted_>>8;}
 bool awaitingWord()const{return phase_==Stage::Await;}bool complete()const{return phase_==Stage::Complete;}
 bool resumeWord(){if(!awaitingWord())return false;phase_=Stage::Complete;return true;}
 unsigned nextClocks()const{switch(phase_){case Stage::SetFactor:return 6;case Stage::ReadValue:case Stage::ReadCurve:return 15;case Stage::Multiply:return 26;case Stage::Dispatch:return 20;case Stage::Await:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){case Stage::SetFactor:factor_=21845;next();break;case Stage::ReadValue:value_=uint8_t(read(false));next();break;
 case Stage::Bias:value_=uint8_t(value_-64);next();break;case Stage::AdmitSign:phase_=value_&128?Stage::NegateValue:Stage::DoubleValue;break;
 case Stage::NegateValue:value_=uint8_t(0-value_);next();break;case Stage::NegateFactor:factor_=-factor_;next();break;
 case Stage::DoubleValue:value_=uint8_t(value_*2);next();break;case Stage::ReadCurve:curve_=uint16_t(read(true));next();break;
 case Stage::Multiply:converted_=uint32_t((curve_<32768?int(curve_):int(curve_)-65536)*factor_);next();break;
 case Stage::DoubleOutput:converted_<<=1;next();break;case Stage::Dispatch:phase_=Stage::Await;break;default:next();break;
 }used+=cost;}return used;}
private:
 void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_=Stage::SetFactor;uint8_t value_=0;int factor_=21845;uint16_t curve_=0;uint32_t converted_=0;
};
}
