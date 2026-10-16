#pragma once
#include <cstdint>
namespace ks {
// Four patch modulation outputs. Live fields are observed at their historical
// deadlines; all arithmetic is explicit integer wrap with no host float math.
// The task owns domain values and child requests, never CPU architectural state.
class ParameterModulationTask final {
public:
 explicit ParameterModulationTask(unsigned channel):output_(channel){}
 enum class Stage {ReadBias,Bias,SignBias,ReadDepth0,BiasDepth0,ReadGlobal0,Multiply0,BeginSum,ReadDepth1,BiasDepth1,ReadGlobal1,Multiply1,AddSum,ReadDepth2,BiasDepth2,ReadGlobal2,Multiply2,FinishSum,SetScale,Scale,CaptureScaled,SetFactor,AddBias,CaptureCombined,AdmitSign,NegateMagnitude,NegateFactor,CompareMagnitude,AdmitClamp,ClampMagnitude,DoubleIndex,ClearIndex,ReadTable,MultiplyTable,DoubleOutput,SetOffset,DispatchWord,AwaitWord,Return,Complete};
 enum class Read {Bias,Depth,Modulation,Curve};
 Stage stage()const{return phase_;}unsigned output()const{return output_;}
 bool complete()const{return phase_==Stage::Complete;}bool awaitingWord()const{return phase_==Stage::AwaitWord;}
 uint16_t biasOffset()const{return output_==3?51:uint16_t(10+8*output_);}
 uint16_t depthOffset()const{return uint16_t((output_==3?53:34)+term_);}
 uint16_t modulationOffset()const{return uint16_t(term_);}
 uint16_t curveIndex()const{return magnitude_;}
 uint16_t offset()const{constexpr uint16_t offsets[]{0x34,0x3f,0x4a,0x61};return offsets[output_];}
 uint32_t dspWord()const{return converted_>>8;}
 bool resumeWord(){if(!awaitingWord())return false;phase_=Stage::Complete;return true;}
 unsigned nextClocks()const{switch(phase_){
 case Stage::ReadBias:case Stage::ReadDepth0:case Stage::ReadDepth1:case Stage::ReadDepth2:case Stage::ReadTable:return 15;
 case Stage::ReadGlobal0:case Stage::ReadGlobal1:case Stage::ReadGlobal2:return 12;
 case Stage::Multiply0:case Stage::Multiply1:case Stage::Multiply2:return 18;
 case Stage::Scale:case Stage::MultiplyTable:return 26;
 case Stage::SetScale:case Stage::SetFactor:case Stage::CompareMagnitude:case Stage::ClampMagnitude:case Stage::SetOffset:return 6;
 case Stage::ClearIndex:return 3;case Stage::DispatchWord:return 20;case Stage::Return:return 17;
 case Stage::AwaitWord:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;
  while(const unsigned cost=nextClocks()){if(cost>available-used)break;
   switch(phase_){
    case Stage::ReadBias:bias_=uint8_t(read(Read::Bias));phase_=Stage::Bias;break;
    case Stage::Bias:bias_=uint8_t(bias_-64);phase_=Stage::SignBias;break;
    case Stage::SignBias:base_=uint16_t(signedByte(bias_));term_=0;phase_=Stage::ReadDepth0;break;
    case Stage::ReadDepth0:case Stage::ReadDepth1:case Stage::ReadDepth2:depth_=uint8_t(read(Read::Depth));next();break;
    case Stage::BiasDepth0:case Stage::BiasDepth1:case Stage::BiasDepth2:depth_=uint8_t(depth_-64);next();break;
    case Stage::ReadGlobal0:case Stage::ReadGlobal1:case Stage::ReadGlobal2:modulation_=uint8_t(read(Read::Modulation));next();break;
    case Stage::Multiply0:case Stage::Multiply1:case Stage::Multiply2:product_=uint16_t(signedByte(depth_)*signedByte(modulation_));next();break;
    case Stage::BeginSum:sum_=product_;++term_;next();break;
    case Stage::AddSum:sum_=uint16_t(sum_+product_);++term_;next();break;
    case Stage::FinishSum:sum_=uint16_t(sum_+product_);next();break;
    case Stage::Scale:scaled_=uint16_t((uint32_t(signedWord(sum_)*512)>>16)&0xffff);next();break;
    case Stage::AddBias:combined_=uint16_t(base_+scaled_);magnitude_=combined_;next();break;
    case Stage::SetFactor:factor_=0x5555;next();break;
    case Stage::AdmitSign:phase_=combined_&0x8000?Stage::NegateMagnitude:Stage::CompareMagnitude;break;
    case Stage::NegateMagnitude:magnitude_=uint16_t(0-magnitude_);next();break;
    case Stage::NegateFactor:factor_=int16_t(-factor_);next();break;
    case Stage::AdmitClamp:phase_=magnitude_<=64?Stage::DoubleIndex:Stage::ClampMagnitude;break;
    case Stage::ClampMagnitude:magnitude_=64;next();break;
    case Stage::DoubleIndex:magnitude_=uint16_t(magnitude_*2);next();break;
    case Stage::ReadTable:curve_=uint16_t(read(Read::Curve));next();break;
    case Stage::MultiplyTable:converted_=uint32_t(signedWord(curve_)*int32_t(factor_));next();break;
    case Stage::DoubleOutput:converted_<<=1;phase_=Stage::DispatchWord;break;
    case Stage::DispatchWord:phase_=Stage::AwaitWord;break;
    case Stage::Return:phase_=Stage::Complete;break;
    default:next();break;
   }used+=cost;
  }return used;
 }
private:
 static int signedByte(uint8_t v){return v<128?int(v):int(v)-256;}
 static int signedWord(uint16_t v){return v<32768?int(v):int(v)-65536;}
 void next(){phase_=Stage(unsigned(phase_)+1);}
 Stage phase_=Stage::ReadBias;unsigned output_=0,term_=0;uint8_t bias_=0,depth_=0,modulation_=0;
 uint16_t base_=0,product_=0,sum_=0,scaled_=0,combined_=0,magnitude_=0,curve_=0;int16_t factor_=0;uint32_t converted_=0;
};
}
