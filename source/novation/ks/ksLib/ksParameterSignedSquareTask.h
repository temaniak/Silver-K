#pragma once
#include <cstdint>
namespace ks {
class ParameterSignedSquareTask final {
public:
 enum class Stage {Read,Bias,CaptureSign,Square,AdmitValue,AdmitSign,Negate,Scale0,Scale1,Scale2,CaptureWord,ClearMiddle,Dispatch,Await,Complete};Stage stage()const{return phase_;}uint32_t dspWord()const{return uint32_t(word_)<<8;}bool awaitingWord()const{return phase_==Stage::Await;}bool complete()const{return phase_==Stage::Complete;}
 bool resumeWord(){if(!awaitingWord())return false;phase_=Stage::Complete;return true;}
 unsigned nextClocks()const{switch(phase_){case Stage::Read:return 15;case Stage::Square:return 18;case Stage::Dispatch:return 20;case Stage::Await:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){case Stage::Read:value_=uint8_t(read());next();break;case Stage::Bias:value_=uint8_t(value_-64);next();break;
 case Stage::Square:{const int v=value_<128?int(value_):int(value_)-256;word_=uint16_t(v*v);next();break;}
 case Stage::AdmitSign:phase_=value_&128?Stage::Negate:Stage::Scale0;break;case Stage::Negate:word_=uint16_t(0-word_);next();break;
 case Stage::Scale0:case Stage::Scale1:case Stage::Scale2:word_=uint16_t(unsigned(word_)*2);next();break;case Stage::Dispatch:phase_=Stage::Await;break;default:next();break;
 }used+=cost;}return used;}
private:void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_=Stage::Read;uint8_t value_=0;uint16_t word_=0;
};
}
