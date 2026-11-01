#pragma once
#include <cstdint>
namespace ks {
// Firmware-update LoaderProgram policy. Logical data and deferred effects; CPU/stack/flags projection remains in the temporary compatibility bridge.
class LoaderProgramTask final {
public:
 enum class Kind { Image, Pages, Result };enum class Stage { ReadImageSize, IncludeChecksum, CompareImageLimit, RejectImageLimit, BindPageHigh, MovePageHigh, BindPageLow, TestPartialPage, SelectPartialPage, IncludePartialPage, ProgramPages, ReturnImage, BindPageTotal, ClearPageProgress, ClearFailureCount, ProgramPage, TestPageAddress, RejectPageAddress, NextPageProgress, BindProgressByte, MaskProgressByte, SelectProgress, SaveProgressLong, SavePageCount, ShowPageProgress, RestorePageCount, RestoreProgressLong, TickPageCount, RepeatPageCount, ShowProgramResult, ReturnPages, ShowImageError, StopImageError, ReadFailures, SelectFailureNotice, BindSuccessNotice, ShowSuccessNotice, SaveSuccessWord, BindSuccessOuter, SaveSuccessOuter, BindSuccessInner, TickSuccessInner, RepeatSuccessInner, RestoreSuccessOuter, TickSuccessOuter, RepeatSuccessOuter, RestoreSuccessWord, ReturnResult, AwaitChild, Complete };
 enum class Read { Size, Failures };enum class Write { Failures };uint32_t size()const{return size_;}uint16_t failures()const{return failures_;}
 explicit LoaderProgramTask(Kind kind,uint16_t pages=0){remaining_=pages;switch(kind){case Kind::Image:stage_=Stage::ReadImageSize;break;case Kind::Pages:stage_=Stage::BindPageTotal;break;case Kind::Result:stage_=Stage::ReadFailures;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint16_t addressHigh){if(!awaitingChild())return false;invalid_=bool(addressHigh&0x8000);stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ReadImageSize:return 15+store; case Stage::CompareImageLimit:case Stage::BindSuccessNotice:return 9; case Stage::ProgramPages:case Stage::ClearFailureCount:case Stage::ShowProgramResult:case Stage::ReadFailures:return 15; case Stage::ReturnImage:case Stage::ReturnPages:case Stage::ReturnResult:return 17; case Stage::ProgramPage:case Stage::SaveProgressLong:case Stage::ShowPageProgress:case Stage::RestoreProgressLong:case Stage::ShowImageError:case Stage::ShowSuccessNotice:return 20; case Stage::SavePageCount:case Stage::RestorePageCount:case Stage::SaveSuccessWord:case Stage::SaveSuccessOuter:case Stage::RestoreSuccessOuter:case Stage::RestoreSuccessWord:return 11; case Stage::BindSuccessOuter:case Stage::BindSuccessInner:return 6; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadImageSize:size_=uint32_t(read(Read::Size));next();break;
case Stage::IncludeChecksum:size_+=4;next();break;
case Stage::RejectImageLimit:stage_=size_>0x16000u?Stage::ShowImageError:Stage::BindPageHigh;break;
case Stage::BindPageHigh:remaining_=uint16_t(size_>>16);next();break;
case Stage::MovePageHigh:remaining_=uint16_t((uint16_t(uint8_t(remaining_))<<8)|uint8_t(remaining_));next();break;
case Stage::BindPageLow:remaining_=uint16_t((remaining_&0xff00u)|uint8_t(size_>>8));next();break;
case Stage::SelectPartialPage:stage_=(size_&255)?Stage::IncludePartialPage:Stage::ProgramPages;break;
case Stage::IncludePartialPage:++remaining_;next();break;
case Stage::ClearPageProgress:progress_=0;next();break;
case Stage::ClearFailureCount:write(Write::Failures,0);next();break;
case Stage::RejectPageAddress:stage_=invalid_?Stage::ShowImageError:Stage::NextPageProgress;break;
case Stage::NextPageProgress:++progress_;next();break;
case Stage::SelectProgress:stage_=progress_&7?Stage::TickPageCount:Stage::SaveProgressLong;break;
case Stage::TickPageCount:--remaining_;next();break;
case Stage::RepeatPageCount:stage_=remaining_?Stage::ProgramPage:Stage::ShowProgramResult;break;
case Stage::StopImageError:stage_=Stage::StopImageError;break;
case Stage::ReadFailures:failures_=uint16_t(read(Read::Failures));next();break;
case Stage::SelectFailureNotice:stage_=failures_?Stage::ReturnResult:Stage::BindSuccessNotice;break;
case Stage::BindSuccessOuter:outer_=200;next();break;
case Stage::BindSuccessInner:inner_=2663;next();break;
case Stage::TickSuccessInner:--inner_;next();break;
case Stage::RepeatSuccessInner:stage_=inner_?Stage::TickSuccessInner:Stage::RestoreSuccessOuter;break;
case Stage::TickSuccessOuter:--outer_;next();break;
case Stage::RepeatSuccessOuter:stage_=outer_?Stage::SaveSuccessOuter:Stage::RestoreSuccessWord;break;
case Stage::ProgramPages:await(Stage::ReturnImage);break;
case Stage::ReturnImage:stage_=Stage::Complete;break;
case Stage::ProgramPage:await(Stage::TestPageAddress);break;
case Stage::ShowPageProgress:await(Stage::RestorePageCount);break;
case Stage::ShowProgramResult:await(Stage::ReturnPages);break;
case Stage::ReturnPages:stage_=Stage::Complete;break;
case Stage::ShowImageError:await(Stage::StopImageError);break;
case Stage::ShowSuccessNotice:await(Stage::SaveSuccessWord);break;
case Stage::ReturnResult:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t size_=0;uint16_t remaining_=0,progress_=0,failures_=0,outer_=0,inner_=0;bool invalid_=false;
};
}
