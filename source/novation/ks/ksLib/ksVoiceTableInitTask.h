#pragma once
#include <cstdint>
namespace ks {
// Default state and calibrated note bounds for the fourteen physical voices.
// External oscillator mapping is observed once per voice, at its deadline.
class VoiceTableInitTask final {public:
 enum class Stage{ClearIndex,SetCount,BindSlots,DefaultNote,DefaultDistance,ClearCounter,PublishNote,PublishTransposed,PublishDistance,PublishCounter,PublishReleased,ReadMapping,PublishMapping,AdmitLowerSign,SetNegativeLower,SkipLower,SetPositiveLower,CompareLowerSplit,AdmitLowerSplit,SetWideLower,ComputeLower,PublishLower,ObserveUpperSign,AdmitUpperSign,SetPositiveUpper,SkipUpper,SetNegativeUpper,CompareUpperSplit,AdmitUpperSplit,SetWideUpper,ComputeUpper,PublishUpper,TickIndex,MaskIndex,AdvanceSlot,TickCount,AdmitCount,Finish,Complete};
 enum class Field{Note,Transposed,Distance,Counter,Released,Mapping,Lower,Upper};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
 unsigned index()const{return index_;}unsigned remaining()const{return remaining_;}
 uint8_t mapping()const{return mapping_;}uint8_t lowerBase()const{return lowerBase_;}uint8_t upperBase()const{return upperBase_;}
 uint8_t lower()const{return lower_;}uint8_t upper()const{return upper_;}
 bool publication()const{switch(stage_){case Stage::PublishNote:case Stage::PublishTransposed:case Stage::PublishDistance:case Stage::PublishCounter:case Stage::PublishReleased:case Stage::PublishMapping:case Stage::PublishLower:case Stage::PublishUpper:return true;default:return false;}}
 Field publicationField()const{switch(stage_){case Stage::PublishNote:return Field::Note;case Stage::PublishTransposed:return Field::Transposed;case Stage::PublishDistance:return Field::Distance;case Stage::PublishCounter:return Field::Counter;case Stage::PublishReleased:return Field::Released;case Stage::PublishMapping:return Field::Mapping;case Stage::PublishLower:return Field::Lower;default:return Field::Upper;}}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::BindSlots:case Stage::AdvanceSlot:return 9;case Stage::ReadMapping:return 15;case Stage::PublishCounter:return 21;case Stage::PublishNote:case Stage::PublishTransposed:case Stage::PublishDistance:case Stage::PublishReleased:case Stage::PublishMapping:case Stage::PublishLower:case Stage::PublishUpper:return 12+store;case Stage::AdmitCount:return 8;case Stage::Finish:return 17;case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationField(),index_):3)){if(cost>available-used)break;auto next=Stage(unsigned(stage_)+1);
 switch(stage_){case Stage::ClearIndex:index_=0;break;case Stage::SetCount:remaining_=14;break;
 case Stage::PublishNote:write(Field::Note,index_,60);break;case Stage::PublishTransposed:write(Field::Transposed,index_,60);break;case Stage::PublishDistance:write(Field::Distance,index_,64);break;case Stage::PublishCounter:write(Field::Counter,index_,0);break;case Stage::PublishReleased:write(Field::Released,index_,0);break;
 case Stage::ReadMapping:mapping_=uint8_t(read(index_));break;case Stage::PublishMapping:write(Field::Mapping,index_,mapping_);break;
 case Stage::AdmitLowerSign:next=(mapping_&128)?Stage::SetNegativeLower:Stage::SetPositiveLower;break;
 case Stage::SetNegativeLower:lowerBase_=15;break;case Stage::SkipLower:next=Stage::ComputeLower;break;
 case Stage::SetPositiveLower:lowerBase_=39;break;case Stage::AdmitLowerSplit:next=mapping_<=7?Stage::ComputeLower:Stage::SetWideLower;break;case Stage::SetWideLower:lowerBase_=55;break;
 case Stage::ComputeLower:lower_=uint8_t(lowerBase_-mapping_);break;case Stage::PublishLower:write(Field::Lower,index_,lower_);break;
 case Stage::AdmitUpperSign:next=(mapping_&128)?Stage::SetNegativeUpper:Stage::SetPositiveUpper;break;
 case Stage::SetPositiveUpper:upperBase_=239;break;case Stage::SkipUpper:next=Stage::ComputeUpper;break;
 case Stage::SetNegativeUpper:upperBase_=215;break;case Stage::AdmitUpperSplit:next=int8_t(mapping_)>=-8?Stage::ComputeUpper:Stage::SetWideUpper;break;case Stage::SetWideUpper:upperBase_=199;break;
 case Stage::ComputeUpper:upper_=uint8_t(upperBase_-mapping_);break;case Stage::PublishUpper:write(Field::Upper,index_,upper_);break;
 case Stage::TickIndex:++index_;break;case Stage::MaskIndex:index_&=31;break;case Stage::TickCount:--remaining_;break;case Stage::AdmitCount:next=remaining_?Stage::PublishNote:Stage::Finish;break;
 default:break;}stage_=next;used+=cost;}return used;}
private:Stage stage_=Stage::ClearIndex;unsigned index_=0,remaining_=0;uint8_t mapping_=0,lowerBase_=0,upperBase_=0,lower_=0,upper_=0;
};}
