#pragma once
#include <cstdint>
namespace ks {
// StateValueBounds semantic policy: logical data, decisions and deferred effects; compatibility frame stays in Calls.
class StateValueBoundsTask final {
public:
 enum class Kind { Resolve };enum class Stage { BindDefaultBounds, TestValueType, SelectDefaultBounds, TestBipolarOne, SelectBipolarOne, BindBipolarOneBounds, TestBipolarOneMode, SelectBipolarOneMode, BindBipolarOneScale, BiasBipolarOne, MultiplyBipolarOne, CenterBipolarOne, BindBipolarOne, JoinBipolarOne, TestBipolarTwo, SelectBipolarTwo, BindBipolarTwoBounds, TestBipolarTwoMode, SelectBipolarTwoMode, BindBipolarTwoScale, BiasBipolarTwo, MultiplyBipolarTwo, DoubleBipolarTwo, CenterBipolarTwo, BindBipolarTwo, JoinBipolarTwo, TestOscillator, SelectOscillator, ReadOscillatorPatch, ReadOscillator, NormalizeOscillatorWord, NormalizeOscillatorLong, LocateOscillator, BindOscillatorBounds, JoinOscillator, TestModulation, SelectModulation, ReadModulationPatch, ReadModulation, NormalizeModulation, BindModulationStride, MultiplyModulation, LocateModulation, BindModulationBounds, JoinModulation, TestTwenty, SelectTwenty, BindTwentyBounds, TestTwentyMode, SelectTwentyMode, BindTwentyScale, MultiplyTwenty, JoinTwenty, TestTwentyFour, SelectTwentyFour, BindTwentyFourBounds, TestTwentyFourMode, SelectTwentyFourMode, BindTwentyFourScale, MultiplyTwentyFour, JoinTwentyFour, TestThirtyTwo, SelectThirtyTwo, BindThirtyTwoBounds, TestThirtyTwoMode, SelectThirtyTwoMode, BindThirtyTwoScale, MultiplyThirtyTwo, JoinThirtyTwo, PublishBounds, ReturnBounds, AwaitChild, Complete };
 enum class Read { Patch, Oscillator, Modulation };uint32_t patch()const{return patch_;}uint32_t index()const{return index_;}uint32_t bounds()const{return bounds_;}uint32_t value()const{return value_;}uint32_t pointer()const{return pointer_;}
 explicit StateValueBoundsTask(Kind kind,uint8_t type=0,uint8_t value=0,bool absolute=false,uint32_t pointer=0){type_=type;value_=value;absolute_=absolute;pointer_=pointer;switch(kind){case Kind::Resolve:stage_=Stage::BindDefaultBounds;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::BindDefaultBounds:case Stage::BindBipolarOneBounds:case Stage::BindBipolarTwoBounds:case Stage::BindOscillatorBounds:case Stage::BindModulationStride:case Stage::BindModulationBounds:case Stage::BindTwentyBounds:case Stage::BindTwentyFourBounds:case Stage::BindThirtyTwoBounds:return 6; case Stage::SelectDefaultBounds:case Stage::SelectBipolarOneMode:case Stage::JoinBipolarOne:case Stage::SelectBipolarTwoMode:case Stage::JoinBipolarTwo:return 8; case Stage::MultiplyBipolarOne:case Stage::MultiplyBipolarTwo:return 18; case Stage::ReadOscillatorPatch:case Stage::ReadOscillator:case Stage::ReadModulationPatch:case Stage::ReadModulation:return 12+store; case Stage::MultiplyModulation:return 23; case Stage::MultiplyTwenty:case Stage::MultiplyTwentyFour:case Stage::MultiplyThirtyTwo:return 15; case Stage::ReturnBounds:return 17; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::BindDefaultBounds:bounds_=0x7f00;next();break;
case Stage::SelectDefaultBounds:stage_=type_?Stage::TestBipolarOne:Stage::PublishBounds;break;
case Stage::SelectBipolarOne:stage_=type_==1?Stage::BindBipolarOneBounds:Stage::TestBipolarTwo;break;
case Stage::BindBipolarOneBounds:bounds_=0x4c34;next();break;
case Stage::SelectBipolarOneMode:stage_=absolute_?Stage::PublishBounds:Stage::BindBipolarOneScale;break;
case Stage::BiasBipolarOne:value_=uint8_t(value_-61);next();break;
case Stage::MultiplyBipolarOne:scaled_=int16_t(int8_t(value_))*49;next();break;
case Stage::CenterBipolarOne:scaled_=int16_t((uint16_t(scaled_)&255)|(uint16_t(uint8_t((uint16_t(scaled_)>>8)+64))<<8));next();break;
case Stage::BindBipolarOne:value_=uint8_t(uint16_t(scaled_)>>8);next();break;
case Stage::SelectBipolarTwo:stage_=type_==2?Stage::BindBipolarTwoBounds:Stage::TestOscillator;break;
case Stage::BindBipolarTwoBounds:bounds_=0x720e;next();break;
case Stage::SelectBipolarTwoMode:stage_=absolute_?Stage::PublishBounds:Stage::BindBipolarTwoScale;break;
case Stage::BiasBipolarTwo:value_=uint8_t(value_-63);next();break;
case Stage::MultiplyBipolarTwo:scaled_=int16_t(int8_t(value_))*101;next();break;
case Stage::DoubleBipolarTwo:scaled_=int16_t(uint16_t(scaled_)<<1);next();break;
case Stage::CenterBipolarTwo:scaled_=int16_t((uint16_t(scaled_)&255)|(uint16_t(uint8_t((uint16_t(scaled_)>>8)+64))<<8));next();break;
case Stage::BindBipolarTwo:value_=uint8_t(uint16_t(scaled_)>>8);next();break;
case Stage::SelectOscillator:stage_=type_==3?Stage::ReadOscillatorPatch:Stage::TestModulation;break;
case Stage::ReadOscillatorPatch:patch_=read(Read::Patch,0);next();break;
case Stage::ReadOscillator:index_=uint8_t(read(Read::Oscillator,patch_+224));next();break;
case Stage::LocateOscillator:pointer_+=index_;next();break;
case Stage::BindOscillatorBounds:bounds_=0x7f00;next();break;
case Stage::SelectModulation:stage_=type_==19?Stage::ReadModulationPatch:Stage::TestTwenty;break;
case Stage::ReadModulationPatch:patch_=read(Read::Patch,0);next();break;
case Stage::ReadModulation:index_=uint8_t(read(Read::Modulation,patch_+139));next();break;
case Stage::LocateModulation:pointer_+=uint32_t(index_)*104;next();break;
case Stage::BindModulationBounds:bounds_=0x7f00;next();break;
case Stage::SelectTwenty:stage_=type_==10?Stage::BindTwentyBounds:Stage::TestTwentyFour;break;
case Stage::BindTwentyBounds:bounds_=0x1300;next();break;
case Stage::SelectTwentyMode:stage_=absolute_?Stage::PublishBounds:Stage::BindTwentyScale;break;
case Stage::MultiplyTwenty:value_=uint8_t(uint16_t(value_)*40>>8);next();break;
case Stage::SelectTwentyFour:stage_=type_==21?Stage::BindTwentyFourBounds:Stage::TestThirtyTwo;break;
case Stage::BindTwentyFourBounds:bounds_=0x1700;next();break;
case Stage::SelectTwentyFourMode:stage_=absolute_?Stage::PublishBounds:Stage::BindTwentyFourScale;break;
case Stage::MultiplyTwentyFour:value_=uint8_t(uint16_t(value_)*48>>8);next();break;
case Stage::SelectThirtyTwo:stage_=type_==22?Stage::BindThirtyTwoBounds:Stage::PublishBounds;break;
case Stage::BindThirtyTwoBounds:bounds_=0x1f00;next();break;
case Stage::SelectThirtyTwoMode:stage_=absolute_?Stage::PublishBounds:Stage::BindThirtyTwoScale;break;
case Stage::MultiplyThirtyTwo:value_=uint8_t(uint16_t(value_)*64>>8);next();break;
case Stage::JoinBipolarOne:stage_=Stage::PublishBounds;break;
case Stage::JoinBipolarTwo:stage_=Stage::PublishBounds;break;
case Stage::JoinOscillator:stage_=Stage::PublishBounds;break;
case Stage::JoinModulation:stage_=Stage::PublishBounds;break;
case Stage::JoinTwenty:stage_=Stage::PublishBounds;break;
case Stage::JoinTwentyFour:stage_=Stage::PublishBounds;break;
case Stage::JoinThirtyTwo:stage_=Stage::PublishBounds;break;
case Stage::ReturnBounds:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t pointer_=0,patch_=0;uint16_t bounds_=0;int16_t scaled_=0;uint8_t type_=0,value_=0,index_=0;bool absolute_=false;
};
}
