#pragma once
#include "ksStatePackFieldsTask.h"
#include <array>
#include <cstdint>
namespace ks {
// Pack one indexed descriptor or all six descriptors into the current patch.
// The external immutable descriptor selects a named native packing recipe.
class StatePackRefreshTask final {
public:enum class Entry {All,One};enum class Stage {BeginAll,DispatchEntry,AddTargetBase,WritePacked,NextGroup,CompareGroup,AdmitGroup,ReturnAll,RetainGroup,PreviousGroup,ScaleGroup,NormalizeWord,NormalizeIndex,ReadRecipe,ReadBase,DispatchRecipe,RestoreGroup,ReturnEntry,AwaitRecipe,Complete};enum class Read {Recipe,Base};
 StatePackRefreshTask(Entry e,uint8_t group):all_(e==Entry::All),phase_(all_?Stage::BeginAll:Stage::RetainGroup),group_(group){}
 Stage stage()const{return phase_;}bool complete()const{return phase_==Stage::Complete;}bool awaitingRecipe()const{return phase_==Stage::AwaitRecipe;}StatePackFieldsTask::Kind recipe()const{return recipe_;}unsigned descriptorOffset()const{return uint8_t(uint8_t(group_-1)<<2);}uint8_t value()const{return value_;}uint32_t field()const{return field_;}uint8_t group()const{return group_;}
 bool resumeRecipe(uint8_t value,uint32_t field){if(!awaitingRecipe())return false;value_=value;field_=field;phase_=Stage::RestoreGroup;return true;}
 unsigned nextClocks(unsigned storeClocks=3)const{constexpr std::array<unsigned,18> costs{{3,15,3,6,3,3,3,17,11,3,3,3,3,21,18,15,11,17}};return unsigned(phase_)>=costs.size()?0:phase_==Stage::WritePacked?costs[unsigned(phase_)]-3+storeClocks:costs[unsigned(phase_)];}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned storeClocks=3){unsigned used=0;while(const auto cost=nextClocks(storeClocks)){if(cost>available-used)break;switch(phase_){case Stage::BeginAll:group_=1;next();break;case Stage::DispatchEntry:phase_=Stage::RetainGroup;break;case Stage::WritePacked:write(field_,value_);next();break;case Stage::NextGroup:++group_;next();break;case Stage::AdmitGroup:phase_=group_<=6?Stage::DispatchEntry:Stage::ReturnAll;break;
 case Stage::ReadRecipe:recipe_=StatePackFieldsTask::Kind(read(Read::Recipe));next();break;case Stage::ReadBase:(void)read(Read::Base);next();break;case Stage::DispatchRecipe:phase_=Stage::AwaitRecipe;break;case Stage::ReturnEntry:phase_=all_?Stage::AddTargetBase:Stage::Complete;break;case Stage::ReturnAll:phase_=Stage::Complete;break;default:next();break;}used+=cost;}return used;}
private:void next(){phase_=Stage(unsigned(phase_)+1);}bool all_;Stage phase_;uint8_t group_,value_=0;uint32_t field_=0;StatePackFieldsTask::Kind recipe_=StatePackFieldsTask::Kind::Oscillators;
};}
