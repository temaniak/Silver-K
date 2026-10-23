#pragma once
#include <cstdint>
namespace ks {
// Diagnostic prefix,14-bit and32-bit encoding, seven-word packing and fixed32-group dump checksum. No instruction or CPU state in the codec policy.
class DiagnosticCodecTask final {
public:
 enum class Kind { Prefix, Word, Long, Block, Group, Sample, Tail };enum class Stage { BindPrefixStart, SendPrefixStart, BindPrefixData, BindPrefixCount, ReadPrefixByte, SendPrefixByte, DecrementPrefixCount, RepeatPrefix, ReturnPrefix, ShiftWordPayload, ClearWordLowHighBit, BindWordLow, SendWordLow, BindWordHigh, SendWordHigh, ReturnWord, SaveLongPayload, MaskLongLow, SendLongLow, RestoreLongPayload, BindLongShiftCount, ShiftLongPayload, DecrementLongShiftCount, RepeatLongShift, SaveLongHigh, MaskLongHigh, SendLongHigh, RestoreLongHigh, ShiftLongTopFirst, ShiftLongTopSecond, BindLongTop, BindLongTopByte, SendLongTop, ReturnLong, ClearBlockChecksum, BindBlockCount, PackBlockGroup, DecrementBlockCount, RepeatBlock, ReturnBlock, ClearGroupCarry, BindGroupWeight, PackGroupSample, ShiftGroupWeightFirst, ShiftGroupWeightSecond, TestGroupWeight, RepeatGroup, BindGroupTail, JoinGroupTail, ReadSample, ClearSampleHigh, AddSampleChecksum, MultiplySample, MergeSampleCarry, ShiftPackedFirst, ShiftPackedSecond, ClearPackedHighBit, ClearPackedLowHighBit, BindPackedLow, SendPackedLow, BindPackedHigh, SendPackedHigh, PublishGroupCarry, ReturnPacked, AwaitChild, Complete };
 enum class Read { Prefix, Sample };uint32_t source()const{return source_;}uint32_t sum()const{return sum_;}uint32_t packed()const{return packed_;}uint32_t payload()const{return payload_;}uint16_t sample()const{return sample_;}uint16_t weight()const{return weight_;}uint16_t carry()const{return carry_;}uint16_t word()const{return word_;}uint8_t byte()const{return byte_;}
 explicit DiagnosticCodecTask(Kind kind,uint32_t payload=0,uint32_t source=0,uint32_t sum=0,uint16_t weight=0,uint16_t carry=0){payload_=payload;packed_=payload;source_=source;sum_=sum;weight_=weight;carry_=carry;switch(kind){case Kind::Prefix:stage_=Stage::BindPrefixStart;break;case Kind::Word:stage_=Stage::ShiftWordPayload;break;case Kind::Long:stage_=Stage::SaveLongPayload;break;case Kind::Block:stage_=Stage::ClearBlockChecksum;break;case Kind::Group:stage_=Stage::ClearGroupCarry;break;case Kind::Sample:stage_=Stage::ReadSample;break;case Kind::Tail:stage_=Stage::ShiftPackedFirst;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint32_t source,uint32_t sum,uint16_t carry,uint32_t packed){if(!awaitingChild())return false;if(resume_==Stage::DecrementBlockCount||resume_==Stage::ShiftGroupWeightFirst){source_=source;sum_=sum;carry_=carry;packed_=packed;}stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::SendPrefixStart:case Stage::SendPrefixByte:case Stage::SendWordLow:case Stage::SendWordHigh:case Stage::SaveLongPayload:case Stage::RestoreLongPayload:case Stage::SaveLongHigh:case Stage::RestoreLongHigh:case Stage::SendLongTop:case Stage::SendPackedLow:case Stage::SendPackedHigh:return 20; case Stage::BindPrefixData:return 9; case Stage::ReadPrefixByte:return 8; case Stage::ReturnPrefix:case Stage::ReturnWord:case Stage::ReturnLong:case Stage::ReturnBlock:case Stage::ReturnPacked:return 17; case Stage::MaskLongLow:case Stage::MaskLongHigh:case Stage::BindBlockCount:case Stage::BindGroupWeight:return 6; case Stage::SendLongLow:case Stage::SendLongHigh:case Stage::PackBlockGroup:case Stage::PackGroupSample:return 15; case Stage::ReadSample:return 5+store; case Stage::MultiplySample:return 23; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::BindPrefixData:offset_=0;next();break;
case Stage::BindPrefixCount:remaining_=6;next();break;
case Stage::ReadPrefixByte:byte_=uint8_t(read(Read::Prefix,offset_++));next();break;
case Stage::DecrementPrefixCount:--remaining_;next();break;
case Stage::RepeatPrefix:stage_=remaining_?Stage::ReadPrefixByte:Stage::ReturnPrefix;break;
case Stage::ShiftWordPayload:payload_=uint16_t(payload_<<1);next();break;
case Stage::ClearWordLowHighBit:payload_=uint16_t((payload_&0xff00)|((payload_&255)>>1));next();break;
case Stage::BindWordLow:byte_=uint8_t(payload_);next();break;
case Stage::BindWordHigh:byte_=uint8_t(payload_>>8);next();break;
case Stage::MaskLongLow:word_=uint16_t(payload_)&0x3fff;next();break;
case Stage::BindLongShiftCount:remaining_=14;next();break;
case Stage::ShiftLongPayload:payload_>>=1;next();break;
case Stage::DecrementLongShiftCount:--remaining_;next();break;
case Stage::RepeatLongShift:stage_=remaining_?Stage::ShiftLongPayload:Stage::SaveLongHigh;break;
case Stage::MaskLongHigh:word_=uint16_t(payload_)&0x3fff;next();break;
case Stage::ShiftLongTopFirst:payload_<<=1;next();break;
case Stage::ShiftLongTopSecond:payload_<<=1;next();break;
case Stage::BindLongTop:payload_=uint16_t(payload_>>16);next();break;
case Stage::BindLongTopByte:byte_=uint8_t(payload_);next();break;
case Stage::ClearBlockChecksum:sum_=0;next();break;
case Stage::BindBlockCount:remaining_=32;next();break;
case Stage::DecrementBlockCount:--remaining_;next();break;
case Stage::RepeatBlock:stage_=remaining_?Stage::PackBlockGroup:Stage::ReturnBlock;break;
case Stage::ClearGroupCarry:carry_=0;next();break;
case Stage::BindGroupWeight:weight_=1;next();break;
case Stage::ShiftGroupWeightFirst:weight_=uint16_t(weight_<<1);next();break;
case Stage::ShiftGroupWeightSecond:weight_=uint16_t(weight_<<1);next();break;
case Stage::RepeatGroup:stage_=(weight_>>8)!=0x40?Stage::PackGroupSample:Stage::BindGroupTail;break;
case Stage::BindGroupTail:packed_=(packed_&0xffff0000u)|carry_;next();break;
case Stage::JoinGroupTail:stage_=Stage::ShiftPackedFirst;break;
case Stage::ReadSample:sample_=uint16_t(read(Read::Sample,source_));source_+=2;next();break;
case Stage::AddSampleChecksum:sum_+=sample_;next();break;
case Stage::MultiplySample:packed_=uint32_t(sample_)*weight_;next();break;
case Stage::MergeSampleCarry:packed_=(packed_&0xffff0000u)|uint16_t(packed_|carry_);next();break;
case Stage::ShiftPackedFirst:packed_<<=1;next();break;
case Stage::ShiftPackedSecond:packed_<<=1;next();break;
case Stage::ClearPackedHighBit:packed_=(packed_&0xffff0000u)|uint16_t(uint16_t(packed_)>>1);next();break;
case Stage::ClearPackedLowHighBit:packed_=(packed_&0xffffff00u)|((packed_&255)>>1);next();break;
case Stage::BindPackedLow:byte_=uint8_t(packed_);next();break;
case Stage::BindPackedHigh:byte_=uint8_t(packed_>>8);next();break;
case Stage::PublishGroupCarry:carry_=uint16_t(packed_>>16);next();break;
case Stage::SendPrefixStart:await(Stage::BindPrefixData);break;
case Stage::SendPrefixByte:await(Stage::DecrementPrefixCount);break;
case Stage::ReturnPrefix:stage_=Stage::Complete;break;
case Stage::SendWordLow:await(Stage::BindWordHigh);break;
case Stage::SendWordHigh:await(Stage::ReturnWord);break;
case Stage::ReturnWord:stage_=Stage::Complete;break;
case Stage::SendLongLow:await(Stage::RestoreLongPayload);break;
case Stage::SendLongHigh:await(Stage::RestoreLongHigh);break;
case Stage::SendLongTop:await(Stage::ReturnLong);break;
case Stage::ReturnLong:stage_=Stage::Complete;break;
case Stage::PackBlockGroup:await(Stage::DecrementBlockCount);break;
case Stage::ReturnBlock:stage_=Stage::Complete;break;
case Stage::PackGroupSample:await(Stage::ShiftGroupWeightFirst);break;
case Stage::SendPackedLow:await(Stage::BindPackedHigh);break;
case Stage::SendPackedHigh:await(Stage::PublishGroupCarry);break;
case Stage::ReturnPacked:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t payload_=0,source_=0,sum_=0,packed_=0;uint16_t sample_=0,weight_=0,carry_=0,word_=0;uint8_t remaining_=0,offset_=0,byte_=0;
};
}
