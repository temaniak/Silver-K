#pragma once
#include "ksPanelMatrixTask.h"
#include "ksPanelInputTask.h"
#include <optional>

namespace ks {
// One owned graph covers six sampled rows, every mapped event publication and
// initialization's final queue reset. Children return to typed owners only.
class PanelScanService final {
public:
 enum class Kind { Initialize, Scan, ButtonPush };
 enum class Component { Matrix, Push, Reset };
 explicit PanelScanService(Kind kind,uint16_t event=0):kind_(kind){
  if(kind==Kind::ButtonPush){push_.emplace(PanelInputTask::Kind::ButtonPush,event);component_=Component::Push;components_=2;}
  else matrix_.emplace(kind==Kind::Initialize?PanelMatrixTask::Kind::Initialize:PanelMatrixTask::Kind::Scan);
 }
 Kind kind()const{return kind_;}Component component()const{return component_;}
 bool complete()const{return complete_;}uint32_t components()const{return components_;}
 uint16_t event()const{return event_;}uint16_t producer()const{return producer_;}
 uint16_t eventIndex()const{return eventIndex_;}uint8_t key()const{return key_;}uint8_t mapping()const{return mapping_;}
 bool lastRowUnchanged()const{return lastRowUnchanged_;}
 bool atMatrix(PanelMatrixTask::Stage stage)const{return component_==Component::Matrix&&(scan_?scan_->stage()==stage:matrix_&&matrix_->stage()==stage);}
 bool atPush(PanelInputTask::Stage stage)const{return component_==Component::Push&&push_&&push_->stage()==stage;}
 bool atReset(PanelInputTask::Stage stage)const{return component_==Component::Reset&&reset_&&reset_->stage()==stage;}
 unsigned nextClocks()const{
  if(complete_)return 0;
  if(component_==Component::Matrix){const auto&t=scan_?*scan_:*matrix_;return t.nextClocks(matrixStore(t.stage()));}
  const auto&t=component_==Component::Push?*push_:*reset_;return t.nextClocks(t.eventIndex()&1?6:3);
 }
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write){
  const auto cost=nextClocks();if(!cost||cost>available)return 0;
  const auto readWord=[&](uint32_t address){const auto high=uint8_t(read(address,cost));return uint16_t((unsigned(high)<<8)|uint8_t(read(address+1,cost)));};
  const auto writeWord=[&](uint32_t address,uint16_t value){const auto gap=(address&1)?3u:0u;write(address,uint8_t(value>>8),cost-gap);write(address+1,uint8_t(value),cost);};
  if(component_==Component::Matrix){
   using T=PanelMatrixTask;auto&t=scan_?*scan_:*matrix_;
   t.advanceBefore(cost,[&](T::Read field){return read(field==T::Read::Previous?0x15400+t.row():0xffff51,cost);},
    [&](T::Write field,uint8_t value){write(field==T::Write::History?0x15400+t.row():field==T::Write::Mux?0xffff62:0xfffeb1,value,cost);},matrixStore(t.stage()));
   if(t.awaitingChild()){
    if(t.resumeStage()==T::Stage::ResetQueue){scan_.emplace(T::Kind::Scan);select(Component::Matrix);}
    else if(t.resumeStage()==T::Stage::ReturnInitial){reset_.emplace(PanelInputTask::Kind::ButtonReset);select(Component::Reset);}
    else {push_.emplace(PanelInputTask::Kind::ButtonPush,uint16_t((unsigned(t.pressed())<<8)|t.key()));select(Component::Push);}
   }else if(t.complete()){
    if(scan_){lastRowUnchanged_=t.previous()==t.sample();scan_.reset();matrix_->resumeChild();select(Component::Matrix);}
    else {lastRowUnchanged_=t.previous()==t.sample();complete_=true;}
   }
  }else{
   using T=PanelInputTask;const bool pushing=component_==Component::Push;auto&t=pushing?*push_:*reset_;
   t.advanceBefore(cost,[&](T::Read field)->uint16_t{
    switch(field){case T::Read::Producer:return readWord(0x1537c);case T::Read::Consumer:return readWord(0x1537e);
    case T::Read::Mapping:return read(0xb8e8+t.key(),cost);case T::Read::Event:return readWord(0x15380+t.eventIndex());}return 0;
   },[&](T::Write field,uint16_t value){writeWord(field==T::Write::Producer?0x1537c:field==T::Write::Consumer?0x1537e:0x15380+t.eventIndex(),value);},t.eventIndex()&1?6:3);
   if(t.complete()){
    if(pushing){event_=t.event();producer_=t.producer();eventIndex_=t.eventIndex();key_=t.key();mapping_=t.mapping();push_.reset();}
    else reset_.reset();
    if(kind_==Kind::ButtonPush)complete_=true;
    else {(scan_?*scan_:*matrix_).resumeChild();select(Component::Matrix);}
   }
  }
  return cost;
 }
private:
 static unsigned matrixStore(PanelMatrixTask::Stage stage){return stage==PanelMatrixTask::Stage::WriteHistory?3u:1u;}
 void select(Component component){component_=component;components_|=1u<<unsigned(component);}
 Kind kind_;Component component_=Component::Matrix;uint32_t components_=1;bool complete_=false,lastRowUnchanged_=true;
 uint16_t event_=0,producer_=0,eventIndex_=0;uint8_t key_=0,mapping_=0;
 std::optional<PanelMatrixTask>matrix_,scan_;std::optional<PanelInputTask>push_,reset_;
};
}
