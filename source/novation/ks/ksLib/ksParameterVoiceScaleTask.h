#pragma once
#include <cstdint>
namespace ks {
class ParameterVoiceScaleTask final {
public:
 enum class Stage {DispatchVoice,ReadAmplitude,ClearLow,ReadPatch,ReadGain,HalfGain0,HalfGain1,ClearGainHigh,Multiply,DispatchWord,AwaitVoice,AwaitWord,Complete};enum class Read {Amplitude,Patch,Gain};Stage stage()const{return phase_;}uint8_t slot()const{return slot_;}uint32_t dspWord()const{return converted_>>8;}
 bool awaitingVoice()const{return phase_==Stage::AwaitVoice;}bool awaitingWord()const{return phase_==Stage::AwaitWord;}bool complete()const{return phase_==Stage::Complete;}
 bool resumeVoice(uint8_t slot){if(!awaitingVoice())return false;slot_=slot;phase_=Stage::ReadAmplitude;return true;}
 bool resumeWord(){if(!awaitingWord())return false;phase_=Stage::Complete;return true;}
 unsigned nextClocks()const{switch(phase_){case Stage::DispatchVoice:case Stage::DispatchWord:return 20;case Stage::ReadAmplitude:case Stage::ReadGain:return 15;case Stage::ReadPatch:return 21;case Stage::Multiply:return 26;case Stage::AwaitVoice:case Stage::AwaitWord:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){case Stage::DispatchVoice:phase_=Stage::AwaitVoice;break;case Stage::ReadAmplitude:amplitude_=uint8_t(read(Read::Amplitude));next();break;
 case Stage::ReadPatch:(void)read(Read::Patch);next();break;case Stage::ReadGain:gain_=uint8_t(read(Read::Gain));next();break;case Stage::HalfGain0:case Stage::HalfGain1:gain_>>=1;next();break;
 case Stage::Multiply:converted_=uint32_t((amplitude_<128?int(amplitude_):int(amplitude_)-256)*256*int(gain_));next();break;
 case Stage::DispatchWord:phase_=Stage::AwaitWord;break;default:next();break;
 }used+=cost;}return used;}
private:
 void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_=Stage::DispatchVoice;uint8_t slot_=0,amplitude_=0,gain_=0;uint32_t converted_=0;
};
}
