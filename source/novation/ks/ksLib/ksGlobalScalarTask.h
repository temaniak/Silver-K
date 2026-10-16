#pragma once
#include <cstdint>
namespace ks {
// Seven global DSP control packet shapes. Native intermediates are unsigned
// fixed-width values; timing phases preserve interruptible publication points.
class GlobalScalarTask final{public:
 enum class Kind{Enabled,Gain,Balance,Delay,CenteredA,CenteredB,CenteredC};
 enum class Stage{ReadValue,ClearWord,TestEnabled,AdmitEnabled,EnableWord,BiasValue,ClearLow,CompareEndpoint,AdmitEndpoint,EndpointLow,Scale,Multiplier,Product,Half,Capture,Reduce1,Reduce2,Reduce3,Reduce4,Subtract,CorrectLow,ClearWire,Offset,Dispatch,Await,Complete};
 explicit GlobalScalarTask(Kind k):kind_(k){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}uint8_t raw()const{return raw_;}uint8_t biased()const{return biased_;}uint16_t shaped()const{return shaped_;}uint16_t captured()const{return captured_;}uint32_t product()const{return product_;}uint8_t corrected()const{return corrected_;}
 uint16_t field()const{return kind_==Kind::Enabled?123:kind_==Kind::Gain?97:kind_==Kind::Balance?98:kind_==Kind::Delay?99:uint16_t(81+unsigned(kind_)-unsigned(Kind::CenteredA));}
 uint16_t offset()const{return uint16_t(0x80b+unsigned(kind_));}
 uint32_t wireWord()const{switch(kind_){case Kind::Enabled:return (raw_&64)?0x7fff00:0;case Kind::Gain:return product_>>8;case Kind::Balance:return (uint32_t(captured_)<<8)|corrected_;case Kind::Delay:return uint32_t(captured_)<<8;default:return uint32_t(captured_)<<8;}}
 bool awaiting()const{return stage_==Stage::Await;}bool resumeChild(){if(!awaiting())return false;stage_=Stage::Complete;return true;}
 unsigned nextClocks()const{switch(stage_){case Stage::ReadValue:return 15;case Stage::EnableWord:case Stage::Multiplier:case Stage::Offset:return 6;case Stage::Product:return 23;case Stage::Dispatch:return 20;case Stage::Await:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&&read){unsigned used=0;while(auto cost=nextClocks()){if(cost>available-used)break;switch(stage_){case Stage::ReadValue:raw_=uint8_t(read());shaped_=uint16_t(raw_)<<8;stage_=kind_==Kind::Enabled?Stage::ClearWord:kind_==Kind::Gain?Stage::ClearLow:Stage::BiasValue;break;
 case Stage::ClearWord:stage_=Stage::TestEnabled;break;case Stage::TestEnabled:stage_=Stage::AdmitEnabled;break;case Stage::AdmitEnabled:stage_=(raw_&64)?Stage::EnableWord:Stage::Offset;break;case Stage::EnableWord:stage_=Stage::Offset;break;
 case Stage::BiasValue:biased_=uint8_t(raw_-(kind_==Kind::Delay?128:64));shaped_=uint16_t(biased_)<<8;stage_=kind_==Kind::Delay?Stage::Half:Stage::ClearLow;break;case Stage::ClearLow:shaped_&=0xff00;stage_=kind_==Kind::Balance?Stage::CompareEndpoint:kind_==Kind::Delay?Stage::Capture:Stage::Scale;break;
 case Stage::CompareEndpoint:stage_=Stage::AdmitEndpoint;break;case Stage::AdmitEndpoint:stage_=biased_==63?Stage::EndpointLow:Stage::Scale;break;case Stage::EndpointLow:shaped_|=255;stage_=Stage::Scale;break;case Stage::Scale:shaped_=uint16_t(shaped_<<1);stage_=kind_==Kind::Gain?Stage::Multiplier:Stage::Capture;break;case Stage::Multiplier:stage_=Stage::Product;break;case Stage::Product:product_=uint32_t(shaped_)*0x8102u;stage_=Stage::Offset;break;
 case Stage::Half:shaped_>>=1;stage_=Stage::ClearLow;break;case Stage::Capture:captured_=shaped_;stage_=kind_==Kind::Balance?Stage::CorrectLow:kind_==Kind::Delay?Stage::Reduce1:Stage::ClearWire;break;
 case Stage::Reduce1:shaped_>>=1;stage_=Stage::Reduce2;break;case Stage::Reduce2:shaped_>>=1;stage_=Stage::Reduce3;break;case Stage::Reduce3:shaped_>>=1;stage_=Stage::Reduce4;break;case Stage::Reduce4:shaped_>>=1;stage_=Stage::Subtract;break;case Stage::Subtract:captured_=uint16_t(captured_-shaped_);stage_=Stage::ClearWire;break;
 case Stage::CorrectLow:corrected_=uint8_t((shaped_>>8)-uint8_t(shaped_));stage_=Stage::Offset;break;case Stage::ClearWire:stage_=Stage::Offset;break;case Stage::Offset:stage_=Stage::Dispatch;break;case Stage::Dispatch:stage_=Stage::Await;break;default:break;}used+=cost;}return used;}
private:Kind kind_;Stage stage_=Stage::ReadValue;uint8_t raw_=0,biased_=0,corrected_=0;uint16_t shaped_=0,captured_=0;uint32_t product_=0;
};}
