#pragma once
#include <cstdint>
namespace ks {
// StateProgramDisplay semantic policy: logical data, decisions and deferred effects; compatibility frame stays in Calls.
class StateProgramDisplayTask final {
public:
 enum class Kind { Program, Heading, Category };enum class Stage { BindProgramPosition, ShowProgramPosition, ReadProgramPage, CompareProgramPage, SelectStoredProgram, SelectMultiProgram, BindMultiName, ShowMultiName, BindNameQuote, ShowNameQuote, ReadNameSlot, AddNameSlot, ShowNameSlot, BindNameEnd, ShowNameEnd, JoinNameEnd, BindMultiNotice, ShowMultiNotice, JoinMultiNotice, ResolveStoredName, ShowStoredName, ReadProgramBank, AddProgramBank, ShowProgramBank, ReadProgramNumber, CompareProgramBank, SelectProgramNumber, CompareFxNumber, SelectProgramNumberKind, BindFxFirst, ShowFxFirst, BindFxSecond, ShowFxSecond, JoinFxNumber, ShowProgramNumber, ClearProgramLatch, WriteProgramLatch, ReturnProgramDisplay, ReadHeadingPage, SelectHeadingPage, ReadHeadingPart, TestHeadingPart, SelectHeadingPart, BindHeadingPart, ShowHeadingPart, ReadHeadingPageAgain, SelectDefaultHeading, BindSingleHeading, ReadHeadingMode, SelectHeadingMode, BindMultiHeading, TickHeadingPage, SelectPerformanceHeading, BindPerformanceHeading, SaveHeadingName, BindHeadingName, ShowHeadingName, RestoreHeadingName, ShowHeadingValue, BindHeadingSpace, ShowHeadingSpace, ReadCategory, BindCategoryLength, BindCategoryWidth, ScaleCategory, NormalizeCategory, LocateCategory, BindCategoryPosition, ShowCategoryPosition, ReadCategoryCharacter, ShowCategoryCharacter, TickCategoryWidth, TickCategoryLength, RepeatCategory, BindCategorySpace, ShowCategorySpace, TickCategoryPadding, RepeatCategoryPadding, ReturnCategory, AwaitChild, Complete };
 enum class Exit { None, Character };Exit exit()const{return exit_;}enum class Read { Page, Slot, Bank, Program, Part, Mode, Category, Character };enum class Write { Latch };uint32_t page()const{return page_;}uint32_t value()const{return value_;}uint32_t program()const{return program_;}uint32_t mode()const{return mode_;}uint32_t category()const{return category_;}
 explicit StateProgramDisplayTask(Kind kind){switch(kind){case Kind::Program:stage_=Stage::BindProgramPosition;break;case Kind::Heading:stage_=Stage::ReadHeadingPage;break;case Kind::Category:stage_=Stage::ReadCategory;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ShowProgramPosition:case Stage::ShowNameQuote:case Stage::ShowNameSlot:case Stage::ShowNameEnd:case Stage::ShowMultiNotice:case Stage::ResolveStoredName:case Stage::ShowProgramBank:case Stage::ShowFxFirst:case Stage::ShowFxSecond:case Stage::ShowProgramNumber:case Stage::SaveHeadingName:case Stage::ShowHeadingName:case Stage::RestoreHeadingName:case Stage::ShowHeadingValue:case Stage::ShowCategoryPosition:case Stage::ShowCategoryCharacter:case Stage::ShowCategorySpace:return 20; case Stage::ReadProgramPage:case Stage::ReadNameSlot:case Stage::ReadProgramBank:case Stage::ReadProgramNumber:case Stage::WriteProgramLatch:case Stage::ReadHeadingPage:case Stage::ReadHeadingPart:case Stage::ReadHeadingPageAgain:case Stage::ReadHeadingMode:case Stage::ReadCategory:return 9+store; case Stage::BindMultiName:case Stage::BindMultiNotice:case Stage::BindSingleHeading:case Stage::BindMultiHeading:case Stage::BindPerformanceHeading:case Stage::LocateCategory:return 9; case Stage::ShowMultiName:case Stage::ShowStoredName:case Stage::ScaleCategory:return 15; case Stage::ReturnProgramDisplay:case Stage::ReturnCategory:return 17; case Stage::ShowHeadingPart:case Stage::ShowHeadingSpace:return 8; case Stage::ReadCategoryCharacter:return 5+store; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadProgramPage:page_=uint8_t(read(Read::Page,0));next();break;
case Stage::SelectStoredProgram:stage_=page_<2?Stage::ResolveStoredName:Stage::SelectMultiProgram;break;
case Stage::SelectMultiProgram:stage_=page_!=2?Stage::BindMultiNotice:Stage::BindMultiName;break;
case Stage::ReadNameSlot:value_=uint8_t(read(Read::Slot,0));next();break;
case Stage::ReadProgramBank:value_=uint8_t(read(Read::Bank,0));next();break;
case Stage::ReadProgramNumber:program_=uint16_t(read(Read::Program,0));next();break;
case Stage::SelectProgramNumber:stage_=uint8_t(program_>>8)<=4?Stage::ShowProgramNumber:Stage::CompareFxNumber;break;
case Stage::SelectProgramNumberKind:stage_=uint8_t(program_)!=49?Stage::ShowProgramNumber:Stage::BindFxFirst;break;
case Stage::WriteProgramLatch:write(Write::Latch,0);next();break;
case Stage::ReadHeadingPage:page_=uint8_t(read(Read::Page,0));next();break;
case Stage::SelectHeadingPage:stage_=page_?Stage::ReadHeadingPageAgain:Stage::ReadHeadingPart;break;
case Stage::ReadHeadingPart:value_=uint8_t(read(Read::Part,0));next();break;
case Stage::SelectHeadingPart:stage_=value_==145?Stage::ReadHeadingPageAgain:Stage::BindHeadingPart;break;
case Stage::ReadHeadingPageAgain:page_=uint8_t(read(Read::Page,0));next();break;
case Stage::SelectDefaultHeading:stage_=page_?Stage::BindSingleHeading:Stage::ShowHeadingValue;break;
case Stage::ReadHeadingMode:mode_=uint8_t(read(Read::Mode,0));next();break;
case Stage::SelectHeadingMode:stage_=mode_?Stage::BindMultiHeading:Stage::TickHeadingPage;break;
case Stage::TickHeadingPage:--page_;next();break;
case Stage::SelectPerformanceHeading:stage_=page_?Stage::BindPerformanceHeading:Stage::SaveHeadingName;break;
case Stage::ReadCategory:category_=uint8_t(read(Read::Category,0));next();break;
case Stage::BindCategoryLength:remaining_=10;next();break;
case Stage::BindCategoryWidth:padding_=20;next();break;
case Stage::LocateCategory:pointer_=0x14f74 + uint32_t(category_)*10;next();break;
case Stage::ReadCategoryCharacter:value_=uint8_t(read(Read::Character,pointer_++));next();break;
case Stage::TickCategoryWidth:--padding_;next();break;
case Stage::TickCategoryLength:--remaining_;next();break;
case Stage::RepeatCategory:stage_=remaining_?Stage::ReadCategoryCharacter:Stage::BindCategorySpace;break;
case Stage::TickCategoryPadding:--padding_;next();break;
case Stage::RepeatCategoryPadding:stage_=padding_?Stage::ShowCategorySpace:Stage::ReturnCategory;break;
case Stage::JoinNameEnd:stage_=Stage::ClearProgramLatch;break;
case Stage::JoinMultiNotice:stage_=Stage::ClearProgramLatch;break;
case Stage::JoinFxNumber:stage_=Stage::ClearProgramLatch;break;
case Stage::ShowHeadingPart:finish(Exit::Character);break;
case Stage::ShowHeadingSpace:finish(Exit::Character);break;
case Stage::ShowProgramPosition:await(Stage::ReadProgramPage);break;
case Stage::ShowMultiName:await(Stage::BindNameQuote);break;
case Stage::ShowNameQuote:await(Stage::ReadNameSlot);break;
case Stage::ShowNameSlot:await(Stage::BindNameEnd);break;
case Stage::ShowNameEnd:await(Stage::JoinNameEnd);break;
case Stage::ShowMultiNotice:await(Stage::JoinMultiNotice);break;
case Stage::ResolveStoredName:await(Stage::ShowStoredName);break;
case Stage::ShowStoredName:await(Stage::ReadProgramBank);break;
case Stage::ShowProgramBank:await(Stage::ReadProgramNumber);break;
case Stage::ShowFxFirst:await(Stage::BindFxSecond);break;
case Stage::ShowFxSecond:await(Stage::JoinFxNumber);break;
case Stage::ShowProgramNumber:await(Stage::ClearProgramLatch);break;
case Stage::ReturnProgramDisplay:stage_=Stage::Complete;break;
case Stage::ShowHeadingName:await(Stage::RestoreHeadingName);break;
case Stage::ShowHeadingValue:await(Stage::BindHeadingSpace);break;
case Stage::ShowCategoryPosition:await(Stage::ReadCategoryCharacter);break;
case Stage::ShowCategoryCharacter:await(Stage::TickCategoryWidth);break;
case Stage::ShowCategorySpace:await(Stage::TickCategoryPadding);break;
case Stage::ReturnCategory:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;void finish(Exit e){exit_=e;stage_=Stage::Complete;}Exit exit_=Exit::None;uint32_t pointer_=0;uint16_t program_=0;uint8_t page_=0,value_=0,mode_=0,category_=0,remaining_=0,padding_=0;
};
}
