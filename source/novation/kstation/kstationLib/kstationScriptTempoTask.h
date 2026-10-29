#pragma once
#include <cstdint>
namespace kstation {
class ScriptTempoTask final {
public:enum class Stage:uint8_t{Shift1,Shift2,Shift3,Shift4,ReadDivision,BindScale,BindFactor,MultiplyScale,BindClock,MultiplyClock,Divide,Extend,Write,Return,Complete};explicit ScriptTempoTask(uint32_t input):input_(input){}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}uint16_t result()const{return value_;}
 template<class R,class W>bool advance(R&&read,W&&write){switch(stage_){case Stage::Shift1:case Stage::Shift2:case Stage::Shift3:case Stage::Shift4:input_>>=1;break;case Stage::ReadDivision:(void)read();break;case Stage::MultiplyScale:product_=0x180u*16u;break;case Stage::MultiplyClock:product_=uint32_t(uint16_t(product_))*0x3e80u;break;case Stage::Divide:{auto divisor=uint16_t(input_);value_=divisor?uint16_t(product_/divisor):0;break;}case Stage::Write:write(value_);break;default:break;}if(stage_==Stage::Complete)return false;stage_=Stage(unsigned(stage_)+1);return true;}
private:Stage stage_=Stage::Shift1;uint32_t input_=0,product_=0;uint16_t value_=0;
};
}
