#pragma once
#include <cstdint>
namespace ks {
// Shared patch-to-packet conversions. Scalar arithmetic and endpoint choices
// use domain values only; the caller supplies one late patch observation.
class ParameterScalarTask final {
public:
 enum class Kind {Wave,Bipolar,Endpoint,DoubleEndpoint,Square,Quarter,BipolarHalf,ByteHigh};
 enum class Stage {Read,Bias,ClearLow,SetFactor,Multiply,DoubleOutput,WaveOffset,CompareEndpoint,AdmitEndpoint,FillEndpoint,DoubleWord,CaptureWord,CaptureLow,DifferenceBytes,CaptureValue,DoubleValue,ClearMiddle,Right0,Right1,ArithmeticHalf,Dispatch,Await,Complete};
 explicit ParameterScalarTask(Kind kind):kind_(kind){}
 Kind kind()const{return kind_;}Stage stage()const{return phase_;}bool complete()const{return phase_==Stage::Complete;}bool awaitingWord()const{return phase_==Stage::Await;}
 uint32_t dspWord()const{return converted_>>8;}bool resumeWord(){if(!awaitingWord())return false;phase_=Stage::Complete;return true;}
 void pendingWord(uint32_t word){converted_=(word&0xffffff)<<8;phase_=Stage::Dispatch;}
 unsigned nextClocks()const{switch(phase_){case Stage::Read:return 15;case Stage::SetFactor:return 6;
 case Stage::Multiply:return kind_==Kind::Bipolar?26:23;case Stage::WaveOffset:return 9;case Stage::Dispatch:return 20;
 case Stage::Await:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){
 case Stage::Read:value_=uint8_t(read());phase_=kind_==Kind::Square?Stage::CaptureValue:(kind_==Kind::Quarter||kind_==Kind::Wave||kind_==Kind::ByteHigh)?Stage::ClearLow:Stage::Bias;break;
 case Stage::Bias:value_=uint8_t(value_-64);phase_=Stage::ClearLow;break;
 case Stage::ClearLow:word_=uint16_t(value_)<<8;phase_=kind_==Kind::Wave||kind_==Kind::Bipolar?Stage::SetFactor:kind_==Kind::Endpoint||kind_==Kind::DoubleEndpoint?Stage::CompareEndpoint:kind_==Kind::Quarter?Stage::Right0:kind_==Kind::BipolarHalf?Stage::ArithmeticHalf:Stage::CaptureWord;break;
 case Stage::SetFactor:phase_=Stage::Multiply;break;
 case Stage::Multiply:converted_=kind_==Kind::Bipolar?uint32_t(sign16(word_)*21845):kind_==Kind::Wave?uint32_t(word_)*9:uint32_t(word_)*word_;phase_=kind_==Kind::Wave?Stage::WaveOffset:Stage::DoubleOutput;break;
 case Stage::DoubleOutput:converted_<<=1;phase_=Stage::Dispatch;break;
 case Stage::WaveOffset:converted_+=0x40000;phase_=Stage::Dispatch;break;
 case Stage::CompareEndpoint:phase_=Stage::AdmitEndpoint;break;
 case Stage::AdmitEndpoint:phase_=value_==63?Stage::FillEndpoint:kind_==Kind::DoubleEndpoint?Stage::DoubleWord:Stage::CaptureWord;break;
 case Stage::FillEndpoint:word_|=255;phase_=kind_==Kind::DoubleEndpoint?Stage::DoubleWord:Stage::CaptureWord;break;
 case Stage::DoubleWord:word_=uint16_t(unsigned(word_)*2);phase_=Stage::CaptureWord;break;
 case Stage::CaptureWord:converted_=(uint32_t(word_)<<16)|word_;phase_=kind_==Kind::Endpoint?Stage::CaptureLow:kind_==Kind::DoubleEndpoint?Stage::DifferenceBytes:Stage::ClearMiddle;break;
 case Stage::CaptureLow:converted_=(converted_&0xffff00ff)|(uint32_t(uint8_t(word_))<<8);phase_=Stage::Dispatch;break;
 case Stage::DifferenceBytes:converted_=(converted_&0xffff00ff)|(uint32_t(uint8_t((word_>>8)-uint8_t(word_)))<<8);phase_=Stage::Dispatch;break;
 case Stage::CaptureValue:word_=uint16_t((uint16_t(value_)<<8)|value_);phase_=Stage::DoubleValue;break;
 case Stage::DoubleValue:word_=uint16_t((word_&0xff00)|uint8_t(value_*2));phase_=Stage::Multiply;break;
 case Stage::ClearMiddle:converted_&=0xffff00ff;phase_=Stage::Dispatch;break;
 case Stage::Right0:word_>>=1;phase_=Stage::Right1;break;case Stage::Right1:word_>>=1;phase_=Stage::CaptureWord;break;
 case Stage::ArithmeticHalf:word_=uint16_t((word_>>1)|(word_&0x8000));phase_=Stage::CaptureWord;break;
 case Stage::Dispatch:phase_=Stage::Await;break;default:break;
 }used+=cost;}return used;}
private:
 static int sign16(uint16_t v){return v<32768?int(v):int(v)-65536;}
 Kind kind_;Stage phase_=Stage::Read;uint8_t value_=0;uint16_t word_=0;uint32_t converted_=0;
};
}
