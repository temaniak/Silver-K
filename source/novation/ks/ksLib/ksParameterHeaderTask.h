#pragma once
#include <cstdint>
namespace ks {
class ParameterHeaderTask final {
public:
 enum class Kind {Header,Part,Byte};enum class Stage {ReadPart,ReadBase,SubtractPart,AdmitNegative,ClampNegative,CapturePart,DoublePart,AddPart,OffsetPart,ClearHigh,DispatchWord,ReadMode,RotateMode,MaskMode,InvertMode,CaptureMode,DoubleMode,AddMode,OffsetMode,ClearModeHigh,DispatchModeWord,ReadByte,AwaitWord,AwaitModeWord,Complete};
 explicit ParameterHeaderTask(Kind kind):kind_(kind),phase_(kind==Kind::Byte?Stage::ReadByte:Stage::ReadPart){}
 Kind kind()const{return kind_;}Stage stage()const{return phase_;}uint32_t dspWord()const{return output_;}bool complete()const{return phase_==Stage::Complete;}
 bool awaitingWord()const{return phase_==Stage::AwaitWord||phase_==Stage::AwaitModeWord;}
 bool resumeWord(){if(!awaitingWord())return false;phase_=phase_==Stage::AwaitWord&&kind_==Kind::Header?Stage::ReadMode:Stage::Complete;return true;}
 unsigned nextClocks()const{switch(phase_){case Stage::ReadPart:case Stage::ReadBase:return 12;case Stage::ReadMode:case Stage::ReadByte:return 15;case Stage::DispatchWord:case Stage::DispatchModeWord:return 20;case Stage::AwaitWord:case Stage::AwaitModeWord:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){case Stage::ReadPart:part_=uint8_t(read(0));next();break;case Stage::ReadBase:base_=uint8_t(read(1));next();break;
 case Stage::SubtractPart:output_=uint8_t(part_-base_);next();break;case Stage::AdmitNegative:phase_=output_&128?Stage::ClampNegative:kind_==Kind::Header?Stage::CapturePart:Stage::ClearHigh;break;
 case Stage::ClampNegative:output_=0;phase_=kind_==Kind::Header?Stage::CapturePart:Stage::ClearHigh;break;
 case Stage::CapturePart:copy_=uint8_t(output_);next();break;case Stage::DoublePart:copy_=uint8_t(copy_*2);next();break;case Stage::AddPart:output_=uint8_t(output_+copy_);next();break;case Stage::OffsetPart:output_=uint8_t(output_+0x67);next();break;
 case Stage::DispatchWord:phase_=Stage::AwaitWord;break;case Stage::ReadMode:mode_=uint8_t(read(2));next();break;case Stage::RotateMode:mode_=uint8_t((mode_<<2)|(mode_>>6));next();break;case Stage::MaskMode:mode_&=1;next();break;case Stage::InvertMode:mode_^=1;next();break;
 case Stage::CaptureMode:copy_=mode_;next();break;case Stage::DoubleMode:copy_=uint8_t(copy_*2);next();break;case Stage::AddMode:mode_=uint8_t(mode_+copy_);next();break;case Stage::OffsetMode:output_=uint8_t(mode_+3);next();break;
 case Stage::DispatchModeWord:phase_=Stage::AwaitModeWord;break;case Stage::ReadByte:output_=uint8_t(read(3));phase_=Stage::ClearHigh;break;default:next();break;
 }used+=cost;}return used;}
private:
 void next(){phase_=Stage(unsigned(phase_)+1);}Kind kind_;Stage phase_;uint8_t part_=0,base_=0,copy_=0,mode_=0;uint32_t output_=0;
};
}
