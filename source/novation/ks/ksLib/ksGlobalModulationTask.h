#pragma once
#include <cstdint>
namespace ks {
// Six level packets preserve B-then-A ordering and fresh observations per packet.
class GlobalModulationTask final {
public:
 enum class Stage {ReadPart,ReadBase,Difference,AdmitNegative,ClampNegative,CaptureDifference,DoubleDifference,AddDifference,ClearDifference,ReadLevel,CaptureLevel,DoubleLevel,CaptureWord,ClearMiddle,SetOffset,AddOffset,DispatchWord,AwaitWord,Return,Complete};enum class Read {Part,Base,Level};
 Stage stage()const{return phase_;}unsigned output()const{return output_;}
 bool complete()const{return phase_==Stage::Complete;}bool awaitingWord()const{return phase_==Stage::AwaitWord;}
 bool targetA()const{return output_>=3;}unsigned levelIndex()const{return output_%3;}
 uint16_t offset()const{return uint16_t(0x19+levelIndex()+slot_);}uint32_t dspWord()const{return uint32_t(levelWord_)<<8;}
 bool resumeWord(){if(!awaitingWord())return false;phase_=++output_==6?Stage::Return:Stage::ReadLevel;return true;}
 unsigned nextClocks()const{switch(phase_){case Stage::ReadPart:case Stage::ReadBase:case Stage::ReadLevel:return 12;
 case Stage::SetOffset:return 6;case Stage::DispatchWord:return 20;case Stage::Return:return 17;
 case Stage::AwaitWord:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;
  while(const auto cost=nextClocks()){if(cost>available-used)break;
   switch(phase_){case Stage::ReadPart:part_=uint8_t(read(Read::Part));next();break;
   case Stage::ReadBase:base_=uint8_t(read(Read::Base));next();break;
   case Stage::Difference:slot_=uint8_t(part_-base_);next();break;
   case Stage::AdmitNegative:phase_=slot_&128?Stage::ClampNegative:Stage::CaptureDifference;break;
   case Stage::ClampNegative:slot_=0;next();break;
   case Stage::CaptureDifference:copy_=slot_;next();break;
   case Stage::DoubleDifference:copy_=uint8_t(copy_*2);next();break;
   case Stage::AddDifference:slot_=uint8_t(slot_+copy_);next();break;
   case Stage::ReadLevel:level_=uint8_t(read(Read::Level));next();break;
   case Stage::CaptureLevel:copy_=level_;next();break;
   case Stage::DoubleLevel:copy_=uint8_t(copy_*2);next();break;
   case Stage::CaptureWord:levelWord_=uint16_t((uint16_t(level_)<<8)|copy_);next();break;
   case Stage::DispatchWord:phase_=Stage::AwaitWord;break;
   case Stage::Return:phase_=Stage::Complete;break;
   default:next();break;
   }used+=cost;
  }return used;
 }
private:
 void next(){phase_=Stage(unsigned(phase_)+1);}
 Stage phase_=Stage::ReadPart;unsigned output_=0;uint8_t part_=0,base_=0,slot_=0,copy_=0,level_=0;uint16_t levelWord_=0;
};
}
