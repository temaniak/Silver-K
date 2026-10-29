#pragma once
#include <cstdint>
namespace kstation {
class ScriptSettingsTask final {
public:enum class Kind:uint8_t{Enter,Exit};enum class Stage:uint8_t{BindSource,BindTarget,BindCount,Read,Write,Increment,Decrement,Repeat,BindSecondSource,BindSecondTarget,BindSecondCount,ReadSecond,WriteSecond,IncrementSecond,DecrementSecond,RepeatSecond,BindProgram,BindFlag,WriteFlag,Return,Complete};explicit ScriptSettingsTask(Kind k):kind_(k){}Kind kind()const{return kind_;}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}uint32_t source()const{return (kind_==Kind::Exit?0xcf1e:stage_>=Stage::ReadSecond?0x21fe00:0xbb12)+index_;}uint32_t target()const{return (kind_==Kind::Exit?0xbb12:stage_>=Stage::ReadSecond?0xbb12:0xcf1e)+index_;}
 template<class R,class W>bool advance(R&&read,W&&write){switch(stage_){case Stage::BindCount:case Stage::BindSecondCount:index_=0;break;case Stage::Read:case Stage::ReadSecond:value_=uint8_t(read(source()));break;case Stage::Write:case Stage::WriteSecond:write(target(),value_);break;case Stage::Increment:case Stage::IncrementSecond:++index_;break;case Stage::Repeat:if(index_<23){stage_=Stage::Read;return true;}if(kind_==Kind::Exit){stage_=Stage::Return;return true;}break;case Stage::RepeatSecond:if(index_<23){stage_=Stage::ReadSecond;return true;}break;case Stage::WriteFlag:write(0xbb1c,1);break;case Stage::Complete:return false;default:break;}stage_=Stage(unsigned(stage_)+1);return true;}
private:Kind kind_;Stage stage_=Stage::BindSource;unsigned index_=0;uint8_t value_=0;
};
}
