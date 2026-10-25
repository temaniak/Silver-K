#pragma once
#include "ksStateMenuFormatTask.h"
#include "ksPanelFormatTask.h"
#include "ksPanelDecimalTask.h"
#include "ksPanelTextTask.h"
#include "ksPanelActionTask.h"
#include "ksPanelPollTask.h"
#include "ksPanelByteTask.h"
#include <array>
#include <optional>
#include <stdexcept>
namespace ks {
// Complete menu presentation and reusable LCD transport. Numeric and disabled
// text tails remain typed children, through busy polling to actual byte ports.
class StateDisplayService final {
public:
 enum class Kind {Format,Off,Text,Character,Command,SignedNumber};
 enum class Component {Format,Number,Decimal,Text,Parse,Action,Poll,Byte};
 enum class Presentation {Unknown,Table,Number,Disabled,Text,Character,Command};
 struct Input {uint8_t type=0,value=0,bit=0;uint16_t character=0;uint32_t source=0;};
 struct LcdResult {PanelTextTask::Action action=PanelTextTask::Action::Command;uint8_t input=0,reply=0;bool observed=false;};
 explicit StateDisplayService(Kind kind):StateDisplayService(kind,Input{}){}
 StateDisplayService(Kind kind,Input input):kind_(kind),input_(input),position_(uint8_t(input.character)),output_(input.character){
  switch(kind){case Kind::Format:format_.emplace(StateMenuFormatTask::Kind::Format,input.type,input.value,input.bit,uint8_t(input.character));select(Component::Format);break;
  case Kind::Off:format_.emplace(StateMenuFormatTask::Kind::Off);select(Component::Format);break;
  case Kind::Text:presentation_=Presentation::Text;startText(TextOwner::Root,input.source,PanelTextTask::Kind::Parsed);break;
  case Kind::Character:presentation_=Presentation::Character;startParse(ParseOwner::Root);break;
  case Kind::Command:presentation_=Presentation::Command;actionOwner_=ActionOwner::Root;startAction(PanelActionTask::Kind::Command);break;
  case Kind::SignedNumber:presentation_=Presentation::Number;numberOwner_=NumberOwner::Tail;numberValue_=input.value;number_.emplace(PanelFormatTask::Kind::SignedThree);select(Component::Number);break;}
 }
 Kind kind()const{return kind_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return complete_;}
 Presentation presentation()const{return presentation_;}const Input&input()const{return input_;}const LcdResult&lcdResult()const{return lcdResult_;}
 uint8_t outputByte()const{return uint8_t(output_);}uint8_t outputCount()const{return uint8_t(output_>>8);}uint16_t output()const{return output_;}
 uint8_t width()const{return width_;}uint32_t cursor()const{return cursor_;}uint32_t tableAfterHeader()const{return format_->table()+1;}bool signedNumber()const{return number_&&number_->kind()==PanelFormatTask::Kind::SignedThree;}
 uint8_t numberValue()const{return numberValue_;}uint32_t textSource()const{return textSource_;}uint32_t textCursor()const{return text_?text_->pointer():textSource_;}
 const StateMenuFormatTask*formatTask()const{return format_?&*format_:nullptr;}const PanelFormatTask*numberTask()const{return number_?&*number_:nullptr;}
 const PanelDecimalTask*decimalTask()const{return decimal_?&*decimal_:nullptr;}
 const PanelTextTask*textTask()const{return text_?&*text_:nullptr;}const PanelTextTask*parseTask()const{return parse_?&*parse_:nullptr;}
 const PanelActionTask*actionTask()const{return actionCount_?&actions_[actionCount_-1]->task:nullptr;}const PanelPollTask*pollTask()const{return poll_?&*poll_:nullptr;}const PanelByteTask*byteTask()const{return byte_?&*byte_:nullptr;}
 bool atFormat(StateMenuFormatTask::Stage s)const{return component_==Component::Format&&format_->stage()==s;}
 bool atNumber(PanelFormatTask::Stage s)const{return component_==Component::Number&&number_->stage()==s;}
 bool atDecimal(PanelDecimalTask::Stage s)const{return component_==Component::Decimal&&decimal_->stage()==s;}
 bool atText(PanelTextTask::Stage s)const{return component_==Component::Text&&text_->stage()==s;}
 bool atParse(PanelTextTask::Stage s)const{return component_==Component::Parse&&parse_->stage()==s;}
 bool atAction(PanelActionTask::Stage s)const{return component_==Component::Action&&actionTask()->stage()==s;}
 bool atAction(PanelActionTask::Kind k,PanelActionTask::Stage s)const{return atAction(s)&&actionTask()->kind()==k;}
 bool atByte(PanelByteTask::Kind k,PanelByteTask::Stage s)const{return atByte(s)&&byte_->kind()==k;}
 bool atPoll(PanelPollTask::Stage s)const{return component_==Component::Poll&&poll_->stage()==s;}
 bool atByte(PanelByteTask::Stage s)const{return component_==Component::Byte&&byte_->stage()==s;}
 static bool dataSpan(uint32_t raw,unsigned size){const auto a=raw&0xffffff;return size&&((a>=0xb8e8&&a<0x200000&&size<=0x200000-a)||(a>=0xa0b8e8&&a<0xc00000&&size<=0xc00000-a)||(a>=0xffdc00&&a<0xfff000&&size<=0xfff000-a));}
 // Scoped transfer boundaries release the previous bus pairing observation.
 bool endsWriteTransfer()const{if(complete_)return false;using C=Component;switch(component()){
 case C::Format:{using S=StateMenuFormatTask::Stage;switch(formatTask()->stage()){case S::SavePosition:case S::SetPosition:case S::SetItemPosition:case S::DisplayCharacter:case S::DisplayBiasedNumber:case S::DisplayIncrementedNumber:return true;default:return false;}}
 case C::Number:{using S=PanelFormatTask::Stage;switch(numberTask()->stage()){case S::RetainValue:case S::RetainPointer:case S::RetainFormat:case S::DispatchDecimal:case S::DispatchText:return true;default:return false;}}
 case C::Text:return textTask()->stage()==PanelTextTask::Stage::HoldText||textTask()->stage()==PanelTextTask::Stage::DispatchText;
 case C::Parse:{using S=PanelTextTask::Stage;switch(parseTask()->stage()){case S::DispatchWrite:case S::DispatchSecondBoth:case S::DispatchFirstBoth:case S::DispatchFirstOnly:case S::DispatchSecondOnly:case S::DispatchCursor:case S::DispatchDisplay:case S::DispatchCommand:return true;default:return false;}}
 case C::Action:{using S=PanelActionTask::Stage;switch(actionTask()->stage()){case S::Hold:case S::Poll:case S::PollAgain:case S::CommandByte:case S::DataByte:case S::DataRead:case S::CommandFirst:case S::CommandSecond:case S::CommandThird:case S::CommandAction:case S::WriteAction:case S::CursorFirst:case S::CursorLast:case S::SpacesAction:return true;default:return false;}}
 case C::Poll:return pollTask()->stage()==PanelPollTask::Stage::DispatchStatus||pollTask()->stage()==PanelPollTask::Stage::DispatchFinal;
 case C::Byte:return byteTask()->stage()==PanelByteTask::Stage::Hold;default:return false;}}
 bool endsReadTransfer()const{if(complete_)return false;using C=Component;switch(component()){
 case C::Format:{using S=StateMenuFormatTask::Stage;switch(formatTask()->stage()){case S::RestorePosition:case S::ReturnText:case S::ReturnBiased:case S::ReturnIncremented:case S::ReturnUnknown:return true;default:return false;}}
 case C::Number:{using S=PanelFormatTask::Stage;switch(numberTask()->stage()){case S::RestoreValue:case S::RestorePointer:case S::RestoreFormat:case S::Finish:return true;default:return false;}}
 case C::Decimal:return decimalTask()->stage()==PanelDecimalTask::Stage::Finish;case C::Text:return textTask()->stage()==PanelTextTask::Stage::RestoreText||textTask()->stage()==PanelTextTask::Stage::Finish;case C::Parse:return parseTask()->stage()==PanelTextTask::Stage::Finish;
 case C::Action:return actionTask()->stage()==PanelActionTask::Stage::Restore||actionTask()->stage()==PanelActionTask::Stage::Finish;case C::Poll:return pollTask()->stage()==PanelPollTask::Stage::Finish;case C::Byte:return byteTask()->stage()==PanelByteTask::Stage::RestoreHeld||byteTask()->stage()==PanelByteTask::Stage::Finish;}return false;}
 unsigned nextClocks(int previousWrite=-1)const{
  if(complete_)return 0;switch(component_){case Component::Format:return format_->nextClocks(byteClock(format_->record()+(format_->stage()==StateMenuFormatTask::Stage::WriteOctaveSign?3:4),previousWrite));
  case Component::Number:return number_->nextClocks();case Component::Decimal:return decimal_->nextClocks();
  case Component::Text:return text_->nextClocks([&]{return ((text_->bound()?text_->pointer():textSource_)&0xffffffu)>=0xfffd10?1u:3u;});
  case Component::Parse:return parse_->nextClocks([]{return 3u;});case Component::Action:return actionTask()->nextClocks();case Component::Poll:return poll_->nextClocks();
  case Component::Byte:return byte_->nextClocks([&](PanelByteTask::Port p){return portClock(p,previousWrite);});}return 0;
 }
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;
  const auto longword=[&](uint32_t a){uint32_t result=0;for(unsigned n=0;n<4;++n)result=(result<<8)|uint8_t(read(a+n,cost-3*((3-n)/2)));return result;};
  switch(component_){case Component::Format:{using T=StateMenuFormatTask;using S=T::Stage;const auto stage=format_->stage();
   format_->advanceBefore(cost,[&](T::Read field,uint32_t ref)->uint32_t{if(field==T::Read::Byte){const auto v=uint8_t(read(ref&0xffffff,cost-(stage==S::ReadWidth||stage==S::ReadCharacter?2:0)));if(stage==S::ReadCharacter)cursor_=ref+1;return v;}return longword(field==T::Read::PrimaryTable?0x167c4:field==T::Read::AlternateTable?0x167c8:field==T::Read::ThirdTable?0x167cc:0x167bc);},[&](T::Write,uint32_t a,uint32_t v){write(a&0xffffff,uint8_t(v),cost);},byteClock(format_->record()+(stage==S::WriteOctaveSign?3:4),previousWrite));
   if(stage==S::RestorePosition)output_=savedPosition_;
   if(stage==S::ReadWidth){width_=format_->width();presentation_=Presentation::Table;}if(stage==S::PositionPadding)position_=uint8_t(position_+3);if(stage==S::PositionWidth)position_=uint8_t(position_-format_->width());
   if(format_->awaitingChild()){
    if(stage==S::SetPosition){savedPosition_=output_;startParse(ParseOwner::Position);}
    else if(stage==S::SetItemPosition){setByte(position_);startParse(ParseOwner::ItemPosition);}
    else if(stage==S::DisplayCharacter){setByte(format_->character());startParse(ParseOwner::Character);}
    else startNumber(false,NumberOwner::Format);
   }else if(format_->complete()){
    if(stage==S::UnsignedNumber||stage==S::SelectEnabledNumber||stage==S::OffsetNumber||stage==S::SignedNumber)startNumber(stage==S::SignedNumber,NumberOwner::Tail);
    else if(stage==S::DisplayDisabledText){presentation_=Presentation::Disabled;startText(TextOwner::Disabled,0x11685,PanelTextTask::Kind::Parsed);}
    else complete_=true;
   }break;}
  case Component::Number:{using T=PanelFormatTask;using S=T::Stage;const auto stage=number_->stage();number_->advanceBefore(cost,[&](T::Read){return numberValue_;},[&](int32_t offset,uint8_t value){write(uint32_t(0xffec34+offset)&0xffffff,value,cost);});
   if(number_->child()==T::Child::Decimal){decimal_.emplace();select(Component::Decimal);}
   else if(number_->child()==T::Child::Text)startText(TextOwner::Number,0xffec34,PanelTextTask::Kind::Plain);
   else if(stage==S::Finish){if(numberOwner_==NumberOwner::Tail)complete_=true;else {format_->resumeChild();select(Component::Format);}}break;}
  case Component::Decimal:{using T=PanelDecimalTask;decimal_->advanceBefore(cost,[&](T::Read field){return field==T::Read::Padding?number_->padding():field==T::Read::Width?number_->width():number_->magnitude();},[&](int32_t offset,uint8_t value){write(uint32_t(0xffec34+offset)&0xffffff,value,cost);});if(decimal_->stage()==T::Stage::Complete){number_->resumeChild(decimal_->position());decimal_.reset();select(Component::Number);}break;}
  case Component::Text:{using T=PanelTextTask;using S=T::Stage;const auto stage=text_->stage();text_->advanceBefore(cost,[&](T::Read field)->uint32_t{if(field==T::Read::Pointer)return textSource_;if(field==T::Read::Input)return outputByte();return read(text_->pointer()&0xffffff,cost-2);},[&]{return ((text_->bound()?text_->pointer():textSource_)&0xffffff)>=0xfffd10?1u:3u;});
   if(stage==S::HoldText&&text_->kind()==T::Kind::Plain)plainSaved_=output_;if(stage==S::ReadText)setByte(text_->byte());if(stage==S::RestoreText&&text_->kind()==T::Kind::Plain)output_=plainSaved_;
   if(text_->child()!=T::Child::None){if(text_->child()==T::Child::Parse)startParse(ParseOwner::Text);else {actionOwner_=ActionOwner::Text;startAction(PanelActionTask::Kind::WriteData);}}
   else if(text_->stage()==S::Complete){if(textOwner_==TextOwner::Number){number_->resumeChild();select(Component::Number);}else complete_=true;}break;}
  case Component::Parse:{using T=PanelTextTask;using S=T::Stage;const auto stage=parse_->stage();parse_->advanceBefore(cost,[&](T::Read){return outputByte();},[]{return 3u;});if(stage==S::SetCursor||stage==S::SetDisplay)setByte(parse_->command());
   if(parse_->child()!=T::Child::None){using C=T::Child;auto child=parse_->child();actionOwner_=ActionOwner::Parse;startAction(child==C::Write?PanelActionTask::Kind::WriteData:child==C::ClearFirst?PanelActionTask::Kind::ClearFirst:child==C::ClearSecond?PanelActionTask::Kind::ClearSecond:PanelActionTask::Kind::Command);}
   else if(parse_->stage()==S::Complete){lcdResult_={parse_->action(),parse_->input(),lastWriteReply_,true};if(parseOwner_==ParseOwner::Position){format_->resumeChild();select(Component::Format);}else if(parseOwner_==ParseOwner::ItemPosition||parseOwner_==ParseOwner::Character){format_->resumeChild();select(Component::Format);}else if(parseOwner_==ParseOwner::Text){text_->resumeChild();select(Component::Text);}else complete_=true;}break;}
  case Component::Action:{using T=PanelActionTask;using S=T::Stage;auto&frame=*actions_[actionCount_-1];const auto stage=frame.task.stage();if(stage==S::Hold)frame.heldCount=outputCount();frame.task.advanceBefore(cost,[&](T::Read field){return field==T::Read::Input?outputByte():outputCount();});
   switch(stage){case S::Restore:output_=uint16_t((unsigned(frame.heldCount)<<8)|frame.task.input());break;case S::SetCount:case S::Decrement:output_=uint16_t((unsigned(frame.task.remaining())<<8)|outputByte());break;case S::SetAddress:case S::SetInitial:case S::SetDisplay:case S::SetClear:case S::SetSpace:case S::SetLine:case S::SetEnd:case S::AddCursor:case S::AddSecond:setByte(frame.task.input());break;case S::CompareReply:lastWriteReply_=frame.task.reply();break;default:break;}
   if(frame.task.child()!=T::Child::None){using C=T::Child;switch(frame.task.child()){case C::Poll:poll_.emplace();select(Component::Poll);break;case C::CommandByte:case C::DataByte:case C::DataRead:byteOwner_=ByteOwner::Action;byte_.emplace(frame.task.child()==C::CommandByte?PanelByteTask::Kind::CommandWrite:frame.task.child()==C::DataByte?PanelByteTask::Kind::DataWrite:PanelByteTask::Kind::DataRead);select(Component::Byte);break;default:startAction(frame.task.child()==C::Command?T::Kind::Command:frame.task.child()==C::Write?T::Kind::WriteData:frame.task.child()==C::Cursor?T::Kind::Cursor:T::Kind::Spaces);break;}}
   else if(frame.task.stage()==S::Complete){actions_[--actionCount_].reset();if(actionCount_){actions_[actionCount_-1]->task.resumeChild(outputByte());select(Component::Action);}else if(actionOwner_==ActionOwner::Parse){parse_->resumeChild();select(Component::Parse);}else if(actionOwner_==ActionOwner::Text){lcdResult_={PanelTextTask::Action::Write,outputByte(),lastWriteReply_,true};text_->resumeChild();select(Component::Text);}else complete_=true;}break;}
  case Component::Poll:poll_->advanceBefore(cost);if(poll_->awaiting()){byteOwner_=ByteOwner::Poll;byte_.emplace(PanelByteTask::Kind::CommandRead);select(Component::Byte);}else if(poll_->stage()==PanelPollTask::Stage::Complete){actions_[actionCount_-1]->task.resumeChild(poll_->reply());poll_.reset();select(Component::Action);}break;
  case Component::Byte:{using T=PanelByteTask;byte_->advanceBefore(cost,[&](T::Read field){return field==T::Read::Input?outputByte():uint8_t(read(0xffff51,cost));},[&](T::Port p,uint8_t value){write(portAddress(p),value,cost);},[&](T::Port p){return portClock(p,previousWrite);});
   if(byte_->reading()&&byte_->stage()==T::Stage::RestoreControl)setByte(byte_->data());if(byte_->stage()==T::Stage::Complete){const auto reply=outputByte();byte_.reset();if(byteOwner_==ByteOwner::Poll){poll_->resumeChild(reply);select(Component::Poll);}else {actions_[actionCount_-1]->task.resumeChild(reply);select(Component::Action);}}break;}
  }
  return cost;
 }
private:
 enum class NumberOwner{Format,Tail};enum class TextOwner{Root,Number,Disabled};enum class ParseOwner{Root,Position,ItemPosition,Character,Text};enum class ActionOwner{Root,Parse,Text};enum class ByteOwner{Poll,Action};
 struct Action {PanelActionTask task;uint8_t heldCount=0;explicit Action(PanelActionTask::Kind kind):task(kind){}};
 static unsigned byteClock(uint32_t a,int previous){a&=0xffffff;return a<0x400000&&!(previous&1)&&a==(uint32_t(previous)|1u)?0:3;}
 static uint32_t portAddress(PanelByteTask::Port p){return p==PanelByteTask::Port::Gate?0xfffeb1:p==PanelByteTask::Port::Control?0xffff62:0xffff61;}
 static unsigned portClock(PanelByteTask::Port p,int previous){const auto a=portAddress(p);return(a&1)&&previous==int(a-1)?0:1;}
 void select(Component component){component_=component;components_|=1u<<unsigned(component);}void setByte(uint8_t v){output_=uint16_t((output_&0xff00)|v);}
 void startNumber(bool signedNumber,NumberOwner owner){lcdResult_={};presentation_=Presentation::Number;numberOwner_=owner;numberValue_=format_->value();number_.emplace(signedNumber?PanelFormatTask::Kind::SignedThree:PanelFormatTask::Kind::ThreeSpaces);select(Component::Number);}
 void startText(TextOwner owner,uint32_t source,PanelTextTask::Kind kind){if(owner==TextOwner::Disabled)lcdResult_={};textOwner_=owner;textSource_=source;text_.emplace(kind);if(kind==PanelTextTask::Kind::Parsed)output_=uint16_t(source);select(Component::Text);}
 void startParse(ParseOwner owner){parseOwner_=owner;parse_.emplace(PanelTextTask::Kind::Parse);select(Component::Parse);}
 void startAction(PanelActionTask::Kind kind){if(actionCount_>=actions_.size())throw std::logic_error("LCD action ownership capacity");actions_[actionCount_++].emplace(kind);select(Component::Action);}
 Kind kind_;Input input_;Component component_=Component::Format;Presentation presentation_=Presentation::Unknown;uint32_t components_=0;bool complete_=false;
 NumberOwner numberOwner_=NumberOwner::Format;TextOwner textOwner_=TextOwner::Root;ParseOwner parseOwner_=ParseOwner::Root;ActionOwner actionOwner_=ActionOwner::Root;ByteOwner byteOwner_=ByteOwner::Poll;
 uint8_t position_=0,width_=0,numberValue_=0,lastWriteReply_=0;uint16_t output_=0,savedPosition_=0,plainSaved_=0;uint32_t cursor_=0,textSource_=0;LcdResult lcdResult_;
 std::optional<StateMenuFormatTask>format_;std::optional<PanelFormatTask>number_;std::optional<PanelDecimalTask>decimal_;std::optional<PanelTextTask>text_,parse_;std::array<std::optional<Action>,4>actions_{};unsigned actionCount_=0;std::optional<PanelPollTask>poll_;std::optional<PanelByteTask>byte_;
};
}
