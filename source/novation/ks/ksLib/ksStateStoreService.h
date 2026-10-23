#pragma once
#include "ksStateMultiSerializeTask.h"
#include "ksStatePackRefreshTask.h"
#include "ksStatePatchNameTask.h"
#include "ksPartControlTasks.h"
#include <optional>
#include <stdexcept>
namespace ks {
// Closed store preparation: serialization, original byte packing and live
// name copies. Flash programming and upper store UI orchestration are separate.
class StateStoreService final {
public:
 enum class Kind {MultiSerialize,PackAll,PackOne,Oscillators,Unison,Lfo,Filter,Envelope,ProgramMode,ReadPatchName,ReadMultiName,ReadProvidedName,WritePatchName,WriteMultiName,WriteProvidedName,WriteSelectedName,ReadSelectedName};
 enum class Component {MultiSerialize,PartReference,PackAll,PackOne,Oscillators,Unison,Lfo,Filter,Envelope,ProgramMode,Name};
 struct Input{uint32_t source=0;uint8_t group=1;};
 explicit StateStoreService(Kind kind):StateStoreService(kind,Input{}){}
 StateStoreService(Kind kind,Input input):kind_(kind),base_(input.source),storage_(input.source){
  if(kind==Kind::MultiSerialize){multi_.emplace();select(Component::MultiSerialize);}
  else if(kind==Kind::PackAll||kind==Kind::PackOne)startPack(kind==Kind::PackAll,input.group);
  else if(kind<=Kind::ProgramMode)startRecipe(StatePackFieldsTask::Kind(unsigned(kind)-3));
  else {name_.emplace(StatePatchNameTask::Entry(unsigned(kind)-9));select(Component::Name);}
 }
 Kind kind()const{return kind_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return complete_;}
 uint8_t value()const{return value_;}uint8_t contributor()const{return contributor_;}bool carry()const{return carry_;}
 uint32_t base()const{return base_;}uint32_t original()const{return original_;}uint32_t storage()const{return storage_;}uint16_t lastProgram()const{return lastProgram_;}uint32_t field()const{return field_;}
 const StateMultiSerializeTask*multiTask()const{return component_==Component::MultiSerialize&&multi_?&*multi_:nullptr;}
 const StatePackRefreshTask*packTask()const{return(component_==Component::PackAll||component_==Component::PackOne)&&pack_?&*pack_:nullptr;}
 const StatePackFieldsTask*recipeTask()const{return component_>=Component::Oscillators&&component_<=Component::ProgramMode&&recipe_?&*recipe_:nullptr;}
 const PartLookupTask*partTask()const{return component_==Component::PartReference&&part_?&*part_:nullptr;}
 const StatePatchNameTask*nameTask()const{return name_?&*name_:nullptr;}
 static bool dataSpan(uint32_t raw,unsigned size){const auto a=raw&0xffffff;return size&&((a>=0xb8e8&&a<0x200000&&size<=0x200000-a)||(a>=0xa0b8e8&&a<0xc00000&&size<=0xc00000-a)||(a>=0xffdc00&&a<0xfff000&&size<=0xfff000-a));}

 unsigned nextClocks(int previousWrite=-1)const{
  if(complete_)return 0;
  if(const auto*t=multiTask())return t->nextClocks(byteClocks(multiTarget(),previousWrite));
  if(const auto*t=packTask())return t->nextClocks(byteClocks(base_+field_,previousWrite));
  if(const auto*t=recipeTask())return t->nextClocks();
  if(const auto*t=partTask())return t->stage()==PartLookupTask::Stage::Complete?17:t->nextClocks();
  return name_->nextClocks(byteClocks(nameTarget(),previousWrite));
 }
 template<class Reader,class Writer,class Resolver>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,Resolver&&resolve,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;
  const auto word=[&](uint32_t address){const unsigned gap=(address&1)?3:0;const auto high=uint8_t(read(address&0xffffff,cost-gap));return uint16_t((unsigned(high)<<8)|uint8_t(read((address+1)&0xffffff,cost)));};
  const auto longword=[&](uint32_t address){const auto high=uint8_t(read(address,cost-3));const auto next=uint8_t(read(address+1,cost-3));const auto low=uint8_t(read(address+2,cost));return(uint32_t(high)<<24)|(uint32_t(next)<<16)|(unsigned(low)<<8)|uint8_t(read(address+3,cost));};
  const auto publishLong=[&](uint32_t address,uint32_t value){write(address,uint8_t(value>>24),cost-3);write(address+1,uint8_t(value>>16),cost-3);write(address+2,uint8_t(value>>8),cost);write(address+3,uint8_t(value),cost);};
  if(multiTask()){
   using T=StateMultiSerializeTask;auto&task=*multi_;const auto target=multiTarget();
   task.advanceBefore(cost,[&](T::Read what)->uint32_t{switch(what){
    case T::Read::Original:return original_=longword(0x167bc);
    case T::Read::GlobalField:return read(0x168fc + task.sourceGlobalOffset(),cost);
    case T::Read::Selection:return read(task.selection()?0x167d3:0x167d2,cost);
    case T::Read::Name:return read((partSource_+9+task.nameOffset())&0xffffff,cost-2);
    case T::Read::Program:return lastProgram_=word(0x167b0 + task.part()*2);
   }return 0;},[&](T::Write what,uint32_t value){if(what==T::Write::SelectCurrent||what==T::Write::SelectOriginal)publishLong(0x167bc,what==T::Write::SelectCurrent?0x169fc + task.part()*256:original_);else write(target,uint8_t(value),cost);},byteClocks(target,previousWrite));
   if(task.awaitingChild()){
    if(task.child()==T::Child::PartReference){part_.emplace(PartLookupTask::controlsFor(uint8_t(task.part()+1)));select(Component::PartReference);}
    else startPack(true,1);
   }else if(task.complete())complete_=true;
  }else if(partTask()){
   if(part_->stage()==PartLookupTask::Stage::Complete){partSource_=0x174ae + part_->offset();part_.reset();multi_->resumeChild();select(Component::MultiSerialize);}
   else part_->advanceBefore(cost,[](PartLookupTask::Layout){return uint8_t(0);});
  }else if(packTask()){
   using T=StatePackRefreshTask;auto&task=*pack_;
   task.advanceBefore(cost,[&](T::Read what)->uint32_t{
    if(what==T::Read::Base){base_=longword(0x167bc);if(!dataSpan(base_,228))throw std::runtime_error("Unsupported native store packing resource");return base_;}
    return unsigned(resolve(longword(71306+task.descriptorOffset())));
   },[&](uint32_t field,uint8_t value){write((base_+field)&0xffffff,value,cost);},byteClocks(base_+field_,previousWrite));
   if(task.awaitingRecipe())startRecipe(task.recipe());
   else if(task.complete()){
    pack_.reset();if(multi_){multi_->resumeChild();select(Component::MultiSerialize);}else complete_=true;
   }else select(packComponent(task.stage()));
  }else if(recipeTask()){
   using T=StatePackFieldsTask;const auto stage=recipe_->stage();const auto before=recipe_->value();
   if(stage==T::Stage::RotateBefore||stage==T::Stage::RotateBeforeExtra||stage==T::Stage::RotateAfter||stage==T::Stage::RotateFinalExtra){const auto n=recipe_->rotation();carry_=bool(before&(1u<<(recipe_->rotateLeft()?8-n:n-1)));}
   if(stage==T::Stage::ShiftSignedLast)carry_=bool(contributor_&0x80);
   recipe_->advanceBefore(cost,[&](){const auto v=uint8_t(read((base_+recipe_->sourceOffset())&0xffffff,cost));if(stage==T::Stage::ReadContributor)contributor_=v;return v;});
   if(stage==T::Stage::ShiftSignedFirst)contributor_=uint8_t(contributor_*4);
   if(stage==T::Stage::ShiftSignedLast)contributor_=uint8_t(contributor_*2);
   if(stage==T::Stage::MaskSigned)contributor_&=127;
   if(recipe_->complete()){value_=recipe_->value();field_=recipe_->targetOffset();recipe_.reset();if(pack_){pack_->resumeRecipe(value_,field_);select(Component::PackOne);}else complete_=true;}
  }else{
   using T=StatePatchNameTask;using S=T::Stage;const auto stage=name_->stage();const auto target=nameTarget();
   name_->advanceBefore(cost,[&](T::Read what)->uint32_t{
    if(what==T::Read::CurrentBase||what==T::Read::SelectedBase){storage_=longword(what==T::Read::CurrentBase?0x167bc:0x1748e);const unsigned offset=what==T::Read::CurrentBase?151:0;if(!dataSpan(storage_+offset,16))throw std::runtime_error("Unsupported native store name resource");return storage_;}
    return read((what==T::Read::Name?storage_+name_->byteOffset():0x1733c + name_->byteOffset())&0xffffff,cost-2);
   },[&](T::Write,uint8_t v){write(target,v,cost);},byteClocks(target,previousWrite));
   if(stage==S::AddPatchName||stage==S::AddWritePatch)storage_+=151;
   if(stage==S::SetReadMulti||stage==S::SetWriteMulti)storage_=0x169fc;
   if(stage==S::AddReadMulti||stage==S::AddWriteMulti)storage_+=192;
   if(name_->complete())complete_=true;
  }
  return cost;
 }
private:
 static unsigned byteClocks(uint32_t address,int previous){address&=0xffffff;return address<0x400000&&!(previous&1)&&address==(uint32_t(previous)|1u)?0:3;}
 static Component packComponent(StatePackRefreshTask::Stage stage){using S=StatePackRefreshTask::Stage;switch(stage){case S::BeginAll:case S::DispatchEntry:case S::AddTargetBase:case S::WritePacked:case S::NextGroup:case S::CompareGroup:case S::AdmitGroup:case S::ReturnAll:return Component::PackAll;default:return Component::PackOne;}}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void startPack(bool all,uint8_t group){pack_.emplace(all?StatePackRefreshTask::Entry::All:StatePackRefreshTask::Entry::One,group);select(all?Component::PackAll:Component::PackOne);}
 void startRecipe(StatePackFieldsTask::Kind kind){contributor_=0;recipe_.emplace(kind);select(Component(unsigned(kind)+4));}
 uint32_t multiTarget()const{using S=StateMultiSerializeTask::Stage;const auto&t=*multi_;switch(t.stage()){case S::WriteGlobalField:return 0x169fc + t.targetGlobalOffset();case S::WriteSelection:return 0x169fc + 189+t.selection();case S::WriteName:return 0x169fc + t.part()*256+179+t.nameOffset();default:return 0x169fc + t.part()*256+(t.stage()==S::WriteBank?187:188);}}
 uint32_t nameTarget()const{using S=StatePatchNameTask::Stage;return(name_->stage()==S::WriteName?storage_+name_->byteOffset():0x1733c + name_->byteOffset())&0xffffff;}
 Kind kind_;Component component_=Component::MultiSerialize;uint32_t components_=0;bool complete_=false,carry_=false;
 uint32_t base_=0,storage_=0,original_=0,partSource_=0,field_=0;uint16_t lastProgram_=0;uint8_t value_=0,contributor_=0;
 std::optional<StateMultiSerializeTask>multi_;std::optional<StatePackRefreshTask>pack_;std::optional<StatePackFieldsTask>recipe_;std::optional<PartLookupTask>part_;std::optional<StatePatchNameTask>name_;
};
}
