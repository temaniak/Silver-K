#pragma once
#include <cstdint>
namespace ks {
class ParameterGateTask final {
public:
 enum class Stage {ReadGate,ReadPart,ReadActive,CompareParts,AdmitPart,ClearGate,ReadMode,CaptureGate,AdmitGate,ClearPacket,CompareMode,AdmitMode,AwaitOutput,Complete};enum class Read {Gate,Part,Active,Mode};enum class Child {Zero,Quarter,BipolarHalf};
 Stage stage()const{return phase_;}unsigned output()const{return output_;}bool awaitingOutput()const{return phase_==Stage::AwaitOutput;}bool complete()const{return phase_==Stage::Complete;}
 Child child()const{return zero_?Child::Zero:(output_==1||output_==3||output_>=5)?Child::BipolarHalf:Child::Quarter;}
 uint16_t patchOffset()const{constexpr uint16_t fields[]{39,143,38,142,37,141,171};return fields[output_];}
 bool resumeOutput(){if(!awaitingOutput())return false;phase_=++output_==7?Stage::Complete:Stage::CaptureGate;return true;}
 unsigned nextClocks()const{switch(phase_){case Stage::ReadGate:case Stage::ReadPart:case Stage::ReadActive:return 12;case Stage::ReadMode:return 15;case Stage::AwaitOutput:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){case Stage::ReadGate:gate_=uint8_t(read(Read::Gate));next();break;case Stage::ReadPart:part_=uint8_t(read(Read::Part));next();break;case Stage::ReadActive:active_=uint8_t(read(Read::Active));next();break;
 case Stage::AdmitPart:phase_=part_==active_?Stage::ReadMode:Stage::ClearGate;break;case Stage::ClearGate:gate_=0;next();break;case Stage::ReadMode:mode_=uint8_t(read(Read::Mode));next();break;
 case Stage::AdmitGate:if(gate_)phase_=Stage::ClearPacket;else{zero_=false;phase_=Stage::AwaitOutput;}break;
 case Stage::AdmitMode:{constexpr uint8_t required[]{2,2,1,1,0,0,0};zero_=mode_!=required[output_];phase_=Stage::AwaitOutput;break;}
 default:next();break;
 }used+=cost;}return used;}
private:
 void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_=Stage::ReadGate;unsigned output_=0;uint8_t gate_=0,part_=0,active_=0,mode_=0;bool zero_=false;
};
}
