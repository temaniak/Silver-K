#pragma once
#include <array>
#include <cstdint>
namespace ks {
// Three pitch outputs, including per-voice unison distribution. Every domain
// operation wraps at the original byte/word width; wheel/fine fields remain live.
class ParameterPitchTask final {
public:
 explicit ParameterPitchTask(unsigned channel):output_(channel){}
 enum class Stage {ReadStaticPitch,ReadOctaves,SetOctaveShift,AdmitShift,ShiftOctaves,DecrementShift,AdmitMoreShift,MaskOctaves,BiasOctave,SetSemitoneScale,MultiplyOctave,AddOctave,ReadCoarse,BiasCoarse,AddCoarse,BindGlobals,ReadGlobalFine,ReadPatchFine,AddFine,ReadWheelFine,AddWheelFine,ClearCarry,CaptureCarry,BiasFine,ReadFlags,MaskUnison,AdmitUnison,ScaleUnison,DispatchVoice,ReadUnisonCounter,MaskUnisonCounter,DoubleCounter0,DoubleCounter1,DoubleCounter2,BiasCounter,CenterCounter,BiasDivisor,DoubleDivisor,DivideUnison,ReadSelectedPatch,ReadSpread,MultiplySpread,CaptureSpread,SignSpread,AddSpread,ScaleFine0,ScaleFine1,ScaleFine2,SetFineScale,MultiplyFine,AddScaledFine,ReadPitchDepth,BiasPitchDepth,SignPitchDepth,CapturePitchDepth,ReadWheel,BiasWheel,MultiplyWheel,AddWheel,SetPitchScale,MultiplyPitch,SetOffset,DispatchWord,AwaitVoice,AwaitWord,Return,Complete};enum class Read {StaticPitch,Octaves,Coarse,GlobalFine,PatchFine,WheelFine,Flags,UnisonCounter,SelectedPatch,Spread,PitchDepth,Wheel};
 Stage stage()const{return phase_;}unsigned output()const{return output_;}uint8_t slot()const{return slot_;}
 bool complete()const{return phase_==Stage::Complete;}bool awaitingVoice()const{return phase_==Stage::AwaitVoice;}bool awaitingWord()const{return phase_==Stage::AwaitWord;}
 uint16_t coarseOffset()const{return uint16_t(7+8*output_);}uint16_t fineOffset()const{return uint16_t(8+8*output_);}uint16_t depthOffset()const{return uint16_t(9+8*output_);}
 uint16_t offset()const{return uint16_t(0x32+11*output_);}uint32_t dspWord()const{return pitchProduct_>>8;}
 bool resumeVoice(uint8_t slot){if(!awaitingVoice())return false;slot_=slot;phase_=Stage::ReadUnisonCounter;return true;}
 bool resumeWord(){if(!awaitingWord())return false;phase_=Stage::Complete;return true;}
 unsigned nextClocks()const{constexpr std::array<unsigned,63> clocks{{12,15,3,3,3,3,3,3,3,3,18,3,15,3,3,9,15,15,3,12,3,3,3,6,15,3,3,3,20,15,3,3,3,3,3,3,3,3,18,21,15,18,3,3,3,3,3,3,6,26,3,15,3,3,3,12,6,26,3,6,23,6,20}};const auto s=unsigned(phase_);return s<63?clocks[s]:phase_==Stage::Return?17:0;}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;
  while(const auto cost=nextClocks()){if(cost>available-used)break;
   switch(phase_){
   case Stage::ReadStaticPitch:pitch_=uint16_t(read(Read::StaticPitch));next();break;
   case Stage::ReadOctaves:octave_=uint8_t(read(Read::Octaves));next();break;
   case Stage::SetOctaveShift:shift_=uint8_t(output_);next();break;
   case Stage::AdmitShift:phase_=shift_?Stage::ShiftOctaves:Stage::MaskOctaves;break;
   case Stage::ShiftOctaves:octave_>>=2;next();break;
   case Stage::DecrementShift:--shift_;next();break;
   case Stage::AdmitMoreShift:phase_=shift_?Stage::ShiftOctaves:Stage::MaskOctaves;break;
   case Stage::MaskOctaves:octave_&=3;next();break;
   case Stage::BiasOctave:octave_=uint8_t(octave_-1);next();break;
   case Stage::MultiplyOctave:octaveProduct_=uint16_t(sign8(octave_)*12);next();break;
   case Stage::AddOctave:addHigh(uint8_t(octaveProduct_));next();break;
   case Stage::ReadCoarse:coarse_=uint8_t(read(Read::Coarse));next();break;
   case Stage::BiasCoarse:coarse_=uint8_t(coarse_-64);next();break;
   case Stage::AddCoarse:addHigh(coarse_);next();break;
   case Stage::ReadGlobalFine:fine_=uint8_t(read(Read::GlobalFine));next();break;
   case Stage::ReadPatchFine:patchFine_=uint8_t(read(Read::PatchFine));next();break;
   case Stage::AddFine:fine_=uint8_t(fine_+patchFine_);next();break;
   case Stage::ReadWheelFine:wheelFine_=uint8_t(read(Read::WheelFine));next();break;
   case Stage::AddWheelFine:fineWord_=uint16_t(unsigned(fine_)+wheelFine_);next();break;
   case Stage::BiasFine:fineWord_=uint16_t(fineWord_-192);next();break;
   case Stage::ReadFlags:unison_=uint8_t(read(Read::Flags));next();break;
   case Stage::MaskUnison:unison_&=7;next();break;
   case Stage::AdmitUnison:phase_=unison_?Stage::ScaleUnison:Stage::ScaleFine0;break;
   case Stage::ScaleUnison:unison_=uint8_t(unison_*4);next();break;
   case Stage::DispatchVoice:phase_=Stage::AwaitVoice;break;
   case Stage::ReadUnisonCounter:counter_=uint8_t(read(Read::UnisonCounter));next();break;
   case Stage::MaskUnisonCounter:counter_&=7;next();break;
   case Stage::DoubleCounter0:case Stage::DoubleCounter1:case Stage::DoubleCounter2:counter_=uint8_t(counter_*2);next();break;
   case Stage::BiasCounter:counter_=uint8_t(counter_+1);next();break;
   case Stage::CenterCounter:counter_=uint8_t(counter_-unison_);next();break;
   case Stage::BiasDivisor:divisor_=uint8_t(unison_+12);next();break;
   case Stage::DoubleDivisor:divisor_=uint8_t(divisor_*2);next();break;
   case Stage::DivideUnison:{const int numerator=sign16(uint16_t((unsigned(counter_)<<8)|divisor_));quotient_=uint8_t(numerator/sign8(divisor_));next();break;}
   case Stage::ReadSelectedPatch:(void)read(Read::SelectedPatch);next();break;
   case Stage::ReadSpread:spread_=uint8_t(read(Read::Spread));next();break;
   case Stage::MultiplySpread:spreadProduct_=uint16_t(sign8(spread_)*sign8(quotient_));next();break;
   case Stage::CaptureSpread:spread_=uint8_t(spreadProduct_>>8);next();break;
   case Stage::AddSpread:fineWord_=uint16_t(fineWord_+sign8(spread_));next();break;
   case Stage::ScaleFine0:case Stage::ScaleFine1:case Stage::ScaleFine2:fineWord_=uint16_t(unsigned(fineWord_)*2);next();break;
   case Stage::MultiplyFine:fineProduct_=uint32_t(sign16(fineWord_)*20971);next();break;
   case Stage::AddScaledFine:pitch_=uint16_t(pitch_+(fineProduct_>>16));next();break;
   case Stage::ReadPitchDepth:depth_=uint8_t(read(Read::PitchDepth));next();break;
   case Stage::BiasPitchDepth:depth_=uint8_t(depth_-64);next();break;
   case Stage::ReadWheel:wheel_=uint16_t(read(Read::Wheel));next();break;
   case Stage::BiasWheel:wheel_=uint16_t(wheel_-256);next();break;
   case Stage::MultiplyWheel:wheelProduct_=uint32_t(sign8(depth_)*sign16(wheel_));next();break;
   case Stage::AddWheel:pitch_=uint16_t(pitch_+wheelProduct_);next();break;
   case Stage::MultiplyPitch:pitchProduct_=uint32_t(pitch_)*0xaaab;phase_=Stage::DispatchWord;break;
   case Stage::DispatchWord:phase_=Stage::AwaitWord;break;
   case Stage::Return:phase_=Stage::Complete;break;
   default:next();break;
   }used+=cost;
  }return used;
 }
private:
 static int sign8(uint8_t v){return v<128?int(v):int(v)-256;}static int sign16(uint16_t v){return v<32768?int(v):int(v)-65536;}
 void next(){phase_=Stage(unsigned(phase_)+1);}void addHigh(uint8_t v){pitch_=uint16_t((uint16_t(uint8_t((pitch_>>8)+v))<<8)|(pitch_&255));}
 Stage phase_=Stage::ReadStaticPitch;unsigned output_=0;uint8_t slot_=0,octave_=0,shift_=0,coarse_=0,fine_=0,patchFine_=0,wheelFine_=0,unison_=0,counter_=0,divisor_=0,quotient_=0,spread_=0,depth_=0;
 uint16_t pitch_=0,octaveProduct_=0,fineWord_=0,spreadProduct_=0,wheel_=0;uint32_t fineProduct_=0,wheelProduct_=0,pitchProduct_=0;
};
}
