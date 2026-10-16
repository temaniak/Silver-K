#pragma once
#include <array>
#include <cstdint>
namespace ks {
class ParameterFilterTask final {
public:
 enum class Stage {DispatchVoice,ReadVelocity,BiasVelocity,ReadDepth,BiasDepth,MultiplyVelocity,DoubleModulation,ReadCutoff,ClearLow,HalfCutoff,AddModulation,AdmitSign,ClampZero,CompareMaximum,AdmitMaximum,ClampMaximum,DoubleCutoff,SetScale,MultiplyCutoff,SubtractOrigin,ReadStatic,AddStatic,DispatchWord,AwaitVoice,AwaitWord,Complete};enum class Read {Velocity,Depth,Cutoff,Static};Stage stage()const{return phase_;}uint8_t slot()const{return slot_;}uint32_t dspWord()const{return converted_>>8;}
 bool awaitingVoice()const{return phase_==Stage::AwaitVoice;}bool awaitingWord()const{return phase_==Stage::AwaitWord;}bool complete()const{return phase_==Stage::Complete;}
 bool resumeVoice(uint8_t slot){if(!awaitingVoice())return false;slot_=slot;phase_=Stage::ReadVelocity;return true;}bool resumeWord(){if(!awaitingWord())return false;phase_=Stage::Complete;return true;}
 unsigned nextClocks()const{constexpr std::array<unsigned,23> c{{20,15,3,15,3,18,3,15,3,3,3,3,3,6,3,6,3,6,23,9,12,3,20}};return unsigned(phase_)<c.size()?c[unsigned(phase_)]:0;}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){case Stage::DispatchVoice:phase_=Stage::AwaitVoice;break;case Stage::ReadVelocity:velocity_=uint8_t(read(Read::Velocity));next();break;case Stage::BiasVelocity:velocity_=uint8_t(velocity_-64);next();break;
 case Stage::ReadDepth:depth_=uint8_t(read(Read::Depth));next();break;case Stage::BiasDepth:depth_=uint8_t(depth_-64);next();break;
 case Stage::MultiplyVelocity:modulation_=uint16_t(sign(velocity_)*sign(depth_));next();break;case Stage::DoubleModulation:modulation_=uint16_t(unsigned(modulation_)*2);next();break;
 case Stage::ReadCutoff:cutoff_=uint16_t(uint8_t(read(Read::Cutoff)))<<8;next();break;case Stage::HalfCutoff:cutoff_>>=1;next();break;case Stage::AddModulation:cutoff_=uint16_t(cutoff_+modulation_);next();break;
 case Stage::AdmitSign:phase_=cutoff_&32768?Stage::ClampZero:Stage::CompareMaximum;break;case Stage::ClampZero:cutoff_=0;next();break;
 case Stage::AdmitMaximum:phase_=cutoff_<=0x3f80?Stage::DoubleCutoff:Stage::ClampMaximum;break;case Stage::ClampMaximum:cutoff_=0x3f80;next();break;
 case Stage::DoubleCutoff:cutoff_=uint16_t(unsigned(cutoff_)*2);next();break;case Stage::MultiplyCutoff:converted_=uint32_t(cutoff_)*0xaaab;next();break;
 case Stage::SubtractOrigin:converted_-=0x48001400;next();break;case Stage::ReadStatic:base_=uint16_t(read(Read::Static));next();break;case Stage::AddStatic:converted_+=uint32_t(base_)<<16;next();break;
 case Stage::DispatchWord:phase_=Stage::AwaitWord;break;default:next();break;
 }used+=cost;}return used;}
private:
 static int sign(uint8_t v){return v<128?int(v):int(v)-256;}void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_=Stage::DispatchVoice;uint8_t slot_=0,velocity_=0,depth_=0;uint16_t modulation_=0,cutoff_=0,base_=0;uint32_t converted_=0;
};
}
