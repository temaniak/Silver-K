#pragma once
#include <cstdint>
namespace ks {
// StateProgramBrowse semantic policy: logical data, decisions and deferred effects; compatibility frame stays in Calls.
class StateProgramBrowseTask final {
public:
 enum class Kind { Category, Step, Entry, Clear };enum class Stage { ReadCategoryFilter, SelectCategoryFilter, RetainBrowseDirection, BindBrowseStep, TestBrowseDirection, SelectBrowseDirection, NegateBrowseStep, ReadBrowseCategory, BindBrowseAttempts, StepCategory, BindCategoryPage, ScaleCategoryPage, LocateCategoryPage, ReadPageCategory, ComparePageCategory, AcceptPageCategory, TickBrowseAttempts, RepeatBrowseAttempts, ReturnCategoryBrowse, AddBrowseStep, SelectNegativeBrowse, AddBrowseLimit, CompareBrowseLimit, SelectBrowseLimit, SubtractBrowseLimit, ReturnBrowseStep, TestProgramDelta, SelectProgramDelta, ReadProgramLatch, SelectProgramLatch, ClearProgramLatch, BindLatchedResult, ReturnLatchedResult, BindProgramTotal, BindProgramTarget, ResolveProgramValue, BindProgramDelta, SignProgramDelta, BrowseCategory, ApplyProgramValue, BindProgramChanged, ReturnProgramChanged, ReadProgramDigitLatch, SelectProgramDigitLatch, CompareProgramDigitLatch, SelectProgramSecond, ReadProgramSecond, MaskProgramSecond, BindProgramStride, ScaleProgramSecond, AddProgramThird, WriteProgramNumber, ReadProgramFirst, MaskProgramFirst, WriteProgramBank, ClearProgramDigits, BindEnteredResult, NotifyProgramEntry, ReturnProgramEntry, SaveClearDigits, ClearDigitsValue, WriteDigitsValue, RestoreClearDigits, ReturnClearDigits, TestProgramFirst, RejectProgramZero, BindProgramFirstLimit, CompareProgramFirst, RejectProgramFirst, AddProgramFirstAscii, WriteProgramFirst, BindProgramBlank, WriteProgramSecondBlank, WriteProgramThirdBlank, TickProgramDigits, WriteProgramDigits, BindProgramEntryPosition, ShowProgramEntryPosition, BindProgramEntryHeading, ShowProgramEntryHeading, BindProgramDigits, ReadProgramFirstCharacter, ShowProgramFirstCharacter, ReadProgramSecondCharacter, ShowProgramSecondCharacter, ReadProgramThirdCharacter, ShowProgramThirdCharacter, ClearProgramEntryResult, ReturnProgramEntryResult, ReadProgramFirstLimit, CompareProgramFirstLimit, SelectProgramFirstLimit, CompareProgramSecondLimit, RejectProgramSecond, AddProgramSecondAscii, WriteProgramSecond, JoinProgramSecond, AwaitChild, Complete };
 enum class Read { Filter, Category, PageCategory, Latch, First, Second, Third };enum class Write { Bank, Number, Latch, First, Second, Third };uint32_t filter()const{return filter_;}uint32_t category()const{return category_;}uint32_t observed()const{return observed_;}uint32_t latch()const{return latch_;}uint32_t value()const{return value_;}
 explicit StateProgramBrowseTask(Kind kind,uint16_t request=0,uint16_t value=0,uint16_t limit=0){request_=step_=request;value_=value;limit_=limit;digit_=uint8_t(request);delta_=uint8_t(request>>8);switch(kind){case Kind::Category:stage_=Stage::ReadCategoryFilter;break;case Kind::Step:stage_=Stage::AddBrowseStep;break;case Kind::Entry:stage_=Stage::TestProgramDelta;break;case Kind::Clear:stage_=Stage::SaveClearDigits;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint16_t value){if(!awaitingChild())return false;if(resume_==Stage::BindCategoryPage||resume_==Stage::BindProgramDelta)value_=value;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ReadCategoryFilter:case Stage::ReadBrowseCategory:case Stage::ReadProgramLatch:case Stage::ReadProgramDigitLatch:case Stage::ReadProgramSecond:case Stage::WriteProgramNumber:case Stage::ReadProgramFirst:case Stage::WriteProgramBank:case Stage::WriteDigitsValue:case Stage::WriteProgramFirst:case Stage::WriteProgramSecondBlank:case Stage::WriteProgramThirdBlank:case Stage::WriteProgramDigits:case Stage::ReadProgramFirstLimit:case Stage::WriteProgramSecond:return 9+store; case Stage::BindBrowseStep:case Stage::BindBrowseAttempts:case Stage::BindCategoryPage:case Stage::BindProgramTotal:return 6; case Stage::StepCategory:case Stage::ClearProgramLatch:case Stage::ScaleProgramSecond:case Stage::ClearProgramDigits:return 15; case Stage::ScaleCategoryPage:return 23; case Stage::LocateCategoryPage:case Stage::BindProgramTarget:case Stage::BindProgramEntryHeading:case Stage::BindProgramDigits:return 9; case Stage::ReadPageCategory:return 12+store; case Stage::ReturnCategoryBrowse:case Stage::ReturnBrowseStep:case Stage::ReturnLatchedResult:case Stage::ReturnProgramChanged:case Stage::ReturnProgramEntry:case Stage::ReturnClearDigits:case Stage::ReturnProgramEntryResult:return 17; case Stage::ResolveProgramValue:case Stage::BrowseCategory:case Stage::ApplyProgramValue:case Stage::NotifyProgramEntry:case Stage::ShowProgramEntryPosition:case Stage::ShowProgramEntryHeading:case Stage::ShowProgramFirstCharacter:case Stage::ShowProgramSecondCharacter:case Stage::ShowProgramThirdCharacter:return 20; case Stage::SelectProgramSecond:return 8; case Stage::SaveClearDigits:case Stage::RestoreClearDigits:return 11; case Stage::ReadProgramFirstCharacter:case Stage::ReadProgramSecondCharacter:case Stage::ReadProgramThirdCharacter:return 5+store; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadCategoryFilter:filter_=uint8_t(read(Read::Filter,0));next();break;
case Stage::SelectCategoryFilter:stage_=filter_?Stage::RetainBrowseDirection:Stage::AddBrowseStep;break;
case Stage::BindBrowseStep:step_=1;next();break;
case Stage::SelectBrowseDirection:stage_=int16_t(request_)<0?Stage::NegateBrowseStep:Stage::ReadBrowseCategory;break;
case Stage::NegateBrowseStep:step_=uint16_t(0-step_);next();break;
case Stage::ReadBrowseCategory:category_=uint8_t(read(Read::Category,0));next();break;
case Stage::BindBrowseAttempts:remaining_=600;next();break;
case Stage::ReadPageCategory:observed_=uint8_t(read(Read::PageCategory,0x820000 + uint32_t(value_)*256+176));next();break;
case Stage::AcceptPageCategory:stage_=observed_==category_?Stage::ReturnCategoryBrowse:Stage::TickBrowseAttempts;break;
case Stage::TickBrowseAttempts:--remaining_;next();break;
case Stage::RepeatBrowseAttempts:stage_=remaining_?Stage::StepCategory:Stage::ReturnCategoryBrowse;break;
case Stage::AddBrowseStep:value_=uint16_t(value_+step_);next();break;
case Stage::SelectNegativeBrowse:stage_=value_&0x8000?Stage::AddBrowseLimit:Stage::CompareBrowseLimit;break;
case Stage::AddBrowseLimit:value_=uint16_t(value_+limit_);next();break;
case Stage::SelectBrowseLimit:stage_=value_<limit_?Stage::ReturnBrowseStep:Stage::SubtractBrowseLimit;break;
case Stage::SubtractBrowseLimit:value_=uint16_t(value_-limit_);next();break;
case Stage::SelectProgramDelta:stage_=delta_?Stage::ReadProgramLatch:Stage::ReadProgramDigitLatch;break;
case Stage::ReadProgramLatch:latch_=uint8_t(read(Read::Latch,0));next();break;
case Stage::SelectProgramLatch:stage_=latch_?Stage::ClearProgramLatch:Stage::BindProgramTotal;break;
case Stage::BindProgramTotal:limit_=600;next();break;
case Stage::SignProgramDelta:request_=step_=uint16_t(int16_t(int8_t(delta_)));next();break;
case Stage::ReadProgramDigitLatch:latch_=uint8_t(read(Read::Latch,0));next();break;
case Stage::SelectProgramDigitLatch:stage_=latch_?Stage::CompareProgramDigitLatch:Stage::TestProgramFirst;break;
case Stage::SelectProgramSecond:stage_=latch_==1?Stage::ReadProgramFirstLimit:Stage::ReadProgramSecond;break;
case Stage::ReadProgramSecond:observed_=uint8_t(read(Read::Second,0));next();break;
case Stage::MaskProgramSecond:observed_&=15;next();break;
case Stage::ScaleProgramSecond:observed_=uint8_t(observed_*10);next();break;
case Stage::AddProgramThird:observed_=uint8_t(observed_+digit_);next();break;
case Stage::WriteProgramNumber:write(Write::Number,observed_);next();break;
case Stage::ReadProgramFirst:observed_=uint8_t(read(Read::First,0));next();break;
case Stage::MaskProgramFirst:observed_&=15;next();break;
case Stage::WriteProgramBank:write(Write::Bank,observed_);next();break;
case Stage::WriteDigitsValue:write(Write::Latch,0);next();break;
case Stage::RejectProgramZero:stage_=digit_?Stage::BindProgramFirstLimit:Stage::ClearProgramEntryResult;break;
case Stage::RejectProgramFirst:stage_=digit_>8?Stage::ClearProgramEntryResult:Stage::AddProgramFirstAscii;break;
case Stage::AddProgramFirstAscii:digit_=uint8_t(digit_+48);next();break;
case Stage::WriteProgramFirst:write(Write::First,digit_);next();break;
case Stage::WriteProgramSecondBlank:write(Write::Second,95);next();break;
case Stage::WriteProgramThirdBlank:write(Write::Third,95);next();break;
case Stage::TickProgramDigits:++latch_;next();break;
case Stage::WriteProgramDigits:write(Write::Latch,latch_);next();break;
case Stage::ReadProgramFirstCharacter:observed_=uint8_t(read(Read::First,0));next();break;
case Stage::ReadProgramSecondCharacter:observed_=uint8_t(read(Read::Second,0));next();break;
case Stage::ReadProgramThirdCharacter:observed_=uint8_t(read(Read::Third,0));next();break;
case Stage::ReadProgramFirstLimit:observed_=uint8_t(read(Read::First,0));latch_=observed_;next();break;
case Stage::SelectProgramFirstLimit:stage_=observed_<=52?Stage::AddProgramSecondAscii:Stage::CompareProgramSecondLimit;break;
case Stage::RejectProgramSecond:stage_=digit_>4?Stage::ClearProgramEntryResult:Stage::AddProgramSecondAscii;break;
case Stage::AddProgramSecondAscii:digit_=uint8_t(digit_+48);next();break;
case Stage::WriteProgramSecond:write(Write::Second,digit_);next();break;
case Stage::JoinProgramSecond:stage_=Stage::TickProgramDigits;break;
case Stage::StepCategory:await(Stage::BindCategoryPage);break;
case Stage::ReturnCategoryBrowse:stage_=Stage::Complete;break;
case Stage::ReturnBrowseStep:stage_=Stage::Complete;break;
case Stage::ClearProgramLatch:await(Stage::BindLatchedResult);break;
case Stage::ReturnLatchedResult:stage_=Stage::Complete;break;
case Stage::ResolveProgramValue:await(Stage::BindProgramDelta);break;
case Stage::BrowseCategory:await(Stage::ApplyProgramValue);break;
case Stage::ApplyProgramValue:await(Stage::BindProgramChanged);break;
case Stage::ReturnProgramChanged:stage_=Stage::Complete;break;
case Stage::ClearProgramDigits:await(Stage::BindEnteredResult);break;
case Stage::NotifyProgramEntry:await(Stage::ReturnProgramEntry);break;
case Stage::ReturnProgramEntry:stage_=Stage::Complete;break;
case Stage::ReturnClearDigits:stage_=Stage::Complete;break;
case Stage::ShowProgramEntryPosition:await(Stage::BindProgramEntryHeading);break;
case Stage::ShowProgramEntryHeading:await(Stage::BindProgramDigits);break;
case Stage::ShowProgramFirstCharacter:await(Stage::ReadProgramSecondCharacter);break;
case Stage::ShowProgramSecondCharacter:await(Stage::ReadProgramThirdCharacter);break;
case Stage::ShowProgramThirdCharacter:await(Stage::ClearProgramEntryResult);break;
case Stage::ReturnProgramEntryResult:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint16_t request_=0,step_=0,limit_=0,value_=0,remaining_=0;uint8_t digit_=0,delta_=0,latch_=0,observed_=0,filter_=0,category_=0;
};
}
