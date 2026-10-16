#pragma once
#include <cstdint>
namespace ks {
class ParameterVolumeTask final {
public:
 enum class Stage {ReadAmount,ReadLevel,CaptureLevel,MultiplyVolume,DoubleVolume,SetUnity,SubtractVolume,DispatchVolume,RestoreLevel,InvertLevel,SetShapeScale,MultiplyShape,CaptureShape,ClearMiddle,DispatchShape,AwaitVolume,AwaitShape,Complete};Stage stage()const{return phase_;}uint32_t dspWord()const{return converted_>>8;}
 bool awaitingWord()const{return phase_==Stage::AwaitVolume||phase_==Stage::AwaitShape;}bool complete()const{return phase_==Stage::Complete;}
 bool resumeWord(){if(!awaitingWord())return false;phase_=phase_==Stage::AwaitVolume?Stage::RestoreLevel:Stage::Complete;return true;}
 unsigned nextClocks()const{switch(phase_){case Stage::ReadAmount:case Stage::ReadLevel:return 15;case Stage::MultiplyVolume:case Stage::MultiplyShape:return 15;case Stage::SetUnity:return 9;case Stage::DispatchVolume:case Stage::DispatchShape:return 20;case Stage::AwaitVolume:case Stage::AwaitShape:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){case Stage::ReadAmount:amount_=uint8_t(read(false));next();break;case Stage::ReadLevel:level_=uint8_t(read(true));next();break;
 case Stage::MultiplyVolume:word_=uint16_t(unsigned(amount_)*level_);next();break;case Stage::DoubleVolume:word_=uint16_t(unsigned(word_)*2);next();break;
 case Stage::SetUnity:converted_=0x7fff0000;next();break;case Stage::SubtractVolume:converted_=uint32_t(uint16_t(0x7fff-word_))<<16;next();break;
 case Stage::DispatchVolume:phase_=Stage::AwaitVolume;break;case Stage::InvertLevel:level_^=127;next();break;case Stage::MultiplyShape:word_=uint16_t(unsigned(level_)*181);next();break;
 case Stage::CaptureShape:converted_=(uint32_t(word_)<<16)|word_;next();break;case Stage::ClearMiddle:converted_&=0xffff00ff;next();break;case Stage::DispatchShape:phase_=Stage::AwaitShape;break;default:next();break;
 }used+=cost;}return used;}
private:
 void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_=Stage::ReadAmount;uint8_t amount_=0,level_=0;uint16_t word_=0;uint32_t converted_=0;
};
}
