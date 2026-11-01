#pragma once
#include "ksStateScreenTask.h"
#include "ksStateScreenEditTask.h"
#include "ksStateEditContextTask.h"
#include "ksStateMenuService.h"
#include "ksStateApplyService.h"
#include "ksStateDisplayService.h"
#include "ksPanelSettingsService.h"
#include <array>
#include <optional>
#include <variant>
#include <stdexcept>
namespace ks {
// Screen navigation, publication and value presentation form one native graph.
// Retained values describe the UI transaction and observed resources only.
class StateScreenService final {
public:
 enum class Kind {Descriptor,Set,Next,Previous,Refresh,Gate,Render,Label,Edit,Value};
 enum class Component {Screen,Edit,Value,Menu,Apply,Display,Settings};
 struct Input {uint8_t index=0,format=0;uint16_t character=0;uint32_t valueIndex=0;};
 struct Effect {bool read=false,write=false;};
 struct DescriptorResult {bool observed=false,sentinel=false,dynamic=false;uint32_t pointer=0,patch=0;uint8_t count=0,page=0,header=0,variant=0,dynamicPage=0,dynamicLimit=0;};
 struct GateResult {bool observed=false,editObserved=false,accepted=false;uint8_t hardware=0,screen=0,page=0,edit=0;};
 struct DisplayResult {bool observed=false;StateDisplayService::Kind kind=StateDisplayService::Kind::Character;StateDisplayService::Presentation presentation=StateDisplayService::Presentation::Unknown;StateDisplayService::Input input{};StateDisplayService::LcdResult lcd{};uint16_t output=0;uint32_t cursor=0,tableAfterHeader=0,record=0,patch=0,textSource=0;uint8_t number=0,mode=0;bool signedNumber=false;};
 struct ValueResult {bool observed=false,stored=false;uint32_t index=0,base=0,storedPointer=0;uint8_t current=0,type=0,reference=0;DisplayResult currentDisplay{},referenceDisplay{},matchDisplay{};};
 struct ApplyResult {bool observed=false,packed=false,signalled=false;uint32_t packingBase=0,packingDescriptor=0;uint8_t cached=0,type=0;};
 struct Summary {DescriptorResult descriptor{};GateResult gate{};ValueResult value{};ApplyResult apply{};DisplayResult heading{};bool label=false,render=false,reset=false;uint32_t record=0,editBase=0;uint8_t parameter=0,format=0,setReset=0,resetLast=0,level=0;uint16_t labelWord=0;};
 StateScreenService(Kind kind):StateScreenService(kind,Input{}){}
 StateScreenService(Kind kind,Input in):kind_(kind),input_(in),textWord_(in.character){
  if(unsigned(kind)<8)push(StateScreenTask(StateScreenTask::Kind(unsigned(kind)),in.index));
  else if(kind==Kind::Edit)push(StateScreenEditTask(StateScreenEditTask::Kind::Edit,uint8_t(in.character)));
  else startValue(in.valueIndex,in.format);
 }
 uint16_t presentation()const{return textWord_;}
 Kind kind()const{return kind_;}const Input&input()const{return input_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return !depth_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 bool critical()const{const auto*t=task<StateApplyService>();return t&&t->critical();}
 template<class T>const T*task()const{return depth_?std::get_if<T>(&frames_[depth_-1]->task):nullptr;}
 static bool dataSpan(uint32_t raw,unsigned n){return StateStoreService::dataSpan(raw,n);}
 template<class Peek>static bool admitted(Kind kind,const Input&in,Peek&&peek){
  const auto wide=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(peek((a+n)&0xffffff));return v;};
  if(kind==Kind::Edit||kind==Kind::Label){if(!dataSpan(wide(0x167f0),20))return false;}
  if(kind==Kind::Edit){if(!dataSpan(wide(0x167bc),256)||uint8_t(peek(0xffed6c))||uint8_t(peek(0xffed6e)))return false;}
  if(kind==Kind::Value){const auto over=wide(0x167a6);if(!dataSpan(over?over:wide(0x167bc),256))return false;}
  if(kind==Kind::Refresh||kind==Kind::Gate||kind==Kind::Edit||kind==Kind::Label||kind==Kind::Value)return true;
  auto index=kind==Kind::Descriptor||kind==Kind::Set?in.index:uint8_t(peek(0x16785));
  if(index==12&&!uint8_t(peek(0x167ba)))++index;
  const auto offset=uint8_t(index<<2);auto descriptor=wide(0x101ae + offset);
  if(uint8_t(peek(0xffec14))&&descriptor==0x10517)descriptor=0x10569;
  if(!dataSpan(descriptor,2))return false;
  if(uint8_t(peek(descriptor&0xffffff))==255){const auto patch=wide(0x167bc);if(!dataSpan(patch,227))return false;descriptor=wide(0x101e6+uint8_t(uint8_t(peek((patch+226)&0xffffff))<<2));if(!dataSpan(descriptor,2))return false;}
  if(kind==Kind::Descriptor)return true;
  const auto page=uint8_t(peek((descriptor+1)&0xffffff));return dataSpan(descriptor+2+20u*page,20);
 }
 unsigned nextClocks(int previous=-1)const{
  if(!depth_)return 0;const auto&f=*frames_[depth_-1];return std::visit([&](const auto&t)->unsigned{using T=std::decay_t<decltype(t)>;
   if constexpr(std::is_same_v<T,StateScreenTask>)return t.nextClocks(byteClocks(screenTarget(t),previous));
   else if constexpr(std::is_same_v<T,StateScreenEditTask>||std::is_same_v<T,StateEditContextTask>)return t.nextClocks();
   else return t.nextClocks(previous);
  },f.task);
 }
 template<class Reader,class Writer,class Resolver>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,Resolver&&resolve,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>available)return 0;effect_={};auto&f=*frames_[depth_-1];
  const auto byte=[&](uint32_t a,unsigned at=0){return uint8_t(read(a&0xffffff,at?at:cost));};
  const auto word32=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|byte(a+n,cost-3*((3-n)/2));return v;};
  const auto checked=[&](uint32_t v,unsigned n){if(!dataSpan(v,n))throw std::logic_error("Unsupported evolving screen data span");return v;};
  const auto base=[&](uint32_t a,bool optional=false){const auto v=word32(a);if(v||!optional)checked(v,256);return v;};
  const auto data=[&](uint32_t a,unsigned at=0){checked(a,1);return byte(a,at);};
  std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;
   if constexpr(std::is_same_v<T,StateScreenTask>){using S=StateScreenTask::Stage;const auto stage=t.stage();
    effect_.write=screenCall(stage);effect_.read=stage==S::ReturnDescriptor||stage==S::ReturnRefresh||stage==S::ReturnGate||stage==S::ReturnRender||stage==S::ReturnLabel;
    t.advanceBefore(cost,[&](StateScreenTask::Read what,uint32_t ref)->uint32_t{using R=StateScreenTask::Read;switch(what){
     case R::Mode:return byte(0x167ba);case R::Hardware:return byte(0xffec14);
     case R::PrimaryPointer:return checked(word32(0x101ae + ref),2);case R::SecondaryPointer:return checked(word32(0x101e6+ref),2);
     case R::DescriptorByte:return data(ref,stage==S::ReadCount||stage==S::ReadCharacter||stage==S::ReadLabelParameter?cost-2:cost);
     case R::PatchPointer:return checked(word32(0x167bc),227);case R::Variant:return data(ref+226);
     case R::ResetFlag:return summary_.setReset=byte(0x169f7);case R::Screen:return byte(0x16785);case R::UiPage:return byte(0x16787);
     case R::RecordPointer:summary_.record=checked(word32(0x167f0),20);summary_.label=true;return summary_.record;
    }return 0;},[&](StateScreenTask::Write what,uint32_t at,uint32_t v){using W=StateScreenTask::Write;if(what==W::RecordPointer){checked(v,20);summary_.record=v;summary_.render=true;for(unsigned n=0;n<4;++n)write(0x167f0+n,uint8_t(v>>(24-8*n)),cost-3*((3-n)/2));}else{at=what==W::DescriptorByte?at:what==W::Screen?0x16785:0x16787;checked(at,1);write(at&0xffffff,uint8_t(v),cost);}},byteClocks(screenTarget(t),previous));
    if(stage==S::CompareDynamicPage){summary_.descriptor.dynamic=true;summary_.descriptor.dynamicPage=t.page();summary_.descriptor.dynamicLimit=t.count();}
    if(stage==S::ReadParameter||stage==S::ReadLabelParameter)summary_.parameter=t.parameter();if(stage==S::ReadFormat)summary_.format=t.format();
    if(t.awaitingChild())switch(stage){
     case S::LookupSet:case S::LookupNext:case S::LookupPrevious:case S::LookupRender:push(StateScreenTask(StateScreenTask::Kind::Descriptor,t.index()));break;
     case S::RenderSelection:push(StateScreenTask(StateScreenTask::Kind::Render));break;
     case S::RefreshGate:push(StateScreenTask(StateScreenTask::Kind::Gate));break;
     case S::InitializeCalibration:push(PanelSettingsService(PanelSettingsService::Kind::Reset));summary_.reset=true;break;
     case S::CheckCalibration:push(StateMenuService(StateMenuService::Kind::EditActive));break;
     case S::DisplayHeading:startDisplay(StateDisplayService::Kind::Text,0,0,0,0x14f5e);break;
     case S::BindParameter:startMenu(StateMenuService::Kind::BindContext,t.parameter());break;
     case S::DisplayLabel:push(StateScreenTask(StateScreenTask::Kind::Label));break;
     case S::ClearParameterBase:startMenu(StateMenuService::Kind::ClearContext,0);break;
     case S::SetLabelPosition:setTextByte(128);startDisplay(StateDisplayService::Kind::Character);break;
     case S::DisplayCharacter:setTextByte(t.character());startDisplay(StateDisplayService::Kind::Character);break;
     case S::DisplaySeparator:setTextByte(32);startDisplay(StateDisplayService::Kind::Character);break;
     case S::DisplayValue:startValue(t.parameter(),t.format());break;
     default:throw std::logic_error("Unexpected screen child");
    }else if(t.complete()){
     if(stage==S::ReturnDescriptor){auto&d=summary_.descriptor;d.observed=true;d.sentinel=t.header()==255;d.pointer=t.pointer();d.patch=t.patch();d.count=t.count();d.page=t.page();d.header=t.header();d.variant=t.variant();}
     if(stage==S::ReturnGate){auto&g=summary_.gate;g.observed=true;g.hardware=t.hardware();g.screen=t.screen();g.page=t.page();g.accepted=t.accepted();}
     finish({t.pointer(),t.count(),t.page(),t.accepted()});
    }
   }else if constexpr(std::is_same_v<T,StateScreenEditTask>){using S=StateScreenEditTask::Stage;const auto stage=t.stage();
    effect_.write=stage==S::BindEditContext||stage==S::PublishValue||stage==S::RefreshLabel||stage==S::ClearContext||stage==S::ApplyPanelLevel;effect_.read=stage==S::ReturnEdit;
    t.advanceBefore(cost,[&](StateScreenEditTask::Read what,uint32_t ref)->uint32_t{using R=StateScreenEditTask::Read;if(what==R::Descriptor)return checked(word32(0x167f0),20);if(what==R::EditBase)return base(0x167a6,true);if(what==R::PatchBase)return base(0x167bc);return data(ref);},[](auto,auto){});
    if(stage==S::ReadPanelLevel)summary_.level=t.level();if(stage==S::ReadEditBase||stage==S::ReadPatchBase)summary_.editBase=t.base();
    if(t.awaitingChild())switch(stage){
     case S::BindEditContext:startMenu(StateMenuService::Kind::BindContext,t.parameter());break;
     case S::PublishValue:{textWord_=uint16_t((unsigned(t.value())<<8)|uint8_t(input_.character));StateApplyService::Input in{t.base()+t.parameter(),textWord_};push(StateApplyService(StateApplyService::Kind::Apply,in));break;}
     case S::RefreshLabel:push(StateScreenTask(StateScreenTask::Kind::Label));break;
     case S::ClearContext:startMenu(StateMenuService::Kind::ClearContext,0);break;
     case S::ApplyPanelLevel:{PanelSettingsService::Input in;in.event=t.level();push(PanelSettingsService(PanelSettingsService::Kind::AnalogLevel,in));break;}
     default:throw std::logic_error("Unexpected screen edit child");
    }else if(t.complete())finish({});
   }else if constexpr(std::is_same_v<T,StateEditContextTask>){using S=StateEditContextTask::Stage;const auto stage=t.stage();
    effect_.write=stage==S::SaveComparedValue||stage==S::SaveComparedIndex||stage==S::DisplayCurrentValue||stage==S::ReadReferenceValue||stage==S::PrintMatchText||stage==S::DisplayReferenceValue;
    effect_.read=stage==S::RestoreComparedIndex||stage==S::RestoreComparedValue||stage==S::ReturnValue;
    t.advanceBefore(cost,[&](StateEditContextTask::Read what,uint32_t ref)->uint32_t{using R=StateEditContextTask::Read;if(what==R::EditBase)return base(0x167a6,true);if(what==R::PatchBase)return base(0x167bc);if(what==R::Value)return data(ref);if(what==R::ValueKind)return data(68974+ref);throw std::logic_error("Unexpected screen display context resource");},[](auto,auto){});
    if(stage==S::ReadValue){summary_.value.current=t.value();summary_.value.base=t.base();}if(stage==S::ReadValueKind)summary_.value.type=t.kind();
    if(t.awaitingChild())switch(stage){
     case S::DisplayCurrentValue:setTextByte(145);startDisplay(StateDisplayService::Kind::Format,t.kind(),t.value(),f.format);break;
     case S::ReadReferenceValue:push(StateApplyService(StateApplyService::Kind::StoredValue,StateApplyService::Input{t.index(),0}));break;
     case S::PrintMatchText:startDisplay(StateDisplayService::Kind::Text,0,0,0,0x14f68);break;
     case S::DisplayReferenceValue:setTextByte(209);startDisplay(StateDisplayService::Kind::Format,t.kind(),summary_.value.reference,f.format);break;
     default:throw std::logic_error("Unexpected screen value child");
    }else if(t.complete())finish({});
   }else if constexpr(std::is_same_v<T,StateMenuService>){
    if(const auto*c=t.contextTask()){using S=StateEditContextTask::Stage;const auto stage=c->stage();effect_.write=stage==S::SaveContextValue||stage==S::SaveContextIndex||stage==S::SaveClearValue;effect_.read=stage==S::RestoreContextIndex||stage==S::RestoreContextValue||stage==S::ReturnContext||stage==S::RestoreClearValue||stage==S::ReturnClear;}else effect_.read=t.component()==StateMenuService::Component::EditGate&&t.gatePhase()==1;
    t.advanceBefore(cost,read,write,[](auto,unsigned){throw std::logic_error("Screen context unexpectedly scheduled navigation");},previous);
    if(t.complete()){if(t.kind()==StateMenuService::Kind::EditActive){summary_.gate.editObserved=true;summary_.gate.edit=t.edit();}finish({0,0,0,bool(t.edit())});}
   }else if constexpr(std::is_same_v<T,StateApplyService>){
    effect_.read=t.clearsReadHistory();effect_.write=t.clearsWriteHistory();
    t.advanceBefore(cost,[&](uint32_t at,unsigned offset){const auto v=uint8_t(read(at&0xffffff,offset));if((at==0xffed6c||at==0xffed6e)&&v)throw std::logic_error("Unsupported evolving screen UART producer");return v;},write,[&](uint32_t raw){summary_.apply.packingDescriptor=raw;return resolve(raw);},previous);
    if(t.complete()){
     if(t.kind()==StateApplyService::Kind::StoredValue){summary_.value.stored=true;summary_.value.reference=t.storedValue();summary_.value.storedPointer=t.storedPointer();}
     else {auto&a=summary_.apply;a.observed=true;a.packed=t.packed();a.signalled=t.signalled();a.packingBase=t.packingBase();a.cached=t.cached();a.type=t.type();}
     const auto reference=t.storedValue();finish({},reference);
    }
   }else if constexpr(std::is_same_v<T,StateDisplayService>){
    effect_.read=t.endsReadTransfer();effect_.write=t.endsWriteTransfer();t.advanceBefore(cost,[&](uint32_t a,unsigned at){if(a!=0xffff51)checked(a,1);return read(a&0xffffff,at);},write,previous);
    if(t.complete()){textWord_=t.output();const auto outcome=displayResult(t);if(depth_>1){auto&parent=frames_[depth_-2]->task;if(const auto*s=std::get_if<StateScreenTask>(&parent)){if(s->resumeStage()==StateScreenTask::Stage::ReadRenderScreen)summary_.heading=outcome;if(s->resumeStage()==StateScreenTask::Stage::ReadLabelParameter)summary_.labelWord=textWord_;}if(const auto*v=std::get_if<StateEditContextTask>(&parent)){using S=StateEditContextTask::Stage;if(v->resumeStage()==S::RestoreComparedIndex)summary_.value.currentDisplay=outcome;else if(v->resumeStage()==S::JoinMatch)summary_.value.matchDisplay=outcome;else summary_.value.referenceDisplay=outcome;}}finish({});}
   }else if constexpr(std::is_same_v<T,PanelSettingsService>){t.advanceBefore(cost,read,write,resolve,previous);effect_={t.effect().read,t.effect().write};if(t.complete()){if(summary_.reset)summary_.resetLast=t.copiedLastByte();finish({});}}
  },f.task);
  selectCurrent();return cost;
 }
private:
 using Task=std::variant<StateScreenTask,StateScreenEditTask,StateEditContextTask,StateMenuService,StateApplyService,StateDisplayService,PanelSettingsService>;
 struct Frame {template<class T>Frame(T t,uint8_t f):task(std::move(t)),format(f){}Task task;uint8_t format=0;};
 template<class T>void push(T t,uint8_t format=0){if constexpr(std::is_same_v<T,StateScreenTask>){if(t.stage()==StateScreenTask::Stage::CheckDescriptorIndex)summary_.descriptor={};if(t.stage()==StateScreenTask::Stage::ClearGate)summary_.gate={};}if(depth_==frames_.size())throw std::logic_error("Screen native continuation capacity");frames_[depth_++].emplace(std::move(t),format);selectCurrent();}
 void startValue(uint32_t index,uint8_t format){summary_.value={};summary_.value.observed=true;summary_.value.index=uint8_t(index);push(StateEditContextTask(StateEditContextTask::Kind::Display,index),format);}
 void startMenu(StateMenuService::Kind kind,uint32_t index){StateMenuService::Input in;in.index=index;push(StateMenuService(kind,in));}
 void startDisplay(StateDisplayService::Kind kind,uint8_t type=0,uint8_t value=0,uint8_t bit=0,uint32_t source=0){StateDisplayService::Input in;in.type=type;in.value=value;in.bit=bit;in.source=source;in.character=textWord_;push(StateDisplayService(kind,in));}
 void setTextByte(uint8_t v){textWord_=uint16_t((textWord_&0xff00)|v);}
 void finish(StateScreenTask::Reply reply,uint8_t reference=0){frames_[--depth_].reset();if(!depth_)return;std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,StateScreenTask>)t.resumeChild(reply);else if constexpr(std::is_same_v<T,StateScreenEditTask>)t.resumeChild();else if constexpr(std::is_same_v<T,StateEditContextTask>)t.resumeChild(reference);else throw std::logic_error("Unexpected screen parent ownership");},frames_[depth_-1]->task);}
 static DisplayResult displayResult(const StateDisplayService&t){DisplayResult r;r.observed=true;r.kind=t.kind();r.presentation=t.presentation();r.input=t.input();r.lcd=t.lcdResult();r.output=t.output();r.cursor=t.cursor();r.number=t.numberValue();r.signedNumber=t.signedNumber();r.textSource=t.textSource();if(const auto*f=t.formatTask()){r.tableAfterHeader=f->table()+1;r.record=f->record();r.patch=f->patch();r.mode=f->mode();}return r;}
 static unsigned byteClocks(uint32_t a,int previous){a&=0xffffff;return a<0x400000&&!(previous&1)&&a==(uint32_t(previous)|1u)?0:3;}
 static uint32_t screenTarget(const StateScreenTask&t){using S=StateScreenTask::Stage;const auto s=t.stage();return s==S::WriteDynamicCount||s==S::WriteCurrent?t.pointer():s==S::ClampDynamicPage?t.pointer()+1:s==S::WriteScreen?0x16785:0x16787;}
 static bool screenCall(StateScreenTask::Stage s){using S=StateScreenTask::Stage;switch(s){case S::LookupSet:case S::LookupNext:case S::LookupPrevious:case S::RenderSelection:case S::RefreshGate:case S::InitializeCalibration:case S::CheckCalibration:case S::DisplayHeading:case S::LookupRender:case S::BindParameter:case S::DisplayLabel:case S::ClearParameterBase:case S::SetLabelPosition:case S::DisplayCharacter:case S::DisplaySeparator:case S::DisplayValue:return true;default:return false;}}
 void selectCurrent(){if(!depth_)return;component_=std::visit([](const auto&t){using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,StateScreenTask>)return Component::Screen;else if constexpr(std::is_same_v<T,StateScreenEditTask>)return Component::Edit;else if constexpr(std::is_same_v<T,StateEditContextTask>)return Component::Value;else if constexpr(std::is_same_v<T,StateMenuService>)return Component::Menu;else if constexpr(std::is_same_v<T,StateApplyService>)return Component::Apply;else if constexpr(std::is_same_v<T,StateDisplayService>)return Component::Display;else return Component::Settings;},frames_[depth_-1]->task);components_|=1u<<unsigned(component_);}
 Kind kind_;Input input_;Summary summary_{};Effect effect_{};std::array<std::optional<Frame>,8>frames_{};unsigned depth_=0;uint16_t textWord_=0;Component component_=Component::Screen;uint32_t components_=0;
};
}
