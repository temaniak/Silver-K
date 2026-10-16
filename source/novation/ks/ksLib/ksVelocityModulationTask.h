#pragma once
#include <cstdint>
namespace ks {
// Byte-exact velocity/depth modulation shared by voice startup and the complete
// parameter upload. Output is the original 24-bit DSP word, never host floating
// point. Negative depth inverts velocity before signed multiplication.
class VelocityModulationTask final {
public:
 enum class Stage {DispatchVoice,AwaitVoice,ReadVelocity,ReadDepth,BiasDepth,AdmitSign,InvertVelocity,NegateDepth,
  BiasVelocity,Multiply,Scale,AdmitZero,SetUnity,JumpOutput,BiasHigh,CaptureWord,ClearMiddle,SetOffset,DispatchWord,AwaitWord,Complete};
 enum class Read {Velocity,Depth};enum class Child {None,Voice,Word};
 explicit VelocityModulationTask(bool relative,uint16_t offset=0):relative_(relative),offset_(offset){}
 Stage stage()const{return stage_;}uint8_t slot()const{return slot_;}uint8_t velocity()const{return velocity_;}uint8_t depth()const{return depth_;}
 int16_t product()const{return product_;}uint16_t word()const{return word_;}uint32_t dspWord()const{return dspWord_;}
 bool relative()const{return relative_;}uint16_t offset()const{return offset_;}bool complete()const{return stage_==Stage::Complete;}
 Child child()const{return stage_==Stage::AwaitVoice?Child::Voice:stage_==Stage::AwaitWord?Child::Word:Child::None;}
 bool resumeChild(uint8_t slot=0){if(stage_==Stage::AwaitVoice){slot_=slot;stage_=Stage::ReadVelocity;return true;}
  if(stage_==Stage::AwaitWord){stage_=Stage::Complete;return true;}return false;}
 unsigned nextClocks()const{switch(stage_){case Stage::DispatchVoice:case Stage::DispatchWord:return 20;
 case Stage::ReadVelocity:case Stage::ReadDepth:return 15;case Stage::Multiply:return 18;case Stage::SetUnity:return 9;
 case Stage::SetOffset:return 6;case Stage::AwaitVoice:case Stage::AwaitWord:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;
  while(const auto cost=nextClocks()){if(cost>available-used)break;
   switch(stage_){case Stage::DispatchVoice:stage_=Stage::AwaitVoice;break;
   case Stage::ReadVelocity:velocity_=uint8_t(read(Read::Velocity));stage_=Stage::ReadDepth;break;
   case Stage::ReadDepth:depth_=uint8_t(read(Read::Depth));stage_=Stage::BiasDepth;break;
   case Stage::BiasDepth:depth_=uint8_t(depth_-64);stage_=Stage::AdmitSign;break;
   case Stage::AdmitSign:stage_=depth_&0x80?Stage::InvertVelocity:Stage::BiasVelocity;break;
   case Stage::InvertVelocity:velocity_^=0x7f;stage_=Stage::NegateDepth;break;
   case Stage::NegateDepth:depth_=uint8_t(0-depth_);stage_=Stage::BiasVelocity;break;
   case Stage::BiasVelocity:velocity_=uint8_t(velocity_-127);stage_=Stage::Multiply;break;
   case Stage::Multiply:product_=int16_t(signedByte(depth_)*signedByte(velocity_));word_=uint16_t(product_);stage_=Stage::Scale;break;
   case Stage::Scale:word_=uint16_t(uint32_t(word_)<<2);stage_=Stage::AdmitZero;break;
   case Stage::AdmitZero:stage_=word_?Stage::BiasHigh:Stage::SetUnity;break;
   case Stage::SetUnity:dspWord_=0x7fffff;stage_=Stage::JumpOutput;break;
   case Stage::JumpOutput:stage_=relative_?Stage::SetOffset:Stage::DispatchWord;break;
   case Stage::BiasHigh:word_=uint16_t((uint16_t(uint8_t((word_>>8)+127))<<8)|(word_&0xff));stage_=Stage::CaptureWord;break;
   case Stage::CaptureWord:dspWord_=uint32_t(word_)<<8;stage_=Stage::ClearMiddle;break;
   case Stage::ClearMiddle:stage_=relative_?Stage::SetOffset:Stage::DispatchWord;break;
   case Stage::SetOffset:stage_=Stage::DispatchWord;break;case Stage::DispatchWord:stage_=Stage::AwaitWord;break;default:break;}
   used+=cost;
  }return used;
 }
private:
 static int signedByte(uint8_t v){return v<128?int(v):int(v)-256;}
 bool relative_;uint16_t offset_;Stage stage_=Stage::DispatchVoice;uint8_t slot_=0,velocity_=0,depth_=0;
 int16_t product_=0;uint16_t word_=0;uint32_t dspWord_=0;
};
}
