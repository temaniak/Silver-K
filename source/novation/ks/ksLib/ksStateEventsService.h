#pragma once
#include "ksPanelInputTask.h"
#include "ksKeyboardEventsTask.h"
#include "ksPanelAnalogTask.h"
#include <optional>

namespace ks {
// Complete event consumption and analog conversion graphs. Children exchange
// logical controller values; only the caller owns the external return edge.
class StateEventsService final {
public:
 enum class Kind { ButtonPop, ControlPop, ControlPeek, KeyboardPop, Convert, Scale, Square, Coefficients, Range };
 enum class Component { Buttons, Controls, Keyboard, Convert, Scale, Square, Coefficients, Range };
 struct Input { uint8_t value=0,selector=0;uint16_t bias=0;uint32_t history=0,coefficients=0; };
 explicit StateEventsService(Kind kind):StateEventsService(kind,Input{}){}
 StateEventsService(Kind kind,Input input):kind_(kind),bias_(input.bias),value_(input.value),selector_(input.selector),history_(input.history&0xffffff),coefficients_(input.coefficients&0xffffff){
  using P=PanelInputTask;using A=PanelAnalogTask;
  switch(kind){
  case Kind::ButtonPop:panel_.emplace(P::Kind::ButtonPop);select(Component::Buttons);break;
  case Kind::ControlPop:case Kind::ControlPeek:panel_.emplace(kind==Kind::ControlPop?P::Kind::ControlPop:P::Kind::ControlPeek);select(Component::Controls);break;
  case Kind::KeyboardPop:keyboard_.emplace(KeyboardEventsTask::Kind::Pop);select(Component::Keyboard);break;
  case Kind::Convert:startConvert(ReturnTo::Caller);break;
  case Kind::Scale:startMath(A::Kind::Scale,Component::Scale,ReturnTo::Caller,value_,bias_,coefficients_);break;
  case Kind::Square:startMath(A::Kind::Square,Component::Square,ReturnTo::Caller,value_,bias_,coefficients_);break;
  case Kind::Coefficients:startMath(A::Kind::Coefficients,Component::Coefficients,ReturnTo::Caller,value_,bias_,coefficients_);break;
  case Kind::Range:startMath(A::Kind::Range,Component::Range,ReturnTo::Caller,value_,bias_,coefficients_);break;
  }
 }
 Kind kind()const{return kind_;}Component component()const{return active_;}uint32_t components()const{return components_;}
 bool complete()const{return complete_;}bool available()const{return available_;}
 uint16_t event()const{return event_;}uint8_t value()const{return value_;}uint8_t selector()const{return selector_;}
 uint8_t scaledValue()const{return scaled_;}uint8_t squareInput()const{return squareInput_;}
 uint8_t minimum()const{return minimum_;}uint8_t maximum()const{return maximum_;}uint8_t factor()const{return factor_;}uint16_t bias()const{return bias_;}
 bool atPanel(PanelInputTask::Stage stage)const{return panel_&&panel_->stage()==stage&&(active_==Component::Buttons||active_==Component::Controls);}
 bool atKeyboard(KeyboardEventsTask::Stage stage)const{return keyboard_&&active_==Component::Keyboard&&keyboard_->stage()==stage;}
 bool atAnalog(PanelAnalogTask::Stage stage)const{return analogTask()&&analogTask()->stage()==stage;}
 const PanelInputTask* panelTask()const{return active_==Component::Buttons||active_==Component::Controls?panel_?&*panel_:nullptr:nullptr;}
 const KeyboardEventsTask* keyboardTask()const{return active_==Component::Keyboard&&keyboard_?&*keyboard_:nullptr;}
 const PanelAnalogTask* analogTask()const{return active_==Component::Convert?convert_?&*convert_:nullptr:math_?&*math_:nullptr;}
 // The resource bus may share a pair of adjacent byte writes. An intervening
 // service publication changes this input without changing the continuation.
 unsigned nextClocks(int previousWrite=-1)const{
  if(complete_)return 0;
  if(active_==Component::Buttons||active_==Component::Controls)return panel_->nextClocks(wordClocks((panel_->controls()?0x1540a:0x15380)+panel_->eventIndex()));
  if(active_==Component::Keyboard)return keyboard_->nextClocks(wordClocks(0x156ec+keyboard_->consumer()));
  const auto*t=analogTask();return t->nextClocks(memoryClocks(*t,previousWrite));
 }
 template<class Reader,class Writer>
 unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;
  const auto readWord=[&](uint32_t address){const auto gap=wordClocks(address)-3;const auto high=uint8_t(read(address,cost-gap));return uint16_t((unsigned(high)<<8)|uint8_t(read(address+1,cost)));};
  const auto writeWord=[&](uint32_t address,uint16_t v){const auto gap=wordClocks(address)-3;write(address,uint8_t(v>>8),cost-gap);write(address+1,uint8_t(v),cost);};
  if(active_==Component::Buttons||active_==Component::Controls){
   using T=PanelInputTask;const auto stage=panel_->stage();const bool controls=panel_->controls();const uint32_t producer=controls?0x15406:0x1537c;
   panel_->advanceBefore(cost,[&](T::Read field)->uint16_t{
    return readWord(field==T::Read::Producer?producer:field==T::Read::Consumer?producer+2:producer+4+panel_->eventIndex());
   },[&](T::Write field,uint16_t v){writeWord(field==T::Write::Producer?producer:producer+2,v);},wordClocks(producer+4+panel_->eventIndex()));
   if(stage==T::Stage::ReadProducer)available_=panel_->consumer()!=panel_->producer();
   if(panel_->awaitingControl()){
    event_=panel_->event();value_=uint8_t(event_>>8);selector_=uint8_t(event_);available_=true;startConvert(ReturnTo::Controls);
   }else if(panel_->complete()){
    if(kind_!=Kind::ControlPop||!available_)event_=available_?panel_->event():controls?0:0xff00;
    complete_=true;
   }
  }else if(active_==Component::Keyboard){
   using T=KeyboardEventsTask;
   keyboard_->advanceBefore(cost,[&](T::Read field)->uint16_t{return readWord(field==T::Read::Producer?0x156e8:field==T::Read::Consumer?0x156ea:0x156ec+keyboard_->consumer());},
    [&](T::Write,uint16_t v){writeWord(0x156ea,v);},wordClocks(0x156ec+keyboard_->consumer()));
   if(keyboard_->complete()){event_=keyboard_->message();available_=keyboard_->consumer()!=keyboard_->producer();complete_=true;}
  }else{
   using T=PanelAnalogTask;using S=T::Stage;
   const bool converting=active_==Component::Convert;auto&t=converting?*convert_:*math_;const auto stage=t.stage();
   t.advanceBefore(cost,[&](T::Read field){return uint8_t(read(field==T::Read::Minimum?history_:field==T::Read::Maximum?history_+1:field==T::Read::Offset?mathCoefficients_:mathCoefficients_+1,cost));},
    [&](T::Write field,uint8_t v){write(field==T::Write::Minimum?history_:field==T::Write::Maximum?history_+1:field==T::Write::Offset?mathCoefficients_:mathCoefficients_+1,v,cost);},memoryClocks(t,previousWrite));
   if(converting){
    if(t.awaitingChild()){
     const bool second=stage==S::ScaleSecond;
     startMath(T::Kind::Scale,Component::Scale,ReturnTo::Convert,t.input(),second?0:0x400,second?0x158ee:0x158ec);
    }else if(t.complete()){
     if(stage==S::SelectSquare&&selector_==0x20){const auto target=convertReturn_;const auto input=t.input();convert_.reset();startMath(T::Kind::Square,Component::Square,target,input,0,0);}
     else {value_=t.input();convert_.reset();finishValue(convertReturn_);}
    }
   }else if(t.complete()){
    value_=t.input();minimum_=t.minimum();maximum_=t.maximum();factor_=t.factor();
    const auto target=mathReturn_;math_.reset();
    if(target==ReturnTo::Convert){scaled_=value_;convert_->resumeChild(value_);select(Component::Convert);}
    else finishValue(target);
   }
  }
  return cost;
 }
private:
 enum class ReturnTo { Caller, Controls, Convert };
 static unsigned wordClocks(uint32_t address){return 3+3*(address&1);}
 unsigned memoryClocks(const PanelAnalogTask&t,int previousWrite)const{
  using S=PanelAnalogTask::Stage;uint32_t address=0;
  switch(t.stage()){
  case S::WriteMinimum:address=history_;break;case S::WriteMaximum:address=history_+1;break;
  case S::WriteOffset:address=mathCoefficients_;break;case S::WriteFactor:address=mathCoefficients_+1;break;
  default:return 3;
  }
  return address<0x400000&&!(previousWrite&1)&&address==(uint32_t(previousWrite)|1u)?0:3;
 }
 void select(Component component){active_=component;components_|=1u<<unsigned(component);}
 void startConvert(ReturnTo target){convertReturn_=target;convert_.emplace(PanelAnalogTask::Kind::Convert,value_,selector_,bias_);select(Component::Convert);}
 void startMath(PanelAnalogTask::Kind kind,Component component,ReturnTo target,uint8_t input,uint16_t bias,uint32_t coefficients){
  mathReturn_=target;mathCoefficients_=coefficients;math_.emplace(kind,input,selector_,bias);if(kind==PanelAnalogTask::Kind::Square)squareInput_=input;select(component);
 }
 void finishValue(ReturnTo target){
  if(target==ReturnTo::Controls){event_=uint16_t((unsigned(value_)<<8)|selector_);panel_->resumeControl();select(Component::Controls);}
  else {event_=uint16_t((unsigned(value_)<<8)|selector_);complete_=true;}
 }
 Kind kind_;Component active_=Component::Buttons;uint32_t components_=0;bool complete_=false,available_=false;
 uint16_t event_=0,bias_=0;uint8_t value_=0,selector_=0,scaled_=0,squareInput_=0,minimum_=0,maximum_=0,factor_=0;
 uint32_t history_=0,coefficients_=0,mathCoefficients_=0;
 ReturnTo convertReturn_=ReturnTo::Caller,mathReturn_=ReturnTo::Caller;
 std::optional<PanelInputTask>panel_;std::optional<KeyboardEventsTask>keyboard_;std::optional<PanelAnalogTask>convert_,math_;
};
}
