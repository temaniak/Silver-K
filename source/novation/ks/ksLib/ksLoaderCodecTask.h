#pragma once
#include <cstdint>
namespace ks {
// Firmware-update LoaderCodec policy. Logical data and deferred effects; CPU/stack/flags projection remains in the temporary compatibility bridge.
class LoaderCodecTask final {
public:
 enum class Kind { Prefix, Word, Long, Block, Group };enum class Stage { ReadStart, CompareStart, RepeatStart, BindPrefix, BindPrefixCount, ReadPrefixData, ReadPrefixExpected, ComparePrefix, RestartPrefix, TickPrefixCount, RepeatPrefix, ReturnPrefix, ReadWordLow, BindWordLow, ReadWordHigh, BindWordHigh, ClearWordHighBit, JoinWord, ReturnWord, ReadLongLow, SaveLongLow, ReadLongHigh, BindLongHigh, RestoreLongLow, ShiftLongLowOne, ShiftLongLowTwo, JoinLongOne, JoinLongTwo, ReadLongTop, BindTopWeight, MultiplyLongTop, RetainLongTop, ClearLongTopLow, MergeLongTop, ReturnLong, ClearBlockSum, BindBlockCount, ReadBlockGroup, TickBlockCount, RepeatBlock, ReturnBlock, ReadGroupLead, BindGroupCarry, BindGroupWeight, ReadGroupWord, MultiplyGroupWord, MergeGroupCarry, StoreGroupWord, NextGroupDestination, RetainGroupCarry, ClearSampleHigh, AddSampleSum, ShiftGroupWeightOne, ShiftGroupWeightTwo, CompareGroupWeight, RepeatGroupWord, ReturnGroup, AwaitChild, Complete };
 enum class Read { Prefix };enum class Write { Sample };uint8_t expected()const{return expected_;}uint16_t word()const{return word_;}uint32_t payload()const{return payload_;}uint32_t destination()const{return destination_;}uint32_t sum()const{return sum_;}
 explicit LoaderCodecTask(Kind kind,uint16_t word=0,uint32_t destination=0,uint32_t sum=0){word_=word;destination_=destination;sum_=sum;switch(kind){case Kind::Prefix:stage_=Stage::ReadStart;break;case Kind::Word:stage_=Stage::ReadWordLow;break;case Kind::Long:stage_=Stage::ReadLongLow;break;case Kind::Block:stage_=Stage::ClearBlockSum;break;case Kind::Group:stage_=Stage::ReadGroupLead;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint8_t byte,uint32_t payload,uint32_t sum,uint32_t destination){if(!awaitingChild())return false;byte_=byte;if(resume_==Stage::SaveLongLow||resume_==Stage::BindLongHigh||resume_==Stage::BindGroupCarry||resume_==Stage::MultiplyGroupWord)word_=uint16_t(payload);if(resume_==Stage::TickBlockCount){sum_=sum;destination_=destination;}stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ReadStart:case Stage::ReadPrefixData:case Stage::ReadWordLow:case Stage::ReadWordHigh:case Stage::ReadLongTop:return 20; case Stage::BindPrefix:return 9; case Stage::ReadPrefixExpected:return 8; case Stage::ReturnPrefix:case Stage::ReturnWord:case Stage::ReturnLong:case Stage::ReturnBlock:case Stage::ReturnGroup:return 17; case Stage::ReadLongLow:case Stage::ReadLongHigh:case Stage::MultiplyLongTop:case Stage::ReadBlockGroup:case Stage::ReadGroupLead:case Stage::ReadGroupWord:return 15; case Stage::SaveLongLow:case Stage::RestoreLongLow:return 11; case Stage::BindGroupWeight:case Stage::CompareGroupWeight:return 6; case Stage::MultiplyGroupWord:return 23; case Stage::StoreGroupWord:return 3+store; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::RepeatStart:stage_=byte_!=0xf0?Stage::ReadStart:Stage::BindPrefix;break;
case Stage::BindPrefix:prefix_=0;next();break;
case Stage::BindPrefixCount:remaining_=6;next();break;
case Stage::ReadPrefixExpected:expected_=uint8_t(read(Read::Prefix,prefix_++));next();break;
case Stage::RestartPrefix:stage_=byte_!=expected_?Stage::ReadStart:Stage::TickPrefixCount;break;
case Stage::TickPrefixCount:--remaining_;next();break;
case Stage::RepeatPrefix:stage_=remaining_?Stage::ReadPrefixData:Stage::ReturnPrefix;break;
case Stage::BindWordLow:word_=(word_&0xff00u)|byte_;next();break;
case Stage::BindWordHigh:word_=(uint16_t(byte_)<<8)|(word_&255);next();break;
case Stage::ClearWordHighBit:word_=(word_&0xff00u)|uint8_t(uint8_t(word_)<<1);next();break;
case Stage::JoinWord:word_>>=1;next();break;
case Stage::SaveLongLow:saved_=word_;next();break;
case Stage::BindLongHigh:payload_=(payload_&0xffffu)|(uint32_t(word_)<<16);next();break;
case Stage::RestoreLongLow:payload_=(payload_&0xffff0000u)|saved_;next();break;
case Stage::ShiftLongLowOne:payload_=(payload_&0xffff0000u)|uint16_t(uint16_t(payload_)<<1);next();break;
case Stage::ShiftLongLowTwo:payload_=(payload_&0xffff0000u)|uint16_t(uint16_t(payload_)<<1);next();break;
case Stage::JoinLongOne:payload_>>=1;next();break;
case Stage::JoinLongTwo:payload_>>=1;next();break;
case Stage::MultiplyLongTop:top_=uint16_t(byte_)*16;next();break;
case Stage::RetainLongTop:top_=uint16_t((uint16_t(uint8_t(top_))<<8)|uint8_t(top_));next();break;
case Stage::ClearLongTopLow:top_&=0xff00;next();break;
case Stage::MergeLongTop:payload_|=uint32_t(top_)<<16;next();break;
case Stage::ClearBlockSum:sum_=0;next();break;
case Stage::BindBlockCount:remaining_=word_;next();break;
case Stage::TickBlockCount:remaining_=uint16_t(remaining_-1);next();break;
case Stage::RepeatBlock:stage_=remaining_?Stage::ReadBlockGroup:Stage::ReturnBlock;break;
case Stage::BindGroupCarry:carry_=word_;next();break;
case Stage::BindGroupWeight:weight_=0x4000;next();break;
case Stage::MultiplyGroupWord:payload_=uint32_t(word_)*weight_;next();break;
case Stage::MergeGroupCarry:payload_|=carry_;next();break;
case Stage::StoreGroupWord:write(Write::Sample,destination_,uint16_t(payload_));next();break;
case Stage::NextGroupDestination:destination_+=2;next();break;
case Stage::RetainGroupCarry:carry_=uint16_t(payload_>>16);next();break;
case Stage::ClearSampleHigh:payload_&=0xffff;next();break;
case Stage::AddSampleSum:sum_+=payload_;next();break;
case Stage::ShiftGroupWeightOne:weight_>>=1;next();break;
case Stage::ShiftGroupWeightTwo:weight_>>=1;next();break;
case Stage::RepeatGroupWord:stage_=weight_!=1?Stage::ReadGroupWord:Stage::ReturnGroup;break;
case Stage::ReadStart:await(Stage::CompareStart);break;
case Stage::ReadPrefixData:await(Stage::ReadPrefixExpected);break;
case Stage::ReturnPrefix:stage_=Stage::Complete;break;
case Stage::ReadWordLow:await(Stage::BindWordLow);break;
case Stage::ReadWordHigh:await(Stage::BindWordHigh);break;
case Stage::ReturnWord:stage_=Stage::Complete;break;
case Stage::ReadLongLow:await(Stage::SaveLongLow);break;
case Stage::ReadLongHigh:await(Stage::BindLongHigh);break;
case Stage::ReadLongTop:await(Stage::BindTopWeight);break;
case Stage::ReturnLong:stage_=Stage::Complete;break;
case Stage::ReadBlockGroup:await(Stage::TickBlockCount);break;
case Stage::ReturnBlock:stage_=Stage::Complete;break;
case Stage::ReadGroupLead:await(Stage::BindGroupCarry);break;
case Stage::ReadGroupWord:await(Stage::MultiplyGroupWord);break;
case Stage::ReturnGroup:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t payload_=0,sum_=0,destination_=0;uint16_t word_=0,saved_=0,top_=0,carry_=0,weight_=0,remaining_=0;uint8_t byte_=0,expected_=0,prefix_=0;
};
}
