#pragma once
#include <array>
#include <cstdint>
namespace kstation {
// Semantic descriptions of the twelve fixed patch-byte aggregations. Each
// component is read live; rotations and OR deliberately retain all input bits.
struct PatchPackSpec {uint32_t entry;uint8_t destination,count;std::array<uint8_t,6>fields;std::array<int8_t,6>rotations;uint8_t biased;};
inline constexpr std::array<PatchPackSpec,12> PatchPackSpecs{{
 {0x528c,0x00,3,{0x81,0x80,0x82},{3,2,3},0},
 {0x52c0,0x05,4,{0x83,0x84,0x85,0x86},{2,2,2,2},0},
 {0x52fe,0x06,4,{0x87,0x88,0x89,0x8a},{2,2,2,2},3},
 {0x5342,0x4e,4,{0x9b,0x9c,0x8b,0x8c},{1,1,2,4},0},
 {0x5380,0x4f,6,{0xa0,0xa1,0xa2,0xa3,0xa4,0xa5},{1,1,1,1,1,3},0},
 {0x53d2,0x50,4,{0x9d,0x9e,0x9f,0x8d},{1,1,1,-3},0},
 {0x540c,0x58,5,{0x8e,0x8f,0xa6,0xa7,0x90},{2,1,1,1,3},0},
 {0x5454,0x79,2,{0x91,0x92},{3,-3},0},
 {0x547a,0x7a,3,{0x93,0x94,0x95},{2,2,4},0},
 {0x54ae,0x7b,5,{0x96,0x98,0x97,0x99,0x9a},{3,1,1,1,2},0},
 {0x54f6,0x7f,4,{0xa8,0xa9,0xaa,0xab},{2,2,2,2},0},
 {0x5534,0x7e,2,{0xac,0xad},{0,0},0}
}};
class PatchPackTask final {
public:
 enum class Stage:uint8_t {Read,Bias,Merge,Rotate,ShiftHigh,MaskHigh,BindDestination,Return,Complete};
 PatchPackTask(unsigned index,uint32_t base):index_(index),base_(base){}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}unsigned index()const{return index_;}unsigned group()const{return group_;}uint8_t value()const{return value_;}uint8_t high()const{return high_;}const PatchPackSpec&spec()const{return PatchPackSpecs[index_];}
 template<class R>bool advance(R&&read){const auto&s=spec();switch(stage_){case Stage::Read:{uint8_t x=read(base_+s.fields[group_]);if(group_)high_=x;else value_=x;if(group_<s.biased)stage_=Stage::Bias;else if(index_==11&&group_) {stage_=Stage::ShiftHigh;shift_=0;}else if(group_)stage_=Stage::Merge;else beginRotate();break;}case Stage::Bias:if(group_)high_=uint8_t(high_+0xc1);else value_=uint8_t(value_+0xc1);if(group_)stage_=Stage::Merge;else beginRotate();break;case Stage::Merge:value_|=high_;if(index_==11)stage_=Stage::BindDestination;else beginRotate();break;case Stage::Rotate:if(s.rotations[group_]<0)value_=uint8_t((value_<<1)|(value_>>7));else value_=uint8_t((value_>>1)|(value_<<7));if(--left_==0)nextGroup();break;case Stage::ShiftHigh:high_=uint8_t(high_<<1);if(++shift_==3)stage_=Stage::MaskHigh;break;case Stage::MaskHigh:high_&=0x7f;stage_=Stage::Merge;break;case Stage::BindDestination:stage_=Stage::Return;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::Complete:return false;}return true;}
private:void beginRotate(){int n=spec().rotations[group_];left_=uint8_t(n<0?-n:n);if(left_)stage_=Stage::Rotate;else nextGroup();}void nextGroup(){if(++group_==spec().count)stage_=Stage::BindDestination;else stage_=Stage::Read;}unsigned index_,group_=0;uint32_t base_;uint8_t value_=0,high_=0,left_=0,shift_=0;Stage stage_=Stage::Read;
};
}
namespace kstation {
class PatchPackRefreshTask final {
public:
 enum class Stage:uint8_t {Begin,Pack,Write,Increment,Compare,Select,Return,AwaitChild,Complete};
 Stage stage()const{return stage_;}uint8_t index()const{return index_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool resume(){if(!awaitingChild())return false;stage_=Stage::Write;return true;}
 template<class W>bool advance(W&&w){switch(stage_){case Stage::Begin:index_=1;stage_=Stage::Pack;break;case Stage::Pack:stage_=Stage::AwaitChild;break;case Stage::Write:w();stage_=Stage::Increment;break;case Stage::Increment:++index_;stage_=Stage::Compare;break;case Stage::Compare:stage_=Stage::Select;break;case Stage::Select:stage_=index_<=12?Stage::Pack:Stage::Return;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitChild:case Stage::Complete:return false;}return true;}
private:Stage stage_=Stage::Begin;uint8_t index_=0;
};
class PatchPackSelectTask final {
public:
 enum class Stage:uint8_t {Save,Decrement,ShiftOne,ShiftTwo,ExtendWord,ExtendLong,ReadTarget,BindPatch,Call,Restore,Return,AwaitChild,Complete};
 explicit PatchPackSelectTask(uint8_t index):index_(index){}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}uint32_t target()const{return target_;}uint8_t offset()const{return index_;}bool resume(){if(!awaitingChild())return false;stage_=Stage::Restore;return true;}
 template<class R>bool advance(R&&r){switch(stage_){case Stage::Save:stage_=Stage::Decrement;break;case Stage::Decrement:--index_;stage_=Stage::ShiftOne;break;case Stage::ShiftOne:index_=uint8_t(index_<<1);stage_=Stage::ShiftTwo;break;case Stage::ShiftTwo:index_=uint8_t(index_<<1);stage_=Stage::ExtendWord;break;case Stage::ExtendWord:stage_=Stage::ExtendLong;break;case Stage::ExtendLong:stage_=Stage::ReadTarget;break;case Stage::ReadTarget:target_=r(0xba2c+index_);stage_=Stage::BindPatch;break;case Stage::BindPatch:stage_=Stage::Call;break;case Stage::Call:stage_=Stage::AwaitChild;break;case Stage::Restore:stage_=Stage::Return;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitChild:case Stage::Complete:return false;}return true;}
private:Stage stage_=Stage::Save;uint8_t index_;uint32_t target_=0;
};
}
